#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace nodes {

/**
 * Status of a node during execution
 */
enum class ExecutionStatus {
    Started,    // Node began execution (yellow indicator)
    Completed,  // Node finished successfully (green indicator)
    Failed      // Node had an error (red indicator)
};

/**
 * Event emitted during graph execution for real-time feedback
 */
struct ExecutionEvent {
    std::string nodeId;              // Which node
    ExecutionStatus status;          // Current status
    int64_t durationMs = 0;          // Execution time (only for Completed/Failed)
    std::string errorMessage;        // Error message (only for Failed)
    nlohmann::json csvMetadata;      // CSV output metadata (only for Completed with CSV output)

    /**
     * Convert to JSON for SSE transmission
     */
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["node_id"] = nodeId;

        switch (status) {
            case ExecutionStatus::Started:
                j["status"] = "started";
                break;
            case ExecutionStatus::Completed:
                j["status"] = "completed";
                j["duration_ms"] = durationMs;
                if (!csvMetadata.empty()) {
                    j["csv_metadata"] = csvMetadata;
                }
                break;
            case ExecutionStatus::Failed:
                j["status"] = "failed";
                j["duration_ms"] = durationMs;
                j["error_message"] = errorMessage;
                break;
        }

        return j;
    }
};

/**
 * Callback type for execution events
 */
using ExecutionCallback = std::function<void(const ExecutionEvent&)>;

} // namespace nodes
