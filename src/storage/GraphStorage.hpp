#pragma once

#include "storage/GraphMetadata.hpp"
#include "nodes/NodeExecutor.hpp"
#include "dataframe/DataFrame.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>

namespace storage {

/**
 * SQLite-based storage for NodeGraphs with versioning support
 *
 * Each graph is identified by a unique slug and can have multiple versions.
 * The graph content is stored as JSON (via NodeGraphSerializer).
 *
 * Usage:
 *   GraphStorage db("./graphs.db");
 *   db.createGraph({.slug = "my-graph", .name = "My Graph", ...});
 *   db.saveVersion("my-graph", graph, "Initial version");
 *   auto loaded = db.loadGraph("my-graph");
 */
class GraphStorage {
public:
    /**
     * Open or create a SQLite database at the given path
     */
    explicit GraphStorage(const std::string& dbPath);
    ~GraphStorage();

    // Non-copyable
    GraphStorage(const GraphStorage&) = delete;
    GraphStorage& operator=(const GraphStorage&) = delete;

    // Movable
    GraphStorage(GraphStorage&&) noexcept;
    GraphStorage& operator=(GraphStorage&&) noexcept;

    // === Graph CRUD ===

    /**
     * Create a new graph with metadata (timestamps are auto-set)
     * Throws if slug already exists
     */
    void createGraph(const GraphMetadata& metadata);

    /**
     * Update graph metadata (updates updated_at timestamp)
     * Throws if graph doesn't exist
     */
    void updateGraph(const GraphMetadata& metadata);

    /**
     * Delete a graph and all its versions
     */
    void deleteGraph(const std::string& slug);

    /**
     * Get graph metadata by slug
     */
    std::optional<GraphMetadata> getGraph(const std::string& slug);

    /**
     * List all graphs ordered by updated_at DESC
     */
    std::vector<GraphMetadata> listGraphs();

    /**
     * Check if a graph exists
     */
    bool graphExists(const std::string& slug);

    // === Version Management ===

    /**
     * Save a new version of a graph
     * Returns the version ID
     * Throws if graph doesn't exist
     */
    int64_t saveVersion(const std::string& slug,
                        const nodes::NodeGraph& graph,
                        const std::optional<std::string>& versionName = std::nullopt);

    /**
     * Get a specific version by ID
     */
    std::optional<GraphVersion> getVersion(int64_t versionId);

    /**
     * Get the latest version of a graph
     */
    std::optional<GraphVersion> getLatestVersion(const std::string& slug);

    /**
     * List all versions of a graph ordered by created_at DESC
     */
    std::vector<GraphVersion> listVersions(const std::string& slug);

    /**
     * Delete a specific version
     */
    void deleteVersion(int64_t versionId);

    // === Convenience Methods ===

    /**
     * Load the latest version of a graph as a NodeGraph
     * Throws if graph doesn't exist or has no versions
     */
    nodes::NodeGraph loadGraph(const std::string& slug);

    /**
     * Load a specific version as a NodeGraph
     * Throws if version doesn't exist
     */
    nodes::NodeGraph loadVersion(int64_t versionId);

    /**
     * Get the database file path
     */
    const std::string& getDbPath() const;

    // === Execution Persistence ===

    /**
     * Save a new execution record
     * Returns the execution ID
     */
    int64_t saveExecution(const std::string& slug,
                          const std::string& sessionId,
                          std::optional<int64_t> versionId,
                          int durationMs,
                          int nodeCount = 0);

    /**
     * Save a DataFrame result from an execution
     * @param outputName Optional name for the output (from "output" node's _name property)
     */
    void saveExecutionDataFrame(int64_t executionId,
                                const std::string& nodeId,
                                const std::string& portName,
                                const dataframe::DataFramePtr& df,
                                const std::string& outputName = "",
                                const std::string& outputType = "",
                                const std::string& metadataJson = "");

    /**
     * Load a specific DataFrame from an execution
     */
    dataframe::DataFramePtr loadExecutionDataFrame(int64_t executionId,
                                                    const std::string& nodeId,
                                                    const std::string& portName);

    /**
     * Load all DataFrames from an execution
     * Returns: Map<nodeId, Map<portName, DataFrame>>
     */
    std::map<std::string, std::map<std::string, dataframe::DataFramePtr>>
        loadExecutionDataFrames(int64_t executionId);

    /**
     * List executions for a graph (ordered by created_at DESC)
     */
    std::vector<ExecutionMetadata> listExecutions(const std::string& slug);

    /**
     * Get execution by session ID (for API compatibility)
     */
    std::optional<ExecutionMetadata> getExecutionBySessionId(const std::string& sessionId);

    /**
     * Get execution by ID
     */
    std::optional<ExecutionMetadata> getExecution(int64_t executionId);

    /**
     * Cleanup old executions, keeping only the N most recent per graph
     */
    void cleanupOldExecutions(const std::string& slug, size_t keepCount = 10);

    /**
     * Get CSV metadata for an execution (for client display)
     * Returns: Map<nodeId, Map<portName, {rows, columns}>>
     */
    std::map<std::string, std::map<std::string, DataFrameMetadata>>
        getExecutionCsvMetadata(int64_t executionId);

    // === Named Outputs ===

    /**
     * Get all named outputs from the latest execution of a graph
     */
    std::vector<NamedOutputInfo> getNamedOutputs(const std::string& slug);

    /**
     * Load a named output DataFrame from the latest execution
     */
    dataframe::DataFramePtr loadNamedOutput(const std::string& slug, const std::string& name);

    /**
     * Get metadata for a specific named output
     */
    std::optional<NamedOutputInfo> getNamedOutputInfo(const std::string& slug, const std::string& name);

    // === Graph Links ===

    /**
     * Replace all outgoing links for a graph (delete + re-insert)
     */
    void replaceGraphLinks(const std::string& sourceSlug, const std::vector<std::string>& targetSlugs);

    /**
     * Get outgoing link targets for a graph
     */
    std::vector<std::string> getOutgoingLinks(const std::string& slug);

    /**
     * Get incoming link sources for a graph
     */
    std::vector<std::string> getIncomingLinks(const std::string& slug);

    // === Parameter Overrides ===

    /**
     * Get all parameter overrides for a graph
     * Returns map of identifier -> value_json string
     */
    std::map<std::string, std::string> getParameterOverrides(const std::string& slug);

    /**
     * Set a parameter override (insert or replace)
     */
    void setParameterOverride(const std::string& slug, const std::string& identifier, const std::string& valueJson);

    /**
     * Delete a single parameter override
     */
    void deleteParameterOverride(const std::string& slug, const std::string& identifier);

    /**
     * Clear all parameter overrides for a graph
     */
    void clearParameterOverrides(const std::string& slug);

    // === Test Scenarios ===

    int64_t createScenario(const std::string& graphSlug, const std::string& name, const std::string& description = "");
    std::optional<ScenarioInfo> getScenario(int64_t scenarioId);
    std::vector<ScenarioInfo> listScenarios(const std::string& graphSlug);
    void updateScenario(int64_t scenarioId, const std::string& name, const std::string& description);
    void updateScenarioRunStatus(int64_t scenarioId, const std::string& status);
    void deleteScenario(int64_t scenarioId);
    void setScenarioInputs(int64_t scenarioId, const std::vector<ScenarioInput>& inputs);
    std::vector<ScenarioInput> getScenarioInputs(int64_t scenarioId);
    void setScenarioExpectedOutputs(int64_t scenarioId, const std::vector<ScenarioExpectedOutput>& outputs);
    std::vector<ScenarioExpectedOutput> getScenarioExpectedOutputs(int64_t scenarioId);
    void setScenarioTriggers(int64_t scenarioId, const std::vector<ScenarioTrigger>& triggers);
    std::vector<ScenarioTrigger> getScenarioTriggers(int64_t scenarioId);
    ScenarioDetails getScenarioDetails(int64_t scenarioId);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace storage
