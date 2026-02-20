#include "server/RequestHandler.hpp"
#include "server/SessionManager.hpp"
#include "server/Logger.hpp"
#include "server/Profiler.hpp"
#include "dataframe/DataFrameIO.hpp"
#include "dataframe/DataFrameSerializer.hpp"
#include "dataframe/Column.hpp"
#include "nodes/NodeGraphSerializer.hpp"
#include "nodes/NodeExecutor.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/EquationParser.hpp"
#include <unordered_set>
#include <cmath>

namespace dataframe {
namespace server {

namespace {

/**
 * Parse a JSON value into a Workload for input overrides
 */
nodes::Workload parseInputValue(const json& value) {
    if (value.is_number_integer()) {
        return nodes::Workload(value.get<int64_t>(), nodes::NodeType::Int);
    } else if (value.is_number_float()) {
        return nodes::Workload(value.get<double>(), nodes::NodeType::Double);
    } else if (value.is_string()) {
        return nodes::Workload(value.get<std::string>(), nodes::NodeType::String);
    } else if (value.is_boolean()) {
        return nodes::Workload(value.get<bool>());
    }
    return nodes::Workload();  // Null for unsupported types
}

/**
 * Get the expected scalar type for a node definition name, or nullopt if not a scalar node
 */
std::optional<nodes::NodeType> getExpectedScalarType(const std::string& nodeType) {
    if (nodeType == "scalar/int_value") return nodes::NodeType::Int;
    if (nodeType == "scalar/double_value") return nodes::NodeType::Double;
    if (nodeType == "scalar/string_value") return nodes::NodeType::String;
    if (nodeType == "scalar/bool_value") return nodes::NodeType::Bool;
    if (nodeType == "scalar/date_value") return nodes::NodeType::String;  // Date is parsed from string
    if (nodeType == "scalar/string_as_field") return nodes::NodeType::String;
    if (nodeType == "scalar/null_value") return nodes::NodeType::Null;
    if (nodeType == "scalar/current_date") return nodes::NodeType::Int;  // Accepts Int for offsets
    return std::nullopt;
}

/**
 * Convert NodeType to human-readable string for error messages
 */
std::string nodeTypeToErrorString(nodes::NodeType type) {
    switch (type) {
        case nodes::NodeType::Int: return "int";
        case nodes::NodeType::Double: return "double";
        case nodes::NodeType::String: return "string";
        case nodes::NodeType::Bool: return "bool";
        case nodes::NodeType::Null: return "null";
        case nodes::NodeType::Field: return "field";
        case nodes::NodeType::Csv: return "csv";
        default: return "unknown";
    }
}

} // anonymous namespace

RequestHandler& RequestHandler::instance() {
    static RequestHandler instance;
    return instance;
}

void RequestHandler::loadDataset(const std::string& csvPath) {
    LOG_INFO("Loading dataset: " + csvPath);

    ScopedTimer timer("loadDataset");

    m_dataset = DataFrameIO::readCSV(csvPath);
    m_datasetPath = csvPath;
    m_originalRows = m_dataset->rowCount();

    double duration = timer.stop();
    LOG_INFO("Dataset loaded: " + std::to_string(m_originalRows) + " rows in " +
             std::to_string(static_cast<int>(duration)) + "ms");
}

json RequestHandler::handleHealth() {
    return json{
        {"status", "ok"},
        {"service", "AnodeServer"},
        {"version", "1.0.0"},
        {"dataset_loaded", isLoaded()}
    };
}

json RequestHandler::handleDatasetInfo() {
    if (!m_dataset) {
        return json{{"status", "error"}, {"message", "No dataset loaded"}};
    }

    auto columns = m_dataset->getColumnNames();
    json columnsInfo = json::array();

    for (const auto& colName : columns) {
        auto col = m_dataset->getColumn(colName);
        std::string typeStr;
        switch (col->getType()) {
            case ColumnTypeOpt::INT: typeStr = "int"; break;
            case ColumnTypeOpt::DOUBLE: typeStr = "double"; break;
            case ColumnTypeOpt::STRING: typeStr = "string"; break;
        }
        columnsInfo.push_back({
            {"name", colName},
            {"type", typeStr}
        });
    }

    return json{
        {"status", "ok"},
        {"path", m_datasetPath},
        {"rows", m_dataset->rowCount()},
        {"columns", columnsInfo}
    };
}

json RequestHandler::handleQuery(const json& request) {
    ScopedTimer queryTimer("handleQuery");

    if (!m_dataset) {
        LOG_WARN("Query received but no dataset loaded");
        return json{{"status", "error"}, {"message", "No dataset loaded"}};
    }

    // Copie du dataset pour les opérations
    auto result = m_dataset;

    // Flag pour groupbytree (retourne JSON directement, pas un DataFrame)
    bool isGroupByTree = false;
    json treeData;

    // Exécuter le pipeline d'opérations
    if (request.contains("operations") && request["operations"].is_array()) {
        for (const auto& op : request["operations"]) {
            if (!op.contains("type")) continue;

            std::string opType = op["type"];
            json params = op.value("params", json{});

            LOG_DEBUG("Executing operation: " + opType);
            ScopedTimer opTimer("op:" + opType);

            try {
                // Cas spécial : groupbytree retourne du JSON, pas un DataFrame
                if (opType == "groupbytree" || opType == "groupby_tree") {
                    treeData = result->groupByTree(params);
                    isGroupByTree = true;
                    break; // groupbytree doit être la dernière opération
                }

                // Pivot retourne un DataFrame (chaînable avec d'autres opérations)
                if (opType == "pivot") {
                    LOG_INFO("Executing pivotDf with params: " + params.dump());
                    result = result->pivotDf(params);
                    if (!result) {
                        LOG_ERROR("Operation 'pivot' returned null");
                        return json{
                            {"status", "error"},
                            {"message", "Operation 'pivot' returned null"}
                        };
                    }
                    LOG_INFO("PivotDf result: " + std::to_string(result->rowCount()) + " rows, " +
                             std::to_string(result->columnCount()) + " columns");
                    continue;
                }

                result = executeOperation(result, opType, params);
                if (!result) {
                    LOG_ERROR("Operation '" + opType + "' returned null");
                    return json{
                        {"status", "error"},
                        {"message", "Operation '" + opType + "' returned null"}
                    };
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Operation '" + opType + "' failed: " + std::string(e.what()));
                return json{
                    {"status", "error"},
                    {"message", "Operation '" + opType + "' failed: " + e.what()}
                };
            }
        }
    }

    double duration = queryTimer.stop();

    // Cas groupbytree : retourner les données hiérarchiques (format columnar)
    if (isGroupByTree) {
        size_t numGroups = treeData.contains("data") ? treeData["data"].size() : 0;
        LOG_DEBUG("GroupByTree completed: " + std::to_string(numGroups) + " groups in " +
                  std::to_string(static_cast<int>(duration)) + "ms");
        return json{
            {"status", "ok"},
            {"stats", {
                {"input_rows", m_originalRows},
                {"output_rows", numGroups},
                {"groups", numGroups},
                {"duration_ms", static_cast<int>(duration)}
            }},
            {"columns", treeData.value("columns", json::array())},
            {"data", treeData.value("data", json::array())}
        };
    }

    // Pagination: offset et limit
    size_t limit = request.value("limit", 100);
    size_t offset = request.value("offset", 0);
    size_t outputRows = result->rowCount();

    // Construire le JSON en format columnar avec pagination
    auto columns = result->getColumnNames();

    // Calculer la plage de lignes à retourner
    size_t startRow = std::min(offset, outputRows);
    size_t endRow = std::min(offset + limit, outputRows);

    // Format columnar: {"columns": [...], "data": [[...], [...]]}
    json data = json::array();
    for (size_t i = startRow; i < endRow; ++i) {
        json row = json::array();
        for (const auto& colName : columns) {
            auto col = result->getColumn(colName);
            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                row.push_back(intCol->at(i));
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                row.push_back(doubleCol->at(i));
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                row.push_back(stringCol->at(i));
            }
        }
        data.push_back(row);
    }

    LOG_DEBUG("Query completed: " + std::to_string(outputRows) + " rows, returned " +
              std::to_string(data.size()) + " in " + std::to_string(static_cast<int>(duration)) + "ms");

    return json{
        {"status", "ok"},
        {"stats", {
            {"input_rows", m_originalRows},
            {"output_rows", outputRows},
            {"offset", startRow},
            {"returned_rows", data.size()},
            {"duration_ms", static_cast<int>(duration)}
        }},
        {"columns", columns},
        {"data", data}
    };
}

std::shared_ptr<DataFrame> RequestHandler::executeOperation(
    std::shared_ptr<DataFrame> df,
    const std::string& type,
    const json& params)
{
    if (type == "filter") {
        // params est directement le tableau de filtres
        return df->filter(params);
    }
    else if (type == "orderby" || type == "order_by" || type == "sort") {
        // params est directement le tableau d'ordres
        return df->orderBy(params);
    }
    else if (type == "groupby" || type == "group_by") {
        // params contient groupBy et aggregations
        return df->groupBy(params);
    }
    else if (type == "select") {
        // params est un tableau de noms de colonnes
        std::vector<std::string> columns;
        if (params.is_array()) {
            for (const auto& col : params) {
                columns.push_back(col.get<std::string>());
            }
        }
        return df->select(columns);
    }
    else {
        throw std::runtime_error("Unknown operation type: " + type);
    }
}

// === Node Definitions ===

json RequestHandler::handleListNodes() {
    auto& registry = nodes::NodeRegistry::instance();

    json nodeList = json::array();
    for (const auto& name : registry.getNodeNames()) {
        auto def = registry.getNode(name);
        if (!def) continue;

        // Inputs
        json inputs = json::array();
        for (const auto& input : def->getInputs()) {
            json types = json::array();
            for (auto t : input.type.getTypes()) {
                types.push_back(nodes::nodeTypeToString(t));
            }
            inputs.push_back({
                {"name", input.name},
                {"types", types},
                {"required", input.required}
            });
        }

        // Outputs
        json outputs = json::array();
        for (const auto& output : def->getOutputs()) {
            json types = json::array();
            for (auto t : output.type.getTypes()) {
                types.push_back(nodes::nodeTypeToString(t));
            }
            outputs.push_back({
                {"name", output.name},
                {"types", types}
            });
        }

        nodeList.push_back({
            {"name", def->getName()},
            {"category", def->getCategory()},
            {"isEntryPoint", def->isEntryPoint()},
            {"inputs", inputs},
            {"outputs", outputs}
        });
    }

    return json{
        {"status", "ok"},
        {"nodes", nodeList},
        {"categories", registry.getCategories()}
    };
}

// === Graph Storage ===

void RequestHandler::initGraphStorage(const std::string& dbPath) {
    LOG_INFO("Initializing graph storage: " + dbPath);
    m_graphStorage = std::make_unique<storage::GraphStorage>(dbPath);
    LOG_INFO("Graph storage initialized");
}

json RequestHandler::handleListGraphs() {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    auto graphs = m_graphStorage->listGraphs();
    json graphList = json::array();

    for (const auto& g : graphs) {
        auto outgoing = m_graphStorage->getOutgoingLinks(g.slug);
        auto incoming = m_graphStorage->getIncomingLinks(g.slug);
        graphList.push_back({
            {"slug", g.slug},
            {"name", g.name},
            {"description", g.description},
            {"author", g.author},
            {"tags", g.tags},
            {"created_at", g.createdAt},
            {"updated_at", g.updatedAt},
            {"links", {
                {"outgoing", outgoing},
                {"incoming", incoming}
            }}
        });
    }

    return json{
        {"status", "ok"},
        {"graphs", graphList}
    };
}

json RequestHandler::handleGetGraph(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    auto metadata = m_graphStorage->getGraph(slug);
    if (!metadata) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    auto latestVersion = m_graphStorage->getLatestVersion(slug);

    json response = {
        {"status", "ok"},
        {"metadata", {
            {"slug", metadata->slug},
            {"name", metadata->name},
            {"description", metadata->description},
            {"author", metadata->author},
            {"tags", metadata->tags},
            {"created_at", metadata->createdAt},
            {"updated_at", metadata->updatedAt}
        }}
    };

    if (latestVersion) {
        response["version"] = {
            {"id", latestVersion->id},
            {"version_name", latestVersion->versionName.value_or("")},
            {"created_at", latestVersion->createdAt}
        };
        response["graph"] = json::parse(latestVersion->graphJson);
    }

    // Add graph links
    auto outgoing = m_graphStorage->getOutgoingLinks(slug);
    auto incoming = m_graphStorage->getIncomingLinks(slug);
    response["links"] = {
        {"outgoing", outgoing},
        {"incoming", incoming}
    };

    return response;
}

json RequestHandler::handleGetGraphVersions(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    if (!m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    auto versions = m_graphStorage->listVersions(slug);
    json versionList = json::array();

    for (const auto& v : versions) {
        versionList.push_back({
            {"id", v.id},
            {"version_name", v.versionName.value_or("")},
            {"created_at", v.createdAt}
        });
    }

    return json{
        {"status", "ok"},
        {"slug", slug},
        {"versions", versionList}
    };
}

json RequestHandler::handleCreateGraph(const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    // Validate required fields
    if (!request.contains("slug") || !request.contains("name")) {
        return json{{"status", "error"}, {"message", "Missing required fields: slug, name"}};
    }

    std::string slug = request["slug"];
    std::string name = request["name"];

    // Validate slug format (URL-safe: lowercase alphanumeric and hyphens only)
    for (char c : slug) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') {
            return json{{"status", "error"}, {"message", "Invalid slug: only lowercase letters, digits, and hyphens are allowed"}};
        }
    }
    if (slug.empty()) {
        return json{{"status", "error"}, {"message", "Slug cannot be empty"}};
    }

    // Check if graph already exists
    if (m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph already exists: " + slug}};
    }

    // Create metadata
    storage::GraphMetadata metadata;
    metadata.slug = slug;
    metadata.name = name;
    metadata.description = request.value("description", "");
    metadata.author = request.value("author", "");

    if (request.contains("tags") && request["tags"].is_array()) {
        for (const auto& tag : request["tags"]) {
            metadata.tags.push_back(tag.get<std::string>());
        }
    }

    // Create the graph
    m_graphStorage->createGraph(metadata);

    // If graph content is provided, save it as first version
    int64_t versionId = 0;
    if (request.contains("graph")) {
        auto nodeGraph = nodes::NodeGraphSerializer::fromJson(request["graph"]);
        versionId = m_graphStorage->saveVersion(slug, nodeGraph, "Initial version");
        detectAndSaveLinks(slug, nodeGraph);
    }

    return json{
        {"status", "ok"},
        {"slug", slug},
        {"version_id", versionId}
    };
}

json RequestHandler::handleUpdateGraph(const std::string& slug, const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    if (!m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    // Graph content is required
    if (!request.contains("graph")) {
        return json{{"status", "error"}, {"message", "Missing required field: graph"}};
    }

    // Parse and save the new version
    auto nodeGraph = nodes::NodeGraphSerializer::fromJson(request["graph"]);
    std::optional<std::string> versionName;
    if (request.contains("version_name") && !request["version_name"].is_null()) {
        versionName = request["version_name"].get<std::string>();
    }

    int64_t versionId = m_graphStorage->saveVersion(slug, nodeGraph, versionName);
    detectAndSaveLinks(slug, nodeGraph);

    // Return links so client can update badges without re-fetching
    auto outgoing = m_graphStorage->getOutgoingLinks(slug);
    auto incoming = m_graphStorage->getIncomingLinks(slug);

    return json{
        {"status", "ok"},
        {"version_id", versionId},
        {"links", {
            {"outgoing", outgoing},
            {"incoming", incoming}
        }}
    };
}

json RequestHandler::handleDeleteGraph(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    if (!m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    m_graphStorage->deleteGraph(slug);

    return json{
        {"status", "ok"},
        {"message", "Graph deleted: " + slug}
    };
}

json RequestHandler::handleExecuteGraph(const std::string& slug, const json& request,
                                        const nodes::CsvOverrides& csvOverrides) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    ScopedTimer timer("executeGraph");

    // Load the graph
    nodes::NodeGraph graph;
    std::optional<int64_t> versionId;
    try {
        if (request.contains("version_id") && !request["version_id"].is_null()) {
            versionId = request["version_id"].get<int64_t>();
            graph = m_graphStorage->loadVersion(*versionId);
        } else {
            graph = m_graphStorage->loadGraph(slug);
            // Get the latest version ID
            auto latestVersion = m_graphStorage->getLatestVersion(slug);
            if (latestVersion) {
                versionId = latestVersion->id;
            }
        }
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string("Failed to load graph: ") + e.what()}};
    }

    // Parse and apply input overrides
    if (request.contains("inputs") && request["inputs"].is_object()) {
        // Build map identifier -> (nodeId, nodeType) for validation
        std::unordered_map<std::string, std::pair<std::string, std::string>> identifierToNode;

        for (const auto& [nodeId, instance] : graph.getNodes()) {
            auto it = instance.properties.find("_identifier");
            if (it != instance.properties.end() && !it->second.isNull()) {
                std::string identifier = it->second.getString();
                if (!identifier.empty()) {
                    // Check for duplicate identifiers
                    auto existing = identifierToNode.find(identifier);
                    if (existing != identifierToNode.end()) {
                        return json{
                            {"status", "error"},
                            {"error", "Duplicate identifier '" + identifier +
                                      "' in nodes " + existing->second.first + " and " + nodeId}
                        };
                    }
                    identifierToNode[identifier] = {nodeId, instance.definitionName};
                }
            }
        }

        // Apply the overrides with strict type validation
        bool skipUnknown = request.value("skip_unknown_inputs", false);
        for (const auto& [identifier, value] : request["inputs"].items()) {
            auto it = identifierToNode.find(identifier);
            if (it == identifierToNode.end()) {
                if (skipUnknown) continue;
                return json{
                    {"status", "error"},
                    {"error", "Input identifier '" + identifier + "' not found in graph"}
                };
            }

            const auto& [nodeId, nodeType] = it->second;

            // Special handling for string_as_fields: accept JSON array
            if (nodeType == "scalar/string_as_fields") {
                if (value.is_array()) {
                    // Convert ["col_a","col_b"] → store as JSON array string
                    graph.setProperty(nodeId, "_value",
                        nodes::Workload(value.dump(), nodes::NodeType::String));
                } else if (value.is_string()) {
                    // Accept raw JSON array string too
                    graph.setProperty(nodeId, "_value",
                        nodes::Workload(value.get<std::string>(), nodes::NodeType::String));
                } else {
                    if (skipUnknown) continue;
                    return json{
                        {"status", "error"},
                        {"error", "string_as_fields identifier '" + identifier +
                                  "' expects a JSON array of field names"}
                    };
                }
                continue;
            }

            nodes::Workload workload = parseInputValue(value);

            // Strict type validation
            auto expectedType = getExpectedScalarType(nodeType);
            if (expectedType.has_value() && workload.getType() != expectedType.value()) {
                // Allow Int -> Double conversion
                bool allowedConversion = (expectedType.value() == nodes::NodeType::Double
                                          && workload.getType() == nodes::NodeType::Int);
                if (!allowedConversion) {
                    if (skipUnknown) continue;
                    return json{
                        {"status", "error"},
                        {"error", "Type mismatch for identifier '" + identifier +
                                  "': expected " + nodeTypeToErrorString(expectedType.value()) +
                                  ", got " + nodeTypeToErrorString(workload.getType())}
                    };
                }
                // Convert Int to Double
                workload = nodes::Workload(static_cast<double>(workload.getInt()), nodes::NodeType::Double);
            }

            graph.setProperty(nodeId, "_value", workload);
        }
    }

    // Apply parameter overrides from DB (viewer mode)
    nodes::CsvOverrides mergedOverrides = csvOverrides;
    if (request.contains("apply_overrides") && request["apply_overrides"] == true) {
        auto overrides = m_graphStorage->getParameterOverrides(slug);

        // Build identifier -> (nodeId, nodeType) map
        std::unordered_map<std::string, std::pair<std::string, std::string>> identifierToNode;
        for (const auto& [nodeId, instance] : graph.getNodes()) {
            auto it = instance.properties.find("_identifier");
            if (it != instance.properties.end() && !it->second.isNull()) {
                std::string identifier = it->second.getString();
                if (!identifier.empty()) {
                    identifierToNode[identifier] = {nodeId, instance.definitionName};
                }
            }
        }

        for (const auto& [identifier, valueJsonStr] : overrides) {
            auto nodeIt = identifierToNode.find(identifier);
            if (nodeIt == identifierToNode.end()) continue;  // Skip unknown identifiers silently

            const auto& [nodeId, nodeType] = nodeIt->second;
            json valueJson = json::parse(valueJsonStr);

            if (valueJson.contains("type") && valueJson["type"] == "csv") {
                // CSV override → inject via CsvOverrides
                if (valueJson.contains("value")) {
                    auto df = dataframe::DataFrameSerializer::fromJson(valueJson["value"]);
                    if (df) {
                        mergedOverrides[identifier] = df;
                    }
                }
            } else {
                // Scalar override → set _value property on the node
                if (valueJson.contains("value")) {
                    nodes::Workload workload = parseInputValue(valueJson["value"]);
                    graph.setProperty(nodeId, "_value", workload);
                }
            }
        }
    }

    // Execute the graph
    try {
        nodes::NodeExecutor executor(nodes::NodeRegistry::instance());
        auto results = executor.execute(graph, mergedOverrides);

        // Check for node errors
        if (executor.hasErrors()) {
            auto errors = executor.getErrors();
            std::string errorMsg = "Node execution errors: ";
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0) errorMsg += "; ";
                errorMsg += errors[i];
            }
            LOG_ERROR(errorMsg);
            return json{{"status", "error"}, {"message", errorMsg}};
        }

        // Create session and store DataFrames in RAM (cache)
        auto& sessionMgr = SessionManager::instance();
        std::string sessionId = sessionMgr.createSession();

        // Convert results to JSON and store CSV outputs in session
        json resultsJson = json::object();
        json csvMetadata = json::object();
        int nodeCount = static_cast<int>(results.size());

        for (const auto& [nodeId, outputs] : results) {
            json nodeOutputs = json::object();

            for (const auto& [portName, workload] : outputs) {
                nodeOutputs[portName] = nodes::NodeGraphSerializer::workloadToJson(workload);

                // Store DataFrame in session if CSV type
                if (workload.getType() == nodes::NodeType::Csv) {
                    auto df = workload.getCsv();
                    if (df) {
                        sessionMgr.storeDataFrame(sessionId, nodeId, portName, df);

                        // Add metadata for this CSV output
                        if (!csvMetadata.contains(nodeId)) {
                            csvMetadata[nodeId] = json::object();
                        }
                        csvMetadata[nodeId][portName] = {
                            {"rows", df->rowCount()},
                            {"columns", df->getColumnNames()}
                        };
                    }
                }
            }
            resultsJson[nodeId] = nodeOutputs;
        }

        double duration = timer.stop();
        int durationMs = static_cast<int>(duration);

        // Persist execution to SQLite for cross-session access
        int64_t executionId = m_graphStorage->saveExecution(
            slug, sessionId, versionId, durationMs, nodeCount);

        // Persist all DataFrames to SQLite
        for (const auto& [nodeId, outputs] : results) {
            std::string outputName;
            std::string outputType;
            std::string metadataJson;

            auto node = graph.getNode(nodeId);
            if (node) {
                if (node->definitionName == "data/output") {
                    auto it = outputs.find("output_name");
                    if (it != outputs.end() && !it->second.isNull()) {
                        outputName = it->second.getString();
                    }
                }
                else if (node->definitionName == "viz/timeline_output") {
                    auto itName = outputs.find("output_name");
                    if (itName != outputs.end() && !itName->second.isNull()) {
                        outputName = itName->second.getString();
                    }
                    auto itType = outputs.find("output_type");
                    if (itType != outputs.end() && !itType->second.isNull()) {
                        outputType = itType->second.getString();
                    }
                    auto itMeta = outputs.find("output_metadata");
                    if (itMeta != outputs.end() && !itMeta->second.isNull()) {
                        metadataJson = itMeta->second.getString();
                    }
                }
                else if (node->definitionName == "viz/diff_output") {
                    auto itName = outputs.find("output_name");
                    if (itName != outputs.end() && !itName->second.isNull()) {
                        outputName = itName->second.getString();
                    }
                    auto itType = outputs.find("output_type");
                    if (itType != outputs.end() && !itType->second.isNull()) {
                        outputType = itType->second.getString();
                    }
                    auto itMeta = outputs.find("output_metadata");
                    if (itMeta != outputs.end() && !itMeta->second.isNull()) {
                        metadataJson = itMeta->second.getString();
                    }
                }
                else if (node->definitionName == "viz/bar_chart_output") {
                    auto itName = outputs.find("output_name");
                    if (itName != outputs.end() && !itName->second.isNull()) {
                        outputName = itName->second.getString();
                    }
                    auto itType = outputs.find("output_type");
                    if (itType != outputs.end() && !itType->second.isNull()) {
                        outputType = itType->second.getString();
                    }
                    auto itMeta = outputs.find("output_metadata");
                    if (itMeta != outputs.end() && !itMeta->second.isNull()) {
                        metadataJson = itMeta->second.getString();
                    }
                }
            }

            for (const auto& [portName, workload] : outputs) {
                if (workload.getType() == nodes::NodeType::Csv) {
                    auto df = workload.getCsv();
                    if (df) {
                        m_graphStorage->saveExecutionDataFrame(executionId, nodeId, portName, df, outputName, outputType, metadataJson);
                    }
                }
            }
        }

        // Cleanup old executions (keep only 10 most recent)
        m_graphStorage->cleanupOldExecutions(slug, 10);

        return json{
            {"status", "ok"},
            {"session_id", sessionId},
            {"execution_id", executionId},
            {"results", resultsJson},
            {"csv_metadata", csvMetadata},
            {"duration_ms", durationMs}
        };
    } catch (const std::exception& e) {
        LOG_ERROR("Graph execution failed: " + std::string(e.what()));
        return json{{"status", "error"}, {"message", std::string("Execution failed: ") + e.what()}};
    }
}

json RequestHandler::handleExecuteDynamic(const std::string& slug, const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    ScopedTimer timer("executeDynamic");

    // Load the graph (working copy)
    nodes::NodeGraph graph;
    try {
        graph = m_graphStorage->loadGraph(slug);
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string("Failed to load graph: ") + e.what()}};
    }

    // Parse and apply input overrides (same logic as handleExecuteGraph)
    if (request.contains("inputs") && request["inputs"].is_object()) {
        std::unordered_map<std::string, std::pair<std::string, std::string>> identifierToNode;

        for (const auto& [nodeId, instance] : graph.getNodes()) {
            auto it = instance.properties.find("_identifier");
            if (it != instance.properties.end() && !it->second.isNull()) {
                std::string identifier = it->second.getString();
                if (!identifier.empty()) {
                    auto existing = identifierToNode.find(identifier);
                    if (existing != identifierToNode.end()) {
                        return json{
                            {"status", "error"},
                            {"error", "Duplicate identifier '" + identifier +
                                      "' in nodes " + existing->second.first + " and " + nodeId}
                        };
                    }
                    identifierToNode[identifier] = {nodeId, instance.definitionName};
                }
            }
        }

        for (const auto& [identifier, value] : request["inputs"].items()) {
            auto it = identifierToNode.find(identifier);
            if (it == identifierToNode.end()) {
                return json{
                    {"status", "error"},
                    {"error", "Input identifier '" + identifier + "' not found in graph"}
                };
            }

            const auto& [nodeId, nodeType] = it->second;

            // Special handling for string_as_fields: accept JSON array
            if (nodeType == "scalar/string_as_fields") {
                if (value.is_array()) {
                    graph.setProperty(nodeId, "_value",
                        nodes::Workload(value.dump(), nodes::NodeType::String));
                } else if (value.is_string()) {
                    graph.setProperty(nodeId, "_value",
                        nodes::Workload(value.get<std::string>(), nodes::NodeType::String));
                } else {
                    return json{
                        {"status", "error"},
                        {"error", "string_as_fields identifier '" + identifier +
                                  "' expects a JSON array of field names"}
                    };
                }
                continue;
            }

            nodes::Workload workload = parseInputValue(value);

            auto expectedType = getExpectedScalarType(nodeType);
            if (expectedType.has_value() && workload.getType() != expectedType.value()) {
                bool allowedConversion = (expectedType.value() == nodes::NodeType::Double
                                          && workload.getType() == nodes::NodeType::Int);
                if (!allowedConversion) {
                    return json{
                        {"status", "error"},
                        {"error", "Type mismatch for identifier '" + identifier +
                                  "': expected " + nodeTypeToErrorString(expectedType.value()) +
                                  ", got " + nodeTypeToErrorString(workload.getType())}
                    };
                }
                workload = nodes::Workload(static_cast<double>(workload.getInt()), nodes::NodeType::Double);
            }

            graph.setProperty(nodeId, "_value", workload);
        }
    }

    // Validate request
    if (!request.contains("dynamic_nodes") || !request["dynamic_nodes"].is_array()) {
        return json{{"status", "error"}, {"message", "Missing or invalid 'dynamic_nodes' array"}};
    }

    try {
        // For each dynamic_nodes entry
        for (const auto& dyn : request["dynamic_nodes"]) {
            if (!dyn.contains("_name") || !dyn.contains("params")) {
                return json{{"status", "error"}, {"message", "Each dynamic_nodes entry requires '_name' and 'params'"}};
            }

            std::string name = dyn["_name"].get<std::string>();

            // Find dynamic_begin and dynamic_end nodes with this _name
            std::string beginId, endId;
            for (const auto& [nodeId, node] : graph.getNodes()) {
                if (node.definitionName == "dynamic/dynamic_begin") {
                    auto prop = graph.getProperty(nodeId, "_name");
                    if (!prop.isNull() && prop.getString() == name) {
                        beginId = nodeId;
                    }
                }
                if (node.definitionName == "dynamic/dynamic_end") {
                    auto prop = graph.getProperty(nodeId, "_name");
                    if (!prop.isNull() && prop.getString() == name) {
                        endId = nodeId;
                    }
                }
            }

            if (beginId.empty()) {
                return json{{"status", "error"}, {"message", "dynamic_begin with _name='" + name + "' not found"}};
            }
            if (endId.empty()) {
                return json{{"status", "error"}, {"message", "dynamic_end with _name='" + name + "' not found"}};
            }

            // Find the connection from begin to end (on "csv" ports)
            const nodes::Connection* conn = nullptr;
            for (const auto& c : graph.getConnections()) {
                if (c.sourceNodeId == beginId && c.sourcePortName == "csv" &&
                    c.targetNodeId == endId && c.targetPortName == "csv") {
                    conn = &c;
                    break;
                }
            }

            if (!conn) {
                return json{{"status", "error"}, {"message", "No direct connection from dynamic_begin to dynamic_end for _name='" + name + "'"}};
            }

            // Disconnect the end node's csv input
            graph.disconnect(endId, "csv");

            // Process each equation and create math nodes
            std::string lastNodeId = beginId;
            std::string lastPortName = "csv";

            // Track temporary field mappings across equations
            std::unordered_map<std::string, std::string> tempFields;
            int tmpCounter = 0;  // Shared counter across all equations

            for (const auto& eqJson : dyn["params"]) {
                std::string equation = eqJson.get<std::string>();
                auto ops = nodes::parseEquation(equation, &tmpCounter);

                for (const auto& op : ops) {
                    // Create math node
                    std::string mathNodeId = graph.addNode("math/" + op.op);

                    // Connect CSV input from last node
                    graph.connect(lastNodeId, lastPortName, mathNodeId, "csv");

                    // Set src property
                    if (op.srcIsField) {
                        // It's a field reference - use string_as_field node pattern
                        graph.setProperty(mathNodeId, "src", nodes::Workload(op.src, nodes::NodeType::Field));
                    } else if (op.src.substr(0, 5) == "_tmp_") {
                        // It's a temporary from a previous operation
                        // Map to actual field name in tempFields
                        auto it = tempFields.find(op.src);
                        if (it != tempFields.end()) {
                            graph.setProperty(mathNodeId, "src", nodes::Workload(it->second, nodes::NodeType::Field));
                        } else {
                            graph.setProperty(mathNodeId, "src", nodes::Workload(op.src, nodes::NodeType::Field));
                        }
                    } else {
                        // It's a scalar value
                        graph.setProperty(mathNodeId, "src", nodes::Workload(std::stod(op.src), nodes::NodeType::Double));
                    }

                    // Set operand property
                    if (op.operandIsField) {
                        graph.setProperty(mathNodeId, "operand", nodes::Workload(op.operand, nodes::NodeType::Field));
                    } else if (op.operand.substr(0, 5) == "_tmp_") {
                        auto it = tempFields.find(op.operand);
                        if (it != tempFields.end()) {
                            graph.setProperty(mathNodeId, "operand", nodes::Workload(it->second, nodes::NodeType::Field));
                        } else {
                            graph.setProperty(mathNodeId, "operand", nodes::Workload(op.operand, nodes::NodeType::Field));
                        }
                    } else {
                        graph.setProperty(mathNodeId, "operand", nodes::Workload(op.operandValue, nodes::NodeType::Double));
                    }

                    // Set dest property - the destination field name
                    std::string destFieldName = op.dest;
                    if (destFieldName.substr(0, 5) == "_tmp_") {
                        // Keep tmp name for tracking, but it will create a column with this name
                        tempFields[destFieldName] = destFieldName;
                    }
                    graph.setProperty(mathNodeId, "dest", nodes::Workload(destFieldName, nodes::NodeType::Field));

                    lastNodeId = mathNodeId;
                    lastPortName = "csv";
                }
            }

            // Reconnect to end node
            graph.connect(lastNodeId, lastPortName, endId, "csv");
        }

        // Execute the modified graph (same pattern as handleExecuteGraph)
        nodes::NodeExecutor executor(nodes::NodeRegistry::instance());
        auto results = executor.execute(graph);

        // Check for node errors
        if (executor.hasErrors()) {
            auto errors = executor.getErrors();
            std::string errorMsg = "Node execution errors: ";
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0) errorMsg += "; ";
                errorMsg += errors[i];
            }
            LOG_ERROR(errorMsg);
            return json{{"status", "error"}, {"message", errorMsg}};
        }

        // Create session and store DataFrames
        auto& sessionMgr = SessionManager::instance();
        std::string sessionId = sessionMgr.createSession();

        // Convert results to JSON and store CSV outputs
        json resultsJson = json::object();
        json csvMetadata = json::object();
        int nodeCount = static_cast<int>(results.size());

        for (const auto& [nodeId, outputs] : results) {
            json nodeOutputs = json::object();

            for (const auto& [portName, workload] : outputs) {
                nodeOutputs[portName] = nodes::NodeGraphSerializer::workloadToJson(workload);

                if (workload.getType() == nodes::NodeType::Csv) {
                    auto df = workload.getCsv();
                    if (df) {
                        sessionMgr.storeDataFrame(sessionId, nodeId, portName, df);

                        if (!csvMetadata.contains(nodeId)) {
                            csvMetadata[nodeId] = json::object();
                        }
                        csvMetadata[nodeId][portName] = {
                            {"rows", df->rowCount()},
                            {"columns", df->getColumnNames()}
                        };
                    }
                }
            }
            resultsJson[nodeId] = nodeOutputs;
        }

        double duration = timer.stop();
        int durationMs = static_cast<int>(duration);

        // Persist execution to SQLite for cross-session access
        int64_t executionId = m_graphStorage->saveExecution(
            slug, sessionId, std::nullopt, durationMs, nodeCount);

        // Persist all DataFrames to SQLite
        for (const auto& [nodeId, outputs] : results) {
            std::string outputName;
            std::string outputType;
            std::string metadataJson;

            auto node = graph.getNode(nodeId);
            if (node) {
                if (node->definitionName == "data/output") {
                    auto it = outputs.find("output_name");
                    if (it != outputs.end() && !it->second.isNull()) {
                        outputName = it->second.getString();
                    }
                }
                else if (node->definitionName == "viz/timeline_output") {
                    auto itName = outputs.find("output_name");
                    if (itName != outputs.end() && !itName->second.isNull()) {
                        outputName = itName->second.getString();
                    }
                    auto itType = outputs.find("output_type");
                    if (itType != outputs.end() && !itType->second.isNull()) {
                        outputType = itType->second.getString();
                    }
                    auto itMeta = outputs.find("output_metadata");
                    if (itMeta != outputs.end() && !itMeta->second.isNull()) {
                        metadataJson = itMeta->second.getString();
                    }
                }
                else if (node->definitionName == "viz/diff_output") {
                    auto itName = outputs.find("output_name");
                    if (itName != outputs.end() && !itName->second.isNull()) {
                        outputName = itName->second.getString();
                    }
                    auto itType = outputs.find("output_type");
                    if (itType != outputs.end() && !itType->second.isNull()) {
                        outputType = itType->second.getString();
                    }
                    auto itMeta = outputs.find("output_metadata");
                    if (itMeta != outputs.end() && !itMeta->second.isNull()) {
                        metadataJson = itMeta->second.getString();
                    }
                }
                else if (node->definitionName == "viz/bar_chart_output") {
                    auto itName = outputs.find("output_name");
                    if (itName != outputs.end() && !itName->second.isNull()) {
                        outputName = itName->second.getString();
                    }
                    auto itType = outputs.find("output_type");
                    if (itType != outputs.end() && !itType->second.isNull()) {
                        outputType = itType->second.getString();
                    }
                    auto itMeta = outputs.find("output_metadata");
                    if (itMeta != outputs.end() && !itMeta->second.isNull()) {
                        metadataJson = itMeta->second.getString();
                    }
                }
            }

            for (const auto& [portName, workload] : outputs) {
                if (workload.getType() == nodes::NodeType::Csv) {
                    auto df = workload.getCsv();
                    if (df) {
                        m_graphStorage->saveExecutionDataFrame(executionId, nodeId, portName, df, outputName, outputType, metadataJson);
                    }
                }
            }
        }

        // Cleanup old executions (keep only 10 most recent)
        m_graphStorage->cleanupOldExecutions(slug, 10);

        return json{
            {"status", "ok"},
            {"session_id", sessionId},
            {"execution_id", executionId},
            {"results", resultsJson},
            {"csv_metadata", csvMetadata},
            {"duration_ms", durationMs}
        };

    } catch (const std::exception& e) {
        LOG_ERROR("Dynamic execution failed: " + std::string(e.what()));
        return json{{"status", "error"}, {"message", std::string("Dynamic execution failed: ") + e.what()}};
    }
}

json RequestHandler::handleApplyDynamic(const std::string& slug, const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    // Load the graph
    nodes::NodeGraph graph;
    try {
        graph = m_graphStorage->loadGraph(slug);
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string("Failed to load graph: ") + e.what()}};
    }

    // Validate request
    if (!request.contains("dynamic_nodes") || !request["dynamic_nodes"].is_array()) {
        return json{{"status", "error"}, {"message", "Missing or invalid 'dynamic_nodes' array"}};
    }

    try {
        int nodesAdded = 0;
        int nodesRemoved = 0;

        // For each dynamic_nodes entry
        for (const auto& dyn : request["dynamic_nodes"]) {
            if (!dyn.contains("_name") || !dyn.contains("params")) {
                return json{{"status", "error"}, {"message", "Each dynamic_nodes entry requires '_name' and 'params'"}};
            }

            std::string name = dyn["_name"].get<std::string>();

            // Find dynamic_begin and dynamic_end nodes with this _name
            std::string beginId, endId;
            for (const auto& [nodeId, node] : graph.getNodes()) {
                if (node.definitionName == "dynamic/dynamic_begin") {
                    auto prop = graph.getProperty(nodeId, "_name");
                    if (!prop.isNull() && prop.getString() == name) {
                        beginId = nodeId;
                    }
                }
                if (node.definitionName == "dynamic/dynamic_end") {
                    auto prop = graph.getProperty(nodeId, "_name");
                    if (!prop.isNull() && prop.getString() == name) {
                        endId = nodeId;
                    }
                }
            }

            if (beginId.empty()) {
                return json{{"status", "error"}, {"message", "dynamic_begin with _name='" + name + "' not found"}};
            }
            if (endId.empty()) {
                return json{{"status", "error"}, {"message", "dynamic_end with _name='" + name + "' not found"}};
            }

            // Find and remove all intermediate nodes between begin and end
            // Follow CSV chain from begin toward end, collecting nodes to delete
            std::vector<std::string> nodesToDelete;
            std::unordered_set<std::string> visited;
            std::string currentId = beginId;

            while (!currentId.empty() && currentId != endId) {
                if (visited.count(currentId)) break;  // Cycle detection
                visited.insert(currentId);

                // Find next node connected via CSV output
                std::string nextId;
                for (const auto& c : graph.getConnections()) {
                    if (c.sourceNodeId == currentId && c.sourcePortName == "csv") {
                        nextId = c.targetNodeId;
                        break;
                    }
                }

                if (nextId.empty()) break;

                // If it's not the end node, mark for deletion
                if (nextId != endId) {
                    nodesToDelete.push_back(nextId);
                }

                currentId = nextId;
            }

            // Also find and delete all scalar input nodes connected to the nodes we're deleting
            std::vector<std::string> inputNodesToDelete;
            for (const auto& nodeId : nodesToDelete) {
                for (const auto& c : graph.getConnections()) {
                    if (c.targetNodeId == nodeId &&
                        (c.targetPortName == "src" || c.targetPortName == "operand" || c.targetPortName == "dest")) {
                        // Check if it's a scalar node (not connected to anything else important)
                        auto srcNode = graph.getNode(c.sourceNodeId);
                        if (srcNode && (srcNode->definitionName == "scalar/string_as_field" ||
                                        srcNode->definitionName == "scalar/double_value")) {
                            inputNodesToDelete.push_back(c.sourceNodeId);
                        }
                    }
                }
            }

            // Delete intermediate nodes (this also removes their connections)
            for (const auto& nodeId : inputNodesToDelete) {
                graph.removeNode(nodeId);
                nodesRemoved++;
            }
            for (const auto& nodeId : nodesToDelete) {
                graph.removeNode(nodeId);
                nodesRemoved++;
            }

            // Disconnect the end node's csv input (may already be disconnected after node removal)
            graph.disconnect(endId, "csv");

            // Get position of begin node for layout
            auto beginNode = graph.getNode(beginId);
            double startX = 0, startY = 0;
            if (beginNode && beginNode->position) {
                startX = beginNode->position->first;
                startY = beginNode->position->second;
            }

            // Process each equation and create math nodes
            std::string lastNodeId = beginId;
            std::string lastPortName = "csv";

            // Track temporary field mappings across equations
            std::unordered_map<std::string, std::string> tempFields;
            int nodeIndex = 0;
            int tmpCounter = 0;  // Shared counter across all equations

            for (const auto& eqJson : dyn["params"]) {
                std::string equation = eqJson.get<std::string>();
                auto ops = nodes::parseEquation(equation, &tmpCounter);

                for (const auto& op : ops) {
                    double mathNodeX = startX + 650 + nodeIndex * 600;
                    double mathNodeY = startY;

                    // Create math node
                    std::string mathNodeId = graph.addNode("math/" + op.op);
                    nodesAdded++;
                    auto mathNode = graph.getNode(mathNodeId);
                    if (mathNode) {
                        mathNode->position = std::make_pair(mathNodeX, mathNodeY);
                    }

                    // Connect CSV input from last node
                    graph.connect(lastNodeId, lastPortName, mathNodeId, "csv");

                    // === Create and connect SRC input node ===
                    std::string srcFieldName;
                    if (op.srcIsField) {
                        srcFieldName = op.src;
                    } else if (op.src.size() >= 5 && op.src.substr(0, 5) == "_tmp_") {
                        auto it = tempFields.find(op.src);
                        srcFieldName = (it != tempFields.end()) ? it->second : op.src;
                    }

                    if (!srcFieldName.empty()) {
                        // Create string_as_field node for src
                        std::string srcNodeId = graph.addNode("scalar/string_as_field");
                        nodesAdded++;
                        auto srcNode = graph.getNode(srcNodeId);
                        if (srcNode) {
                            srcNode->position = std::make_pair(mathNodeX - 280, mathNodeY - 120);
                        }
                        graph.setProperty(srcNodeId, "_value", nodes::Workload(srcFieldName, nodes::NodeType::String));
                        graph.connect(srcNodeId, "value", mathNodeId, "src");
                    } else {
                        // Create double_value node for scalar src
                        std::string srcNodeId = graph.addNode("scalar/double_value");
                        nodesAdded++;
                        auto srcNode = graph.getNode(srcNodeId);
                        if (srcNode) {
                            srcNode->position = std::make_pair(mathNodeX - 280, mathNodeY - 120);
                        }
                        graph.setProperty(srcNodeId, "_value", nodes::Workload(std::stod(op.src), nodes::NodeType::Double));
                        graph.connect(srcNodeId, "value", mathNodeId, "src");
                    }

                    // === Create and connect OPERAND input node ===
                    std::string operandFieldName;
                    if (op.operandIsField) {
                        operandFieldName = op.operand;
                    } else if (op.operand.size() >= 5 && op.operand.substr(0, 5) == "_tmp_") {
                        auto it = tempFields.find(op.operand);
                        operandFieldName = (it != tempFields.end()) ? it->second : op.operand;
                    }

                    if (!operandFieldName.empty()) {
                        // Create string_as_field node for operand
                        std::string operandNodeId = graph.addNode("scalar/string_as_field");
                        nodesAdded++;
                        auto operandNode = graph.getNode(operandNodeId);
                        if (operandNode) {
                            operandNode->position = std::make_pair(mathNodeX - 280, mathNodeY + 20);
                        }
                        graph.setProperty(operandNodeId, "_value", nodes::Workload(operandFieldName, nodes::NodeType::String));
                        graph.connect(operandNodeId, "value", mathNodeId, "operand");
                    } else {
                        // Create double_value node for scalar operand
                        std::string operandNodeId = graph.addNode("scalar/double_value");
                        nodesAdded++;
                        auto operandNode = graph.getNode(operandNodeId);
                        if (operandNode) {
                            operandNode->position = std::make_pair(mathNodeX - 280, mathNodeY + 20);
                        }
                        graph.setProperty(operandNodeId, "_value", nodes::Workload(op.operandValue, nodes::NodeType::Double));
                        graph.connect(operandNodeId, "value", mathNodeId, "operand");
                    }

                    // === Create and connect DEST input node ===
                    std::string destFieldName = op.dest;
                    if (destFieldName.size() >= 5 && destFieldName.substr(0, 5) == "_tmp_") {
                        tempFields[destFieldName] = destFieldName;
                    }

                    std::string destNodeId = graph.addNode("scalar/string_as_field");
                    nodesAdded++;
                    auto destNode = graph.getNode(destNodeId);
                    if (destNode) {
                        destNode->position = std::make_pair(mathNodeX - 280, mathNodeY + 160);
                    }
                    graph.setProperty(destNodeId, "_value", nodes::Workload(destFieldName, nodes::NodeType::String));
                    graph.connect(destNodeId, "value", mathNodeId, "dest");

                    lastNodeId = mathNodeId;
                    lastPortName = "csv";
                    nodeIndex++;
                }
            }

            // Reconnect to end node
            graph.connect(lastNodeId, lastPortName, endId, "csv");
        }

        // Save the modified graph as a new version
        std::string versionName = "Applied dynamic equations";
        if (request.contains("version_name") && !request["version_name"].is_null()) {
            versionName = request["version_name"].get<std::string>();
        }

        int64_t versionId = m_graphStorage->saveVersion(slug, graph, versionName);

        LOG_INFO("Applied dynamic equations to graph '" + slug + "': " +
                 std::to_string(nodesRemoved) + " nodes removed, " +
                 std::to_string(nodesAdded) + " nodes added, version " + std::to_string(versionId));

        return json{
            {"status", "ok"},
            {"version_id", versionId},
            {"nodes_added", nodesAdded},
            {"nodes_removed", nodesRemoved},
            {"message", "Graph updated: " + std::to_string(nodesRemoved) + " nodes removed, " +
                        std::to_string(nodesAdded) + " new nodes added"}
        };

    } catch (const std::exception& e) {
        LOG_ERROR("Apply dynamic failed: " + std::string(e.what()));
        return json{{"status", "error"}, {"message", std::string("Apply dynamic failed: ") + e.what()}};
    }
}

json RequestHandler::handleGetDynamicEquations(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    // Load the graph
    nodes::NodeGraph graph;
    try {
        graph = m_graphStorage->loadGraph(slug);
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string("Failed to load graph: ") + e.what()}};
    }

    // Find all dynamic zones (pairs of dynamic_begin/dynamic_end with matching _name)
    std::unordered_map<std::string, std::string> beginNodes;  // _name -> nodeId
    std::unordered_map<std::string, std::string> endNodes;    // _name -> nodeId

    for (const auto& [nodeId, node] : graph.getNodes()) {
        if (node.definitionName == "dynamic/dynamic_begin") {
            auto prop = graph.getProperty(nodeId, "_name");
            if (!prop.isNull() && prop.getType() == nodes::NodeType::String) {
                beginNodes[prop.getString()] = nodeId;
            }
        }
        if (node.definitionName == "dynamic/dynamic_end") {
            auto prop = graph.getProperty(nodeId, "_name");
            if (!prop.isNull() && prop.getType() == nodes::NodeType::String) {
                endNodes[prop.getString()] = nodeId;
            }
        }
    }

    // Build response with equations for each zone
    json zones = json::array();

    for (const auto& [name, beginId] : beginNodes) {
        auto endIt = endNodes.find(name);
        if (endIt == endNodes.end()) continue;  // No matching end node

        const std::string& endId = endIt->second;

        // Extract MathOps between begin and end
        auto mathOps = nodes::extractMathOps(graph, beginId, endId);

        // Reconstruct equations from MathOps
        auto equations = nodes::reconstructEquations(mathOps);

        zones.push_back({
            {"_name", name},
            {"equations", equations}
        });
    }

    return json{
        {"status", "ok"},
        {"zones", zones}
    };
}

json RequestHandler::handleSessionDataFrame(const std::string& sessionId,
                                            const std::string& nodeId,
                                            const std::string& portName,
                                            const json& request) {
    ScopedTimer queryTimer("handleSessionDataFrame");

    auto& sessionMgr = SessionManager::instance();
    auto df = sessionMgr.getDataFrame(sessionId, nodeId, portName);

    // Fallback to SQLite if not in session cache
    if (!df && m_graphStorage) {
        auto execution = m_graphStorage->getExecutionBySessionId(sessionId);
        if (execution) {
            df = m_graphStorage->loadExecutionDataFrame(execution->id, nodeId, portName);
            // Re-cache in session for faster subsequent access
            if (df) {
                sessionMgr.storeDataFrame(sessionId, nodeId, portName, df);
                LOG_DEBUG("Loaded DataFrame from SQLite: session=" + sessionId +
                          ", node=" + nodeId + ", port=" + portName);
            }
        }
    }

    if (!df) {
        return json{
            {"status", "error"},
            {"message", "DataFrame not found for session=" + sessionId +
                        ", node=" + nodeId + ", port=" + portName}
        };
    }

    auto result = df;

    // Apply operations (reuse existing pattern from handleQuery)
    if (request.contains("operations") && request["operations"].is_array()) {
        for (const auto& op : request["operations"]) {
            if (!op.contains("type")) continue;

            std::string opType = op["type"];
            json params = op.value("params", json{});

            try {
                result = executeOperation(result, opType, params);
                if (!result) {
                    return json{
                        {"status", "error"},
                        {"message", "Operation '" + opType + "' returned null"}
                    };
                }
            } catch (const std::exception& e) {
                return json{
                    {"status", "error"},
                    {"message", "Operation '" + opType + "' failed: " + e.what()}
                };
            }
        }
    }

    // Pagination
    size_t limit = request.value("limit", 100);
    size_t offset = request.value("offset", 0);
    size_t totalRows = result->rowCount();

    // Build columnar response
    auto columns = result->getColumnNames();
    size_t startRow = std::min(offset, totalRows);
    size_t endRow = std::min(offset + limit, totalRows);

    json data = json::array();
    for (size_t i = startRow; i < endRow; ++i) {
        json row = json::array();
        for (const auto& colName : columns) {
            auto col = result->getColumn(colName);
            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                row.push_back(intCol->at(i));
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                row.push_back(doubleCol->at(i));
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                row.push_back(stringCol->at(i));
            }
        }
        data.push_back(row);
    }

    double duration = queryTimer.stop();

    return json{
        {"status", "ok"},
        {"stats", {
            {"total_rows", totalRows},
            {"offset", startRow},
            {"returned_rows", data.size()},
            {"duration_ms", static_cast<int>(duration)}
        }},
        {"columns", columns},
        {"data", data}
    };
}

json RequestHandler::handleListExecutions(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    if (!m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    auto executions = m_graphStorage->listExecutions(slug);
    json executionList = json::array();

    for (const auto& exec : executions) {
        executionList.push_back({
            {"id", exec.id},
            {"session_id", exec.sessionId},
            {"version_id", exec.versionId.value_or(0)},
            {"created_at", exec.createdAt},
            {"duration_ms", exec.durationMs},
            {"node_count", exec.nodeCount},
            {"dataframe_count", exec.dataframeCount}
        });
    }

    return json{
        {"status", "ok"},
        {"slug", slug},
        {"executions", executionList}
    };
}

json RequestHandler::handleGetExecution(int64_t executionId) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    auto execution = m_graphStorage->getExecution(executionId);
    if (!execution) {
        return json{{"status", "error"}, {"message", "Execution not found: " + std::to_string(executionId)}};
    }

    // Get CSV metadata for this execution
    auto csvMetadataMap = m_graphStorage->getExecutionCsvMetadata(executionId);
    json csvMetadata = json::object();

    for (const auto& [nodeId, ports] : csvMetadataMap) {
        csvMetadata[nodeId] = json::object();
        for (const auto& [portName, meta] : ports) {
            json columns = json::array();
            for (const auto& col : meta.schema) {
                columns.push_back(col.name);
            }
            csvMetadata[nodeId][portName] = {
                {"rows", meta.rowCount},
                {"columns", columns}
            };
        }
    }

    return json{
        {"status", "ok"},
        {"execution", {
            {"id", execution->id},
            {"graph_slug", execution->graphSlug},
            {"session_id", execution->sessionId},
            {"version_id", execution->versionId.value_or(0)},
            {"created_at", execution->createdAt},
            {"duration_ms", execution->durationMs},
            {"node_count", execution->nodeCount},
            {"dataframe_count", execution->dataframeCount}
        }},
        {"csv_metadata", csvMetadata}
    };
}

json RequestHandler::handleRestoreExecution(int64_t executionId) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    auto execution = m_graphStorage->getExecution(executionId);
    if (!execution) {
        return json{{"status", "error"}, {"message", "Execution not found: " + std::to_string(executionId)}};
    }

    // Load all DataFrames from the execution and store them in session cache
    auto& sessionMgr = SessionManager::instance();
    auto dataframes = m_graphStorage->loadExecutionDataFrames(executionId);

    // Restore to the original session ID
    std::string sessionId = execution->sessionId;

    for (const auto& [nodeId, ports] : dataframes) {
        for (const auto& [portName, df] : ports) {
            if (df) {
                sessionMgr.storeDataFrame(sessionId, nodeId, portName, df);
            }
        }
    }

    // Get CSV metadata for client display
    auto csvMetadataMap = m_graphStorage->getExecutionCsvMetadata(executionId);
    json csvMetadata = json::object();

    for (const auto& [nodeId, ports] : csvMetadataMap) {
        csvMetadata[nodeId] = json::object();
        for (const auto& [portName, meta] : ports) {
            json columns = json::array();
            for (const auto& col : meta.schema) {
                columns.push_back(col.name);
            }
            csvMetadata[nodeId][portName] = {
                {"rows", meta.rowCount},
                {"columns", columns}
            };
        }
    }

    LOG_INFO("Restored execution " + std::to_string(executionId) +
             " to session " + sessionId + " with " +
             std::to_string(dataframes.size()) + " nodes");

    return json{
        {"status", "ok"},
        {"session_id", sessionId},
        {"execution_id", executionId},
        {"csv_metadata", csvMetadata}
    };
}

json RequestHandler::handleListOutputs(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    if (!m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    auto outputs = m_graphStorage->getNamedOutputs(slug);
    json outputList = json::array();

    for (const auto& output : outputs) {
        json item = {
            {"name", output.name},
            {"node_id", output.nodeId},
            {"rows", output.rowCount},
            {"columns", output.columns},
            {"execution_id", output.executionId},
            {"created_at", output.createdAt}
        };
        if (!output.outputType.empty()) {
            item["type"] = output.outputType;
        }
        if (!output.metadataJson.empty()) {
            item["metadata"] = json::parse(output.metadataJson);
        }
        outputList.push_back(item);
    }

    return json{
        {"status", "ok"},
        {"outputs", outputList}
    };
}

json RequestHandler::handleGetOutput(const std::string& slug, const std::string& name, const json& request) {
    ScopedTimer queryTimer("handleGetOutput");

    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    if (!m_graphStorage->graphExists(slug)) {
        return json{{"status", "error"}, {"message", "Graph not found: " + slug}};
    }

    // Get metadata first
    auto info = m_graphStorage->getNamedOutputInfo(slug, name);
    if (!info) {
        return json{{"status", "error"}, {"message", "Output not found: " + name}};
    }

    // Load the DataFrame
    auto df = m_graphStorage->loadNamedOutput(slug, name);
    if (!df) {
        return json{{"status", "error"}, {"message", "Failed to load output: " + name}};
    }

    auto result = df;

    // Apply operations (reuse existing pattern)
    if (request.contains("operations") && request["operations"].is_array()) {
        for (const auto& op : request["operations"]) {
            if (!op.contains("type")) continue;

            std::string opType = op["type"];
            json params = op.value("params", json{});

            try {
                result = executeOperation(result, opType, params);
                if (!result) {
                    return json{
                        {"status", "error"},
                        {"message", "Operation '" + opType + "' returned null"}
                    };
                }
            } catch (const std::exception& e) {
                return json{
                    {"status", "error"},
                    {"message", "Operation '" + opType + "' failed: " + e.what()}
                };
            }
        }
    }

    // Pagination
    size_t limit = request.value("limit", 100);
    size_t offset = request.value("offset", 0);
    size_t totalRows = result->rowCount();

    // Build columnar response
    auto columns = result->getColumnNames();
    size_t startRow = std::min(offset, totalRows);
    size_t endRow = std::min(offset + limit, totalRows);

    json data = json::array();
    for (size_t i = startRow; i < endRow; ++i) {
        json row = json::array();
        for (const auto& colName : columns) {
            auto col = result->getColumn(colName);
            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                row.push_back(intCol->at(i));
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                row.push_back(doubleCol->at(i));
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                row.push_back(stringCol->at(i));
            }
        }
        data.push_back(row);
    }

    double duration = queryTimer.stop();

    return json{
        {"status", "ok"},
        {"output", {
            {"name", info->name},
            {"node_id", info->nodeId},
            {"execution_id", info->executionId},
            {"created_at", info->createdAt}
        }},
        {"stats", {
            {"total_rows", totalRows},
            {"offset", startRow},
            {"returned_rows", data.size()},
            {"duration_ms", static_cast<int>(duration)}
        }},
        {"columns", columns},
        {"data", data}
    };
}

// =============================================================================
// Parameter Overrides
// =============================================================================

json RequestHandler::handleGetParameters(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        auto overrides = m_graphStorage->getParameterOverrides(slug);
        json params = json::object();
        for (const auto& [identifier, valueJson] : overrides) {
            params[identifier] = json::parse(valueJson);
        }
        return json{{"status", "ok"}, {"parameters", params}};
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", e.what()}};
    }
}

json RequestHandler::handleSetParameter(const std::string& slug, const std::string& identifier, const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        m_graphStorage->setParameterOverride(slug, identifier, request.dump());
        return json{{"status", "ok"}};
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", e.what()}};
    }
}

json RequestHandler::handleDeleteParameter(const std::string& slug, const std::string& identifier) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        m_graphStorage->deleteParameterOverride(slug, identifier);
        return json{{"status", "ok"}};
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", e.what()}};
    }
}

// =============================================================================
// Plugin Route Extension
// =============================================================================

void RequestHandler::registerRouteHandler(RouteHandler handler) {
    m_pluginRouteHandlers.push_back(std::move(handler));
}

std::optional<RouteResult> RequestHandler::tryPluginRoutes(
    const std::string& method, const std::string& target,
    const json& body) const
{
    for (const auto& handler : m_pluginRouteHandlers) {
        auto result = handler(method, target, body);
        if (result) return result;
    }
    return std::nullopt;
}

void RequestHandler::registerRequestValidator(RequestValidator validator) {
    m_requestValidators.push_back(std::move(validator));
}

std::optional<RouteResult> RequestHandler::validateRequest(
    const std::string& method, const std::string& target,
    const std::map<std::string, std::string>& cookies) const
{
    for (const auto& validator : m_requestValidators) {
        auto result = validator(method, target, cookies);
        if (result) return result;
    }
    return std::nullopt;
}

// =============================================================================
// Test Scenarios
// =============================================================================

json RequestHandler::handleListScenarios(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    auto scenarios = m_graphStorage->listScenarios(slug);
    json list = json::array();
    for (const auto& s : scenarios) {
        list.push_back({
            {"id", s.id},
            {"name", s.name},
            {"description", s.description},
            {"last_run_at", s.lastRunAt},
            {"last_run_status", s.lastRunStatus},
            {"created_at", s.createdAt},
            {"updated_at", s.updatedAt}
        });
    }
    return json{{"status", "ok"}, {"scenarios", list}};
}

json RequestHandler::handleCreateScenario(const std::string& slug, const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }
    if (!request.contains("name") || !request["name"].is_string()) {
        return json{{"status", "error"}, {"message", "Missing required field: name"}};
    }

    std::string name = request["name"].get<std::string>();
    std::string description = request.value("description", std::string(""));

    try {
        int64_t id = m_graphStorage->createScenario(slug, name, description);
        return json{{"status", "ok"}, {"id", id}};
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string(e.what())}};
    }
}

json RequestHandler::handleGetScenario(const std::string& /*slug*/, int64_t scenarioId) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        auto details = m_graphStorage->getScenarioDetails(scenarioId);

        json inputsJson = json::array();
        for (const auto& inp : details.inputs) {
            inputsJson.push_back({
                {"identifier", inp.identifier},
                {"value_json", inp.valueJson}
            });
        }

        json expectedJson = json::array();
        for (const auto& exp : details.expectedOutputs) {
            expectedJson.push_back({
                {"output_name", exp.outputName},
                {"expected_json", exp.expectedJson}
            });
        }

        json triggersJson = json::array();
        for (const auto& trig : details.triggers) {
            triggersJson.push_back({
                {"trigger_type", trig.triggerType},
                {"identifier", trig.identifier},
                {"data_json", trig.dataJson}
            });
        }

        return json{
            {"status", "ok"},
            {"scenario", {
                {"id", details.info.id},
                {"name", details.info.name},
                {"description", details.info.description},
                {"last_run_at", details.info.lastRunAt},
                {"last_run_status", details.info.lastRunStatus},
                {"created_at", details.info.createdAt},
                {"updated_at", details.info.updatedAt}
            }},
            {"inputs", inputsJson},
            {"expected_outputs", expectedJson},
            {"triggers", triggersJson}
        };
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string(e.what())}};
    }
}

json RequestHandler::handleUpdateScenario(const std::string& /*slug*/, int64_t scenarioId, const json& request) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        auto existing = m_graphStorage->getScenario(scenarioId);
        if (!existing) {
            return json{{"status", "error"}, {"message", "Scenario not found"}};
        }

        std::string name = request.value("name", existing->name);
        std::string description = request.value("description", existing->description);
        m_graphStorage->updateScenario(scenarioId, name, description);

        // Update inputs if provided
        if (request.contains("inputs") && request["inputs"].is_array()) {
            std::vector<storage::ScenarioInput> inputs;
            for (const auto& inp : request["inputs"]) {
                storage::ScenarioInput si;
                si.scenarioId = scenarioId;
                si.identifier = inp["identifier"].get<std::string>();
                si.valueJson = inp["value_json"].get<std::string>();
                inputs.push_back(std::move(si));
            }
            m_graphStorage->setScenarioInputs(scenarioId, inputs);
        }

        // Update expected outputs if provided
        if (request.contains("expected_outputs") && request["expected_outputs"].is_array()) {
            std::vector<storage::ScenarioExpectedOutput> outputs;
            for (const auto& exp : request["expected_outputs"]) {
                storage::ScenarioExpectedOutput seo;
                seo.scenarioId = scenarioId;
                seo.outputName = exp["output_name"].get<std::string>();
                seo.expectedJson = exp["expected_json"].get<std::string>();
                outputs.push_back(std::move(seo));
            }
            m_graphStorage->setScenarioExpectedOutputs(scenarioId, outputs);
        }

        // Update triggers if provided
        if (request.contains("triggers") && request["triggers"].is_array()) {
            std::vector<storage::ScenarioTrigger> triggers;
            for (const auto& trig : request["triggers"]) {
                storage::ScenarioTrigger st;
                st.scenarioId = scenarioId;
                st.triggerType = trig["trigger_type"].get<std::string>();
                st.identifier = trig["identifier"].get<std::string>();
                st.dataJson = trig["data_json"].get<std::string>();
                triggers.push_back(std::move(st));
            }
            m_graphStorage->setScenarioTriggers(scenarioId, triggers);
        }

        return json{{"status", "ok"}, {"message", "Scenario updated"}};
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string(e.what())}};
    }
}

json RequestHandler::handleDeleteScenario(int64_t scenarioId) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        m_graphStorage->deleteScenario(scenarioId);
        return json{{"status", "ok"}, {"message", "Scenario deleted"}};
    } catch (const std::exception& e) {
        return json{{"status", "error"}, {"message", std::string(e.what())}};
    }
}

json RequestHandler::handleRunScenario(const std::string& slug, int64_t scenarioId) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    try {
        auto details = m_graphStorage->getScenarioDetails(scenarioId);

        // 1. Build inputs JSON from scenario inputs
        json inputsObj = json::object();
        for (const auto& inp : details.inputs) {
            inputsObj[inp.identifier] = json::parse(inp.valueJson);
        }

        // 2. Build CsvOverrides from scenario triggers
        nodes::CsvOverrides csvOverrides;
        for (const auto& trig : details.triggers) {
            json dfJson = json::parse(trig.dataJson);
            auto df = dataframe::DataFrameSerializer::fromJson(dfJson);
            if (df) {
                csvOverrides[trig.identifier] = df;
            }
        }

        // 3. Execute the graph with overrides
        json execRequest = json::object();
        if (!inputsObj.empty()) {
            execRequest["inputs"] = inputsObj;
        }

        json execResult = handleExecuteGraph(slug, execRequest, csvOverrides);
        if (execResult.value("status", "") != "ok") {
            m_graphStorage->updateScenarioRunStatus(scenarioId, "fail");
            return json{
                {"status", "ok"},
                {"run_status", "fail"},
                {"error", execResult.value("message", "Execution failed")},
                {"outputs", json::array()}
            };
        }

        // 4. Compare expected outputs with actuals
        bool allPass = true;
        json outputResults = json::array();

        for (const auto& expected : details.expectedOutputs) {
            json outputResult = {{"output_name", expected.outputName}};

            auto actualDf = m_graphStorage->loadNamedOutput(slug, expected.outputName);
            if (!actualDf) {
                outputResult["match"] = false;
                outputResult["error"] = "Output not found: " + expected.outputName;
                allPass = false;
                outputResults.push_back(outputResult);
                continue;
            }

            // Parse expected DataFrame
            json expectedDfJson = json::parse(expected.expectedJson);
            auto expectedDf = dataframe::DataFrameSerializer::fromJson(expectedDfJson);
            if (!expectedDf) {
                outputResult["match"] = false;
                outputResult["error"] = "Failed to parse expected data";
                allPass = false;
                outputResults.push_back(outputResult);
                continue;
            }

            // Compare DataFrames
            json mismatches = json::array();

            // Compare columns
            auto actualCols = actualDf->getColumnNames();
            auto expectedCols = expectedDf->getColumnNames();

            std::unordered_set<std::string> actualColSet(actualCols.begin(), actualCols.end());
            std::unordered_set<std::string> expectedColSet(expectedCols.begin(), expectedCols.end());

            for (const auto& col : expectedCols) {
                if (actualColSet.find(col) == actualColSet.end()) {
                    mismatches.push_back({{"type", "missing_column"}, {"column", col}});
                }
            }
            for (const auto& col : actualCols) {
                if (expectedColSet.find(col) == expectedColSet.end()) {
                    mismatches.push_back({{"type", "extra_column"}, {"column", col}});
                }
            }

            // Compare row counts
            if (actualDf->rowCount() != expectedDf->rowCount()) {
                mismatches.push_back({
                    {"type", "row_count"},
                    {"expected", expectedDf->rowCount()},
                    {"actual", actualDf->rowCount()}
                });
            }

            // Cell comparison (use common columns, up to min rows)
            size_t maxRows = std::min(actualDf->rowCount(), expectedDf->rowCount());
            int mismatchCount = 0;
            const int maxMismatches = 50;

            for (const auto& colName : expectedCols) {
                if (actualColSet.find(colName) == actualColSet.end()) continue;
                if (mismatchCount >= maxMismatches) break;

                auto actualCol = actualDf->getColumn(colName);
                auto expectedCol = expectedDf->getColumn(colName);
                if (!actualCol || !expectedCol) continue;

                for (size_t row = 0; row < maxRows && mismatchCount < maxMismatches; ++row) {
                    bool match = false;

                    auto actInt = std::dynamic_pointer_cast<IntColumn>(actualCol);
                    auto expInt = std::dynamic_pointer_cast<IntColumn>(expectedCol);
                    auto actDbl = std::dynamic_pointer_cast<DoubleColumn>(actualCol);
                    auto expDbl = std::dynamic_pointer_cast<DoubleColumn>(expectedCol);
                    auto actStr = std::dynamic_pointer_cast<StringColumn>(actualCol);
                    auto expStr = std::dynamic_pointer_cast<StringColumn>(expectedCol);

                    if (actInt && expInt) {
                        match = (actInt->at(row) == expInt->at(row));
                    } else if (actDbl && expDbl) {
                        match = (std::abs(actDbl->at(row) - expDbl->at(row)) < 1e-9);
                    } else if (actStr && expStr) {
                        match = (actStr->at(row) == expStr->at(row));
                    } else {
                        // Type mismatch between columns
                        match = false;
                    }

                    if (!match) {
                        json mismatch = {
                            {"type", "cell"},
                            {"row", row},
                            {"column", colName}
                        };
                        // Add values to mismatch
                        if (expInt) mismatch["expected"] = expInt->at(row);
                        else if (expDbl) mismatch["expected"] = expDbl->at(row);
                        else if (expStr) mismatch["expected"] = expStr->at(row);

                        if (actInt) mismatch["actual"] = actInt->at(row);
                        else if (actDbl) mismatch["actual"] = actDbl->at(row);
                        else if (actStr) mismatch["actual"] = actStr->at(row);

                        mismatches.push_back(mismatch);
                        mismatchCount++;
                    }
                }
            }

            bool outputMatch = mismatches.empty();
            if (!outputMatch) allPass = false;

            outputResult["match"] = outputMatch;
            outputResult["mismatches"] = mismatches;
            outputResults.push_back(outputResult);
        }

        // 5. Update run status
        std::string runStatus = allPass ? "pass" : "fail";
        m_graphStorage->updateScenarioRunStatus(scenarioId, runStatus);

        return json{
            {"status", "ok"},
            {"run_status", runStatus},
            {"outputs", outputResults},
            {"execution_id", execResult.value("execution_id", 0)},
            {"duration_ms", execResult.value("duration_ms", 0)}
        };
    } catch (const std::exception& e) {
        try { m_graphStorage->updateScenarioRunStatus(scenarioId, "fail"); } catch (...) {}
        return json{{"status", "error"}, {"message", std::string(e.what())}};
    }
}

json RequestHandler::handleRunAllScenarios(const std::string& slug) {
    if (!m_graphStorage) {
        return json{{"status", "error"}, {"message", "Graph storage not initialized"}};
    }

    auto scenarios = m_graphStorage->listScenarios(slug);
    json results = json::array();
    int passCount = 0, failCount = 0;

    for (const auto& scenario : scenarios) {
        json result = handleRunScenario(slug, scenario.id);
        std::string runStatus = result.value("run_status", "fail");
        if (runStatus == "pass") passCount++;
        else failCount++;

        results.push_back({
            {"id", scenario.id},
            {"name", scenario.name},
            {"run_status", runStatus},
            {"outputs", result.value("outputs", json::array())}
        });
    }

    return json{
        {"status", "ok"},
        {"results", results},
        {"summary", {
            {"total", scenarios.size()},
            {"pass", passCount},
            {"fail", failCount}
        }}
    };
}

void RequestHandler::detectAndSaveLinks(const std::string& slug, const nodes::NodeGraph& nodeGraph) {
    if (!m_graphStorage) return;

    std::vector<std::string> targetSlugs;

    // Find all timeline_output nodes
    for (const auto& [nodeId, node] : nodeGraph.getNodes()) {
        if (node.definitionName != "viz/timeline_output") continue;

        // Find connection to this node's "event" port
        for (const auto& conn : nodeGraph.getConnections()) {
            if (conn.targetNodeId != nodeId || conn.targetPortName != "event") continue;

            // Get the source node
            const auto* sourceNode = nodeGraph.getNode(conn.sourceNodeId);
            if (!sourceNode || sourceNode->definitionName != "scalar/string_value") continue;

            // Read _value property
            auto it = sourceNode->properties.find("_value");
            if (it == sourceNode->properties.end()) continue;

            try {
                std::string targetSlug = it->second.getString();
                if (!targetSlug.empty()) {
                    targetSlugs.push_back(targetSlug);
                }
            } catch (...) {
                // Property is not a string type, skip
            }
        }
    }

    m_graphStorage->replaceGraphLinks(slug, targetSlugs);
}

} // namespace server
} // namespace dataframe
