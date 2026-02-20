#pragma once

#include "dataframe/DataFrame.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <chrono>
#include <mutex>
#include <memory>
#include <random>

namespace dataframe {
namespace server {

using json = nlohmann::json;

/**
 * Session data containing execution results
 */
struct SessionData {
    std::string sessionId;
    // Map: nodeId -> (portName -> DataFrame)
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::shared_ptr<DataFrame>>> dataframes;
    std::chrono::steady_clock::time_point createdAt;
};

/**
 * Session manager for storing DataFrame outputs from graph execution.
 * Singleton pattern, thread-safe.
 */
class SessionManager {
public:
    static SessionManager& instance();

    /**
     * Create a new session and return its ID
     */
    std::string createSession();

    /**
     * Store a DataFrame for a node output in a session
     */
    void storeDataFrame(const std::string& sessionId,
                        const std::string& nodeId,
                        const std::string& portName,
                        std::shared_ptr<DataFrame> df);

    /**
     * Retrieve a DataFrame from a session
     * Returns nullptr if not found
     */
    std::shared_ptr<DataFrame> getDataFrame(const std::string& sessionId,
                                            const std::string& nodeId,
                                            const std::string& portName);

    /**
     * Check if a session exists
     */
    bool sessionExists(const std::string& sessionId);

    /**
     * Cleanup sessions older than maxAge
     */
    void cleanupByAge(std::chrono::minutes maxAge = std::chrono::minutes(30));

    /**
     * Cleanup oldest sessions to keep only maxSessions
     */
    void cleanupByCount(size_t maxSessions = 10);

    /**
     * Get number of active sessions
     */
    size_t sessionCount() const;

private:
    SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    std::string generateSessionId();

    std::unordered_map<std::string, SessionData> m_sessions;
    mutable std::mutex m_mutex;
};

} // namespace server
} // namespace dataframe
