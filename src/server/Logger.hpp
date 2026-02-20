#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <unordered_map>

namespace dataframe {
namespace server {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

/**
 * Logger singleton - gestion centralisée des logs
 */
class Logger {
public:
    static Logger& instance();

    // Configuration
    void setLevel(LogLevel level) { m_level = level; }
    void setOutputStream(std::ostream* os) { m_output = os; }
    void enableFileLogging(const std::string& filepath);
    void setLogRequests(bool enabled) { m_logRequests = enabled; }
    void setLogResponses(bool enabled) { m_logResponses = enabled; }

    // Logging methods
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    // Request/Response logging with request ID correlation
    uint64_t logRequest(const std::string& method, const std::string& target, const std::string& body = "");
    void logResponse(uint64_t requestId, int statusCode, const std::string& body, size_t bodySize = 0);

    // Helpers
    static std::string levelToString(LogLevel level);
    static std::string formatSize(size_t bytes);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);
    std::string timestamp();
    std::string truncate(const std::string& str, size_t maxLen = 500);

    LogLevel m_level = LogLevel::INFO;
    std::ostream* m_output = &std::cout;
    std::ofstream m_fileStream;
    std::mutex m_mutex;
    bool m_logRequests = true;
    bool m_logResponses = true;

    // Request ID generation and timing
    std::atomic<uint64_t> m_requestIdCounter{0};
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> m_requestStartTimes;
};

// Convenience macros
#define LOG_DEBUG(msg) dataframe::server::Logger::instance().debug(msg)
#define LOG_INFO(msg) dataframe::server::Logger::instance().info(msg)
#define LOG_WARN(msg) dataframe::server::Logger::instance().warn(msg)
#define LOG_ERROR(msg) dataframe::server::Logger::instance().error(msg)

} // namespace server
} // namespace dataframe
