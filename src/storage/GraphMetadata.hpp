#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace storage {

/**
 * Metadata for a saved graph
 */
struct GraphMetadata {
    std::string slug;                    // Unique identifier (URL-friendly)
    std::string name;                    // Display name
    std::string description;             // Optional description
    std::string author;                  // Optional author name
    std::vector<std::string> tags;       // Optional tags for categorization
    std::string createdAt;               // ISO 8601 timestamp
    std::string updatedAt;               // ISO 8601 timestamp
};

/**
 * A specific version of a graph
 */
struct GraphVersion {
    int64_t id;                          // Auto-incremented version ID
    std::string graphSlug;               // Reference to parent graph
    std::optional<std::string> versionName;  // Optional name (e.g., "v1.0", "before refactor")
    std::string graphJson;               // Serialized NodeGraph JSON
    std::string createdAt;               // ISO 8601 timestamp
};

/**
 * Graph metadata with its latest version (convenience struct)
 */
struct GraphWithLatestVersion {
    GraphMetadata metadata;
    std::optional<GraphVersion> latestVersion;
};

/**
 * Metadata for a graph execution
 */
struct ExecutionMetadata {
    int64_t id;                          // Auto-incremented execution ID
    std::string graphSlug;               // Reference to parent graph
    std::optional<int64_t> versionId;    // Version of the graph executed (optional)
    std::string sessionId;               // Session ID for API compatibility
    std::string createdAt;               // ISO 8601 timestamp
    int durationMs;                      // Execution duration in milliseconds
    int nodeCount;                       // Number of nodes in the graph
    int dataframeCount;                  // Number of DataFrames stored
};

/**
 * Schema for a DataFrame column
 */
struct ColumnSchema {
    std::string name;
    std::string type;  // "INT", "DOUBLE", "STRING"
};

/**
 * Metadata for a stored DataFrame
 */
struct DataFrameMetadata {
    int64_t id;                          // Auto-incremented ID
    int64_t executionId;                 // Reference to parent execution
    std::string nodeId;                  // Node that produced this DataFrame
    std::string portName;                // Output port name
    size_t rowCount;                     // Number of rows
    std::vector<ColumnSchema> schema;    // Column definitions
};

/**
 * Information about a named output (from an "output" node)
 */
struct NamedOutputInfo {
    std::string name;                    // Output name (from _name property)
    std::string nodeId;                  // Node ID of the output node
    size_t rowCount;                     // Number of rows in the DataFrame
    std::vector<std::string> columns;    // Column names
    int64_t executionId;                 // Execution that produced this output
    std::string createdAt;               // When the execution was run
    std::string outputType;              // "timeline", or "" for regular data/output
    std::string metadataJson;            // JSON with field mappings (for timeline, etc.)
};

/**
 * Test scenario info (summary for list views)
 */
struct ScenarioInfo {
    int64_t id = 0;
    std::string graphSlug;
    std::string name;
    std::string description;
    std::string lastRunAt;       // empty if never run
    std::string lastRunStatus;   // "pass", "fail", or empty
    std::string createdAt;
    std::string updatedAt;
};

/**
 * A parameter override for a scenario
 */
struct ScenarioInput {
    int64_t id = 0;
    int64_t scenarioId = 0;
    std::string identifier;      // _identifier on scalar node
    std::string valueJson;       // JSON-encoded value
};

/**
 * Expected output for comparison after scenario execution
 */
struct ScenarioExpectedOutput {
    int64_t id = 0;
    int64_t scenarioId = 0;
    std::string outputName;      // output node _name
    std::string expectedJson;    // {columns, schema, data}
};

/**
 * Trigger simulation data for a scenario
 */
struct ScenarioTrigger {
    int64_t id = 0;
    int64_t scenarioId = 0;
    std::string triggerType;
    std::string identifier;      // target csv_source _identifier
    std::string dataJson;        // DataFrame JSON
};

/**
 * Full scenario details (info + inputs + expected outputs + triggers)
 */
struct ScenarioDetails {
    ScenarioInfo info;
    std::vector<ScenarioInput> inputs;
    std::vector<ScenarioExpectedOutput> expectedOutputs;
    std::vector<ScenarioTrigger> triggers;
};

} // namespace storage
