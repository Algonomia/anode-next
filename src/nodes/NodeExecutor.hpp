#pragma once

#include "nodes/Types.hpp"
#include "nodes/NodeContext.hpp"
#include "nodes/NodeDefinition.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/ExecutionEvent.hpp"
#include "dataframe/DataFrame.hpp"
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>

namespace nodes {

/**
 * Instance of a node in a graph
 */
struct NodeInstance {
    std::string id;              // Unique instance ID (e.g., "node_1")
    std::string definitionName;  // Reference to NodeDefinition (e.g., "add")
    std::unordered_map<std::string, Workload> properties;  // Widget/property values
    std::optional<std::pair<double, double>> position;     // Optional [x, y] position for UI
};

/**
 * Connection between two nodes
 */
struct Connection {
    std::string sourceNodeId;
    std::string sourcePortName;
    std::string targetNodeId;
    std::string targetPortName;
};

/**
 * Visual group for organizing nodes in the editor.
 * Not used during execution, only for UI purposes.
 */
struct VisualGroup {
    std::string title;
    std::array<double, 4> bounding; // x, y, width, height
    std::string color;
    double fontSize = 24.0;
};

/**
 * A complete node graph
 *
 * Holds node instances and their connections.
 * Used by NodeExecutor to run the graph.
 */
class NodeGraph {
public:
    NodeGraph() = default;

    // === Node Management ===

    /**
     * Add a node instance, returns unique ID
     */
    std::string addNode(const std::string& definitionName);

    /**
     * Remove a node and all its connections
     */
    void removeNode(const std::string& nodeId);

    /**
     * Get a node instance by ID
     */
    NodeInstance* getNode(const std::string& nodeId);
    const NodeInstance* getNode(const std::string& nodeId) const;

    // === Connection Management ===

    /**
     * Connect two nodes
     */
    void connect(const std::string& sourceNodeId, const std::string& sourcePort,
                 const std::string& targetNodeId, const std::string& targetPort);

    /**
     * Disconnect an input
     */
    void disconnect(const std::string& targetNodeId, const std::string& targetPort);

    /**
     * Get the connection for a target input (if any)
     */
    const Connection* getConnectionTo(const std::string& targetNodeId,
                                       const std::string& targetPort) const;

    // === Properties ===

    /**
     * Set a property value on a node (e.g., widget value)
     */
    void setProperty(const std::string& nodeId, const std::string& name, const Workload& value);

    /**
     * Get a property value from a node
     */
    Workload getProperty(const std::string& nodeId, const std::string& name) const;

    // === Getters ===

    const std::unordered_map<std::string, NodeInstance>& getNodes() const { return m_nodes; }
    const std::vector<Connection>& getConnections() const { return m_connections; }
    const std::vector<VisualGroup>& getGroups() const { return m_groups; }
    size_t nodeCount() const { return m_nodes.size(); }

    // === Visual Groups ===

    void addGroup(const VisualGroup& group) { m_groups.push_back(group); }
    void clearGroups() { m_groups.clear(); }

    // === Serialization Support ===

    /**
     * Add a node with a specific ID (for deserialization)
     * Updates nextId counter if needed
     */
    void addNodeWithId(const std::string& id, const std::string& definitionName);

    /**
     * Set the next ID counter (for deserialization)
     */
    void setNextId(uint64_t nextId) { m_nextId = nextId; }

    /**
     * Get the next ID counter
     */
    uint64_t getNextId() const { return m_nextId; }

private:
    std::unordered_map<std::string, NodeInstance> m_nodes;
    std::vector<Connection> m_connections;
    std::vector<VisualGroup> m_groups;
    uint64_t m_nextId = 1;
};

/**
 * Result of executing a single node
 */
struct NodeResult {
    std::string nodeId;
    std::unordered_map<std::string, Workload> outputs;
    bool hasError = false;
    std::string errorMessage;
};

/// Map from _identifier string to a DataFrame to inject into matching csv_source nodes
using CsvOverrides = std::unordered_map<std::string, std::shared_ptr<dataframe::DataFrame>>;

/**
 * Executes a node graph
 *
 * Uses topological sort to determine execution order,
 * then executes each node in sequence, passing outputs
 * from upstream nodes as inputs to downstream nodes.
 */
class NodeExecutor {
public:
    /**
     * Create executor with a registry to look up node definitions
     */
    explicit NodeExecutor(const NodeRegistry& registry);

    /**
     * Set callback for real-time execution events
     * Called for each node as it starts and completes
     */
    void setExecutionCallback(ExecutionCallback callback);

    /**
     * Execute all nodes in the graph
     *
     * @param csvOverrides Optional map of _identifier -> DataFrame to inject into csv_source nodes
     * Returns map: nodeId -> (portName -> Workload)
     */
    std::unordered_map<std::string, std::unordered_map<std::string, Workload>>
    execute(const NodeGraph& graph, const CsvOverrides& csvOverrides = {});

    /**
     * Execute a single node definition (for testing)
     */
    NodeContext executeNode(const NodeDefinition& definition,
                            const std::unordered_map<std::string, Workload>& inputs);

    /**
     * Get results for a specific node after execution
     */
    const NodeResult* getResult(const std::string& nodeId) const;

    /**
     * Check if any node had an error
     */
    bool hasErrors() const;

    /**
     * Get all errors
     */
    std::vector<std::string> getErrors() const;

private:
    const NodeRegistry& m_registry;
    std::unordered_map<std::string, NodeResult> m_results;
    ExecutionCallback m_callback;  // Optional callback for real-time events

    /**
     * Topological sort - returns execution order
     * Entry points first, then nodes in dependency order
     */
    std::vector<std::string> topologicalSort(const NodeGraph& graph) const;

    /**
     * Gather inputs for a node from already-executed nodes
     */
    void gatherInputs(const NodeGraph& graph,
                      const std::string& nodeId,
                      NodeContext& ctx) const;

    /**
     * Find which CSV should be active for a node
     */
    std::shared_ptr<dataframe::DataFrame> findActiveCsv(
        const NodeGraph& graph,
        const std::string& nodeId) const;
};

} // namespace nodes
