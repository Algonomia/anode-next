#pragma once

#include "nodes/NodeExecutor.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace nodes {

using json = nlohmann::json;

/**
 * Serialization/Deserialization for NodeGraph
 *
 * JSON format:
 * {
 *   "nodes": [
 *     {"id": "node_1", "type": "add", "properties": {"_value": {"value": 10, "type": "int"}}}
 *   ],
 *   "connections": [
 *     {"from": "node_1", "fromPort": "value", "to": "node_2", "toPort": "a"}
 *   ]
 * }
 */
class NodeGraphSerializer {
public:
    // === Serialization ===

    /**
     * Convert a NodeGraph to JSON
     */
    static json toJson(const NodeGraph& graph);

    /**
     * Convert a NodeGraph to JSON string
     */
    static std::string toString(const NodeGraph& graph, int indent = 2);

    // === Deserialization ===

    /**
     * Create a NodeGraph from JSON
     */
    static NodeGraph fromJson(const json& j);

    /**
     * Create a NodeGraph from JSON string
     */
    static NodeGraph fromString(const std::string& str);

    // === Helpers (public for result serialization) ===

    static json workloadToJson(const Workload& w);
    static Workload jsonToWorkload(const json& j);

private:

    static json nodeInstanceToJson(const NodeInstance& node);
    static json connectionToJson(const Connection& conn);
    static Connection jsonToConnection(const json& j);
};

} // namespace nodes
