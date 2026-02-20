#include "server/SessionManager.hpp"
#include "server/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace dataframe {
namespace server {

SessionManager& SessionManager::instance() {
    static SessionManager instance;
    return instance;
}

std::string SessionManager::generateSessionId() {
    // Generate a random session ID: sess_<16 hex chars>
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t value = dis(gen);
    std::stringstream ss;
    ss << "sess_" << std::hex << std::setfill('0') << std::setw(16) << value;
    return ss.str();
}

std::string SessionManager::createSession() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Cleanup old sessions if we have too many
    if (m_sessions.size() >= 10) {
        // Find and remove the oldest session
        auto oldest = m_sessions.begin();
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it->second.createdAt < oldest->second.createdAt) {
                oldest = it;
            }
        }
        LOG_DEBUG("Removing oldest session: " + oldest->first);
        m_sessions.erase(oldest);
    }

    std::string sessionId = generateSessionId();
    SessionData data;
    data.sessionId = sessionId;
    data.createdAt = std::chrono::steady_clock::now();

    m_sessions[sessionId] = std::move(data);

    LOG_DEBUG("Created session: " + sessionId);
    return sessionId;
}

void SessionManager::storeDataFrame(const std::string& sessionId,
                                    const std::string& nodeId,
                                    const std::string& portName,
                                    std::shared_ptr<DataFrame> df) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) {
        LOG_WARN("Session not found: " + sessionId);
        return;
    }

    it->second.dataframes[nodeId][portName] = df;
    LOG_DEBUG("Stored DataFrame for " + sessionId + "/" + nodeId + "/" + portName +
              " (" + std::to_string(df ? df->rowCount() : 0) + " rows)");
}

std::shared_ptr<DataFrame> SessionManager::getDataFrame(const std::string& sessionId,
                                                        const std::string& nodeId,
                                                        const std::string& portName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto sessionIt = m_sessions.find(sessionId);
    if (sessionIt == m_sessions.end()) {
        return nullptr;
    }

    auto nodeIt = sessionIt->second.dataframes.find(nodeId);
    if (nodeIt == sessionIt->second.dataframes.end()) {
        return nullptr;
    }

    auto portIt = nodeIt->second.find(portName);
    if (portIt == nodeIt->second.end()) {
        return nullptr;
    }

    return portIt->second;
}

bool SessionManager::sessionExists(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.find(sessionId) != m_sessions.end();
}

void SessionManager::cleanupByAge(std::chrono::minutes maxAge) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    size_t removed = 0;

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.createdAt);
        if (age > maxAge) {
            it = m_sessions.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        LOG_DEBUG("Cleaned up " + std::to_string(removed) + " old sessions");
    }
}

void SessionManager::cleanupByCount(size_t maxSessions) {
    std::lock_guard<std::mutex> lock(m_mutex);

    while (m_sessions.size() > maxSessions) {
        // Find and remove the oldest session
        auto oldest = m_sessions.begin();
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it->second.createdAt < oldest->second.createdAt) {
                oldest = it;
            }
        }
        LOG_DEBUG("Removing oldest session: " + oldest->first);
        m_sessions.erase(oldest);
    }
}

size_t SessionManager::sessionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.size();
}

} // namespace server
} // namespace dataframe
