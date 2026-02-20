#include "Logger.hpp"

namespace dataframe {
namespace server {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() = default;

Logger::~Logger() {
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

void Logger::enableFileLogging(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    m_fileStream.open(filepath, std::ios::app);
    if (m_fileStream.is_open()) {
        m_output = &m_fileStream;
    }
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "?????";
    }
}

std::string Logger::truncate(const std::string& str, size_t maxLen) {
    if (str.length() <= maxLen) {
        return str;
    }
    return str.substr(0, maxLen) + "... [truncated, total " + std::to_string(str.length()) + " bytes]";
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_level) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    *m_output << "[" << timestamp() << "] "
              << "[" << levelToString(level) << "] "
              << message << std::endl;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

std::string Logger::formatSize(size_t bytes) {
    std::ostringstream oss;
    if (bytes < 1024) {
        oss << bytes << " o";
    } else if (bytes < 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " ko";
    } else {
        oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " Mo";
    }
    return oss.str();
}

uint64_t Logger::logRequest(const std::string& method, const std::string& target, const std::string& body) {
    uint64_t requestId = ++m_requestIdCounter;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_requestStartTimes[requestId] = std::chrono::steady_clock::now();
    }

    if (!m_logRequests) return requestId;

    std::ostringstream oss;
    oss << "[REQ-" << requestId << "] " << method << " " << target;
    if (!body.empty()) {
        oss << " | Body: " << truncate(body);
    }
    log(LogLevel::INFO, oss.str());

    return requestId;
}

void Logger::logResponse(uint64_t requestId, int statusCode, const std::string& body, size_t bodySize) {
    // Calculate duration
    double durationMs = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_requestStartTimes.find(requestId);
        if (it != m_requestStartTimes.end()) {
            auto now = std::chrono::steady_clock::now();
            durationMs = std::chrono::duration<double, std::milli>(now - it->second).count();
            m_requestStartTimes.erase(it);
        }
    }

    if (!m_logResponses) return;

    std::ostringstream oss;
    oss << "[REQ-" << requestId << "] RESPONSE " << statusCode;
    if (bodySize > 0) {
        oss << " | Size: " << formatSize(bodySize);
    }
    oss << " | Time: " << std::fixed << std::setprecision(1) << durationMs << "ms";
    if (!body.empty() && m_level <= LogLevel::DEBUG) {
        oss << " | Body: " << truncate(body);
    }
    log(LogLevel::INFO, oss.str());
}

} // namespace server
} // namespace dataframe
