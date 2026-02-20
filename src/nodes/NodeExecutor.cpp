#include "nodes/NodeExecutor.hpp"
#include "nodes/LabelRegistry.hpp"
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <chrono>

namespace nodes {

// =============================================================================
// NodeGraph Implementation
// =============================================================================

std::string NodeGraph::addNode(const std::string& definitionName) {
    std::string id = "node_" + std::to_string(m_nextId++);
    NodeInstance instance;
    instance.id = id;
    instance.definitionName = definitionName;
    m_nodes[id] = std::move(instance);
    return id;
}

void NodeGraph::removeNode(const std::string& nodeId) {
    m_nodes.erase(nodeId);

    // Remove all connections involving this node
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
            [&nodeId](const Connection& c) {
                return c.sourceNodeId == nodeId || c.targetNodeId == nodeId;
            }),
        m_connections.end()
    );
}

NodeInstance* NodeGraph::getNode(const std::string& nodeId) {
    auto it = m_nodes.find(nodeId);
    return it != m_nodes.end() ? &it->second : nullptr;
}

const NodeInstance* NodeGraph::getNode(const std::string& nodeId) const {
    auto it = m_nodes.find(nodeId);
    return it != m_nodes.end() ? &it->second : nullptr;
}

void NodeGraph::connect(const std::string& sourceNodeId, const std::string& sourcePort,
                        const std::string& targetNodeId, const std::string& targetPort) {
    // Remove existing connection to this target port (if any)
    disconnect(targetNodeId, targetPort);

    Connection conn;
    conn.sourceNodeId = sourceNodeId;
    conn.sourcePortName = sourcePort;
    conn.targetNodeId = targetNodeId;
    conn.targetPortName = targetPort;
    m_connections.push_back(std::move(conn));
}

void NodeGraph::disconnect(const std::string& targetNodeId, const std::string& targetPort) {
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
            [&](const Connection& c) {
                return c.targetNodeId == targetNodeId && c.targetPortName == targetPort;
            }),
        m_connections.end()
    );
}

const Connection* NodeGraph::getConnectionTo(const std::string& targetNodeId,
                                              const std::string& targetPort) const {
    for (const auto& conn : m_connections) {
        if (conn.targetNodeId == targetNodeId && conn.targetPortName == targetPort) {
            return &conn;
        }
    }
    return nullptr;
}

void NodeGraph::setProperty(const std::string& nodeId, const std::string& name, const Workload& value) {
    auto* node = getNode(nodeId);
    if (node) {
        node->properties[name] = value;
    }
}

Workload NodeGraph::getProperty(const std::string& nodeId, const std::string& name) const {
    const auto* node = getNode(nodeId);
    if (node) {
        auto it = node->properties.find(name);
        if (it != node->properties.end()) {
            return it->second;
        }
    }
    return Workload();
}

void NodeGraph::addNodeWithId(const std::string& id, const std::string& definitionName) {
    NodeInstance instance;
    instance.id = id;
    instance.definitionName = definitionName;
    m_nodes[id] = std::move(instance);

    // Update nextId if this ID is numeric and higher than current
    // Try to extract number from "node_X" format
    if (id.rfind("node_", 0) == 0) {
        try {
            uint64_t num = std::stoull(id.substr(5));
            if (num >= m_nextId) {
                m_nextId = num + 1;
            }
        } catch (...) {
            // Non-numeric suffix, ignore
        }
    }
}

// =============================================================================
// NodeExecutor Implementation
// =============================================================================

NodeExecutor::NodeExecutor(const NodeRegistry& registry)
    : m_registry(registry)
{}

void NodeExecutor::setExecutionCallback(ExecutionCallback callback) {
    m_callback = std::move(callback);
}

std::unordered_map<std::string, std::unordered_map<std::string, Workload>>
NodeExecutor::execute(const NodeGraph& graph, const CsvOverrides& csvOverrides) {
    m_results.clear();

    // Clear labels from previous execution
    LabelRegistry::instance().clear();

    // Get execution order
    auto order = topologicalSort(graph);

    // Execute each node
    for (const auto& nodeId : order) {
        const auto* instance = graph.getNode(nodeId);
        if (!instance) continue;

        // Emit "started" event
        if (m_callback) {
            ExecutionEvent evt;
            evt.nodeId = nodeId;
            evt.status = ExecutionStatus::Started;
            m_callback(evt);
        }

        auto startTime = std::chrono::high_resolution_clock::now();

        auto def = m_registry.getNode(instance->definitionName);
        if (!def) {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

            NodeResult result;
            result.nodeId = nodeId;
            result.hasError = true;
            result.errorMessage = "Node definition not found: " + instance->definitionName;
            m_results[nodeId] = std::move(result);

            // Emit "failed" event
            if (m_callback) {
                ExecutionEvent evt;
                evt.nodeId = nodeId;
                evt.status = ExecutionStatus::Failed;
                evt.durationMs = durationMs;
                evt.errorMessage = result.errorMessage;
                m_callback(evt);
            }
            continue;
        }

        // Create context
        NodeContext ctx;

        // Set active CSV if available
        auto activeCsv = findActiveCsv(graph, nodeId);
        if (activeCsv) {
            ctx.setActiveCsv(activeCsv);
        }

        // Gather inputs from connected nodes
        gatherInputs(graph, nodeId, ctx);

        // Add properties as inputs (for widget values)
        // Only if there's no connected input with the same name
        for (const auto& [propName, propValue] : instance->properties) {
            if (!ctx.hasInputEntry(propName)) {
                ctx.setInput(propName, propValue);
            }
        }

        // Check if this node has a DataFrame injected via _identifier (csvOverrides)
        bool injected = false;
        if (!csvOverrides.empty()) {
            auto identIt = instance->properties.find("_identifier");
            if (identIt != instance->properties.end() && !identIt->second.isNull()) {
                std::string ident = identIt->second.getString();
                if (!ident.empty()) {
                    auto ovIt = csvOverrides.find(ident);
                    if (ovIt != csvOverrides.end()) {
                        ctx.setOutput("csv", Workload(ovIt->second));
                        injected = true;
                    }
                }
            }
        }

        // Execute (skip compile if DataFrame was injected)
        if (!injected) {
            def->compile(ctx);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        // Store result
        NodeResult result;
        result.nodeId = nodeId;
        result.hasError = ctx.hasError();
        result.errorMessage = ctx.getErrorMessage();
        for (const auto& [outName, outValue] : ctx.getOutputs()) {
            result.outputs[outName] = outValue;
        }
        m_results[nodeId] = std::move(result);

        // Emit completion event
        if (m_callback) {
            ExecutionEvent evt;
            evt.nodeId = nodeId;
            evt.durationMs = durationMs;

            if (ctx.hasError()) {
                evt.status = ExecutionStatus::Failed;
                evt.errorMessage = ctx.getErrorMessage();
            } else {
                evt.status = ExecutionStatus::Completed;
                // Add CSV metadata for outputs
                for (const auto& [outName, outValue] : ctx.getOutputs()) {
                    if (outValue.getType() == NodeType::Csv) {
                        auto df = outValue.getCsv();
                        if (df) {
                            evt.csvMetadata[outName] = {
                                {"rows", df->rowCount()},
                                {"columns", df->getColumnNames()}
                            };
                        }
                    }
                }
            }
            m_callback(evt);
        }
    }

    // Build return map
    std::unordered_map<std::string, std::unordered_map<std::string, Workload>> outputs;
    for (const auto& [nodeId, result] : m_results) {
        outputs[nodeId] = result.outputs;
    }
    return outputs;
}

NodeContext NodeExecutor::executeNode(const NodeDefinition& definition,
                                       const std::unordered_map<std::string, Workload>& inputs) {
    NodeContext ctx;
    for (const auto& [name, value] : inputs) {
        ctx.setInput(name, value);
    }
    definition.compile(ctx);
    return ctx;
}

const NodeResult* NodeExecutor::getResult(const std::string& nodeId) const {
    auto it = m_results.find(nodeId);
    return it != m_results.end() ? &it->second : nullptr;
}

bool NodeExecutor::hasErrors() const {
    for (const auto& [nodeId, result] : m_results) {
        if (result.hasError) return true;
    }
    return false;
}

std::vector<std::string> NodeExecutor::getErrors() const {
    std::vector<std::string> errors;
    for (const auto& [nodeId, result] : m_results) {
        if (result.hasError) {
            errors.push_back(nodeId + ": " + result.errorMessage);
        }
    }
    return errors;
}

std::vector<std::string> NodeExecutor::topologicalSort(const NodeGraph& graph) const {
    // Build adjacency list and in-degree count
    std::unordered_map<std::string, std::vector<std::string>> dependents;  // node -> nodes that depend on it
    std::unordered_map<std::string, int> inDegree;

    // Initialize
    for (const auto& [nodeId, instance] : graph.getNodes()) {
        inDegree[nodeId] = 0;
    }

    // Count incoming edges (dependencies)
    for (const auto& conn : graph.getConnections()) {
        inDegree[conn.targetNodeId]++;
        dependents[conn.sourceNodeId].push_back(conn.targetNodeId);
    }

    // Add implicit dependencies between label_define_* and label_ref_* with same _label
    // This ensures that ref nodes execute after their corresponding define nodes
    std::unordered_map<std::string, std::string> labelDefines;  // identifier -> nodeId
    std::unordered_map<std::string, std::vector<std::string>> labelRefs;  // identifier -> nodeIds

    for (const auto& [nodeId, instance] : graph.getNodes()) {
        // Check if it's a label_define_* node (handles both "label_define_x" and "label/label_define_x")
        if (instance.definitionName.find("label_define_") != std::string::npos) {
            auto it = instance.properties.find("_label");
            if (it != instance.properties.end() && !it->second.isNull()) {
                std::string identifier = it->second.getString();
                if (!identifier.empty()) {
                    labelDefines[identifier] = nodeId;
                }
            }
        }
        // Check if it's a label_ref_* node (handles both "label_ref_x" and "label/label_ref_x")
        else if (instance.definitionName.find("label_ref_") != std::string::npos) {
            auto it = instance.properties.find("_label");
            if (it != instance.properties.end() && !it->second.isNull()) {
                std::string identifier = it->second.getString();
                if (!identifier.empty()) {
                    labelRefs[identifier].push_back(nodeId);
                }
            }
        }
    }

    // Add implicit edges: define -> ref (for same identifier)
    for (const auto& [identifier, defineNodeId] : labelDefines) {
        auto refIt = labelRefs.find(identifier);
        if (refIt != labelRefs.end()) {
            for (const auto& refNodeId : refIt->second) {
                // Add implicit dependency: ref depends on define
                inDegree[refNodeId]++;
                dependents[defineNodeId].push_back(refNodeId);
            }
        }
    }

    // Start with nodes that have no dependencies (in-degree = 0)
    std::queue<std::string> ready;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            ready.push(nodeId);
        }
    }

    // Process
    std::vector<std::string> order;
    while (!ready.empty()) {
        std::string nodeId = ready.front();
        ready.pop();
        order.push_back(nodeId);

        // Reduce in-degree of dependents
        for (const auto& dependent : dependents[nodeId]) {
            inDegree[dependent]--;
            if (inDegree[dependent] == 0) {
                ready.push(dependent);
            }
        }
    }

    // Check for cycles
    if (order.size() != graph.getNodes().size()) {
        throw std::runtime_error("Cycle detected in node graph");
    }

    return order;
}

void NodeExecutor::gatherInputs(const NodeGraph& graph,
                                const std::string& nodeId,
                                NodeContext& ctx) const {
    // For each connection targeting this node
    for (const auto& conn : graph.getConnections()) {
        if (conn.targetNodeId != nodeId) continue;

        // Get output from source node
        auto it = m_results.find(conn.sourceNodeId);
        if (it == m_results.end()) continue;

        // Check if source is a string_as_fields node → expand into multiple inputs
        const auto* sourceInstance = graph.getNode(conn.sourceNodeId);
        if (sourceInstance &&
            sourceInstance->definitionName.find("string_as_fields") != std::string::npos) {
            // Expand: map value→targetPort, value_N→targetPort_N
            const std::string& basePort = conn.targetPortName;
            for (const auto& [outName, outValue] : it->second.outputs) {
                if (outValue.isNull()) continue;
                std::string targetPort;
                if (outName == "value") {
                    targetPort = basePort;
                } else if (outName.rfind("value_", 0) == 0) {
                    // value_N → basePort_N (e.g., value_1 → field_1)
                    std::string suffix = outName.substr(5); // "_1", "_2", etc.
                    targetPort = basePort + suffix;
                } else {
                    continue;
                }
                ctx.setInput(targetPort, outValue);
            }
            continue;
        }

        auto outIt = it->second.outputs.find(conn.sourcePortName);
        if (outIt == it->second.outputs.end()) continue;

        // Set as input
        ctx.setInput(conn.targetPortName, outIt->second);
    }
}

std::shared_ptr<dataframe::DataFrame> NodeExecutor::findActiveCsv(
    const NodeGraph& graph,
    const std::string& nodeId) const {

    // Look through inputs for a CSV
    for (const auto& conn : graph.getConnections()) {
        if (conn.targetNodeId != nodeId) continue;

        auto it = m_results.find(conn.sourceNodeId);
        if (it == m_results.end()) continue;

        auto outIt = it->second.outputs.find(conn.sourcePortName);
        if (outIt == it->second.outputs.end()) continue;

        if (outIt->second.getType() == NodeType::Csv) {
            return outIt->second.getCsv();
        }
    }

    return nullptr;
}

} // namespace nodes
