#include "nodes/NodeGraphSerializer.hpp"
#include "dataframe/DataFrameSerializer.hpp"
#include <stdexcept>

namespace nodes {

// =============================================================================
// Serialization
// =============================================================================

json NodeGraphSerializer::toJson(const NodeGraph& graph) {
    json result;

    // Serialize nodes
    json nodesArray = json::array();
    for (const auto& [nodeId, instance] : graph.getNodes()) {
        nodesArray.push_back(nodeInstanceToJson(instance));
    }
    result["nodes"] = nodesArray;

    // Serialize connections
    json connectionsArray = json::array();
    for (const auto& conn : graph.getConnections()) {
        connectionsArray.push_back(connectionToJson(conn));
    }
    result["connections"] = connectionsArray;

    // Serialize groups (visual only)
    if (!graph.getGroups().empty()) {
        json groupsArray = json::array();
        for (const auto& group : graph.getGroups()) {
            json g;
            g["title"] = group.title;
            g["bounding"] = json::array({
                group.bounding[0], group.bounding[1],
                group.bounding[2], group.bounding[3]
            });
            if (!group.color.empty()) {
                g["color"] = group.color;
            }
            if (group.fontSize != 24.0) {
                g["font_size"] = group.fontSize;
            }
            groupsArray.push_back(g);
        }
        result["groups"] = groupsArray;
    }

    return result;
}

std::string NodeGraphSerializer::toString(const NodeGraph& graph, int indent) {
    return toJson(graph).dump(indent);
}

// =============================================================================
// Deserialization
// =============================================================================

NodeGraph NodeGraphSerializer::fromJson(const json& j) {
    NodeGraph graph;

    // Deserialize nodes
    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& nodeJson : j["nodes"]) {
            if (!nodeJson.contains("id") || !nodeJson.contains("type")) {
                throw std::runtime_error("Invalid node: missing 'id' or 'type'");
            }

            std::string id = nodeJson["id"].get<std::string>();
            std::string type = nodeJson["type"].get<std::string>();

            graph.addNodeWithId(id, type);

            // Deserialize properties
            if (nodeJson.contains("properties") && nodeJson["properties"].is_object()) {
                for (const auto& [propName, propValue] : nodeJson["properties"].items()) {
                    graph.setProperty(id, propName, jsonToWorkload(propValue));
                }
            }

            // Deserialize position (optional)
            if (nodeJson.contains("position") && nodeJson["position"].is_array() &&
                nodeJson["position"].size() >= 2) {
                auto* node = graph.getNode(id);
                if (node) {
                    double x = nodeJson["position"][0].get<double>();
                    double y = nodeJson["position"][1].get<double>();
                    node->position = std::make_pair(x, y);
                }
            }
        }
    }

    // Deserialize connections
    if (j.contains("connections") && j["connections"].is_array()) {
        for (const auto& connJson : j["connections"]) {
            Connection conn = jsonToConnection(connJson);
            graph.connect(conn.sourceNodeId, conn.sourcePortName,
                         conn.targetNodeId, conn.targetPortName);
        }
    }

    // Deserialize groups (visual only)
    if (j.contains("groups") && j["groups"].is_array()) {
        for (const auto& groupJson : j["groups"]) {
            VisualGroup group;
            group.title = groupJson.value("title", "Group");
            if (groupJson.contains("bounding") && groupJson["bounding"].is_array() &&
                groupJson["bounding"].size() >= 4) {
                group.bounding = {
                    groupJson["bounding"][0].get<double>(),
                    groupJson["bounding"][1].get<double>(),
                    groupJson["bounding"][2].get<double>(),
                    groupJson["bounding"][3].get<double>()
                };
            }
            group.color = groupJson.value("color", "");
            group.fontSize = groupJson.value("font_size", 24.0);
            graph.addGroup(group);
        }
    }

    return graph;
}

NodeGraph NodeGraphSerializer::fromString(const std::string& str) {
    return fromJson(json::parse(str));
}

// =============================================================================
// Helpers - Workload Serialization
// =============================================================================

json NodeGraphSerializer::workloadToJson(const Workload& w) {
    json result;
    result["type"] = nodeTypeToString(w.getType());

    switch (w.getType()) {
        case NodeType::Int:
            result["value"] = w.getInt();
            break;
        case NodeType::Double:
            result["value"] = w.getDouble();
            break;
        case NodeType::String:
        case NodeType::Field:
            result["value"] = w.getString();
            break;
        case NodeType::Bool:
            result["value"] = w.getBool();
            break;
        case NodeType::Null:
            result["value"] = nullptr;
            break;
        case NodeType::Csv: {
            auto df = w.getCsv();
            if (df && df->rowCount() > 0) {
                auto columnOrder = df->getColumnNames();
                result["value"] = dataframe::DataFrameSerializer::toJsonWithSchema(
                    df->rowCount(),
                    columnOrder,
                    [&df](const std::string& name) { return df->getColumn(name); }
                );
            }
            break;
        }
    }

    return result;
}

Workload NodeGraphSerializer::jsonToWorkload(const json& j) {
    if (!j.contains("type")) {
        throw std::runtime_error("Invalid workload: missing 'type'");
    }

    std::string typeStr = j["type"].get<std::string>();
    NodeType type = stringToNodeType(typeStr);

    // Auto-detect CSV: if value is an object with "columns" and "data",
    // treat as CSV regardless of declared type (the editor may store it as "string")
    if (j.contains("value") && j["value"].is_object() &&
        j["value"].contains("columns") && j["value"].contains("data")) {
        auto df = dataframe::DataFrameSerializer::fromJson(j["value"]);
        if (df) {
            return Workload(df);
        }
    }

    switch (type) {
        case NodeType::Int:
            if (j.contains("value") && !j["value"].is_null()) {
                return Workload(j["value"].get<int64_t>(), NodeType::Int);
            }
            return Workload(int64_t(0), NodeType::Int);

        case NodeType::Double:
            if (j.contains("value") && !j["value"].is_null()) {
                return Workload(j["value"].get<double>(), NodeType::Double);
            }
            return Workload(0.0, NodeType::Double);

        case NodeType::String:
            if (j.contains("value") && !j["value"].is_null()) {
                return Workload(j["value"].get<std::string>(), NodeType::String);
            }
            return Workload(std::string(""), NodeType::String);

        case NodeType::Field:
            if (j.contains("value") && !j["value"].is_null()) {
                return Workload(j["value"].get<std::string>(), NodeType::Field);
            }
            return Workload(std::string(""), NodeType::Field);

        case NodeType::Bool:
            if (j.contains("value") && !j["value"].is_null()) {
                return Workload(j["value"].get<bool>());
            }
            return Workload(false);

        case NodeType::Csv:
            if (j.contains("value") && j["value"].is_object()) {
                auto df = dataframe::DataFrameSerializer::fromJson(j["value"]);
                if (df) {
                    return Workload(df);
                }
            }
            return Workload();

        case NodeType::Null:
        default:
            return Workload();
    }
}

// =============================================================================
// Helpers - Node/Connection Serialization
// =============================================================================

json NodeGraphSerializer::nodeInstanceToJson(const NodeInstance& node) {
    json result;
    result["id"] = node.id;
    result["type"] = node.definitionName;

    // Serialize properties
    json props = json::object();
    for (const auto& [propName, propValue] : node.properties) {
        props[propName] = workloadToJson(propValue);
    }
    result["properties"] = props;

    // Serialize position if present
    if (node.position.has_value()) {
        result["position"] = json::array({node.position->first, node.position->second});
    }

    return result;
}

json NodeGraphSerializer::connectionToJson(const Connection& conn) {
    json result;
    result["from"] = conn.sourceNodeId;
    result["fromPort"] = conn.sourcePortName;
    result["to"] = conn.targetNodeId;
    result["toPort"] = conn.targetPortName;
    return result;
}

Connection NodeGraphSerializer::jsonToConnection(const json& j) {
    Connection conn;

    if (!j.contains("from") || !j.contains("fromPort") ||
        !j.contains("to") || !j.contains("toPort")) {
        throw std::runtime_error("Invalid connection: missing required fields");
    }

    conn.sourceNodeId = j["from"].get<std::string>();
    conn.sourcePortName = j["fromPort"].get<std::string>();
    conn.targetNodeId = j["to"].get<std::string>();
    conn.targetPortName = j["toPort"].get<std::string>();

    return conn;
}

} // namespace nodes
