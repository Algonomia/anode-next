#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <sstream>

namespace dataframe {
namespace server {

/**
 * Profiler - mesure et agrège les temps d'exécution
 */
class Profiler {
public:
    struct Stats {
        size_t count = 0;
        double totalMs = 0.0;
        double minMs = std::numeric_limits<double>::max();
        double maxMs = 0.0;

        double avgMs() const { return count > 0 ? totalMs / count : 0.0; }
    };

    static Profiler& instance();

    // Enable/disable profiling
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Start a timer, returns a timer ID
    size_t start(const std::string& name);

    // Stop a timer and record the duration
    double stop(size_t timerId);

    // Get stats for a specific operation
    Stats getStats(const std::string& name) const;

    // Get all stats
    std::unordered_map<std::string, Stats> getAllStats() const;

    // Reset all stats
    void reset();

    // Format stats as string
    std::string formatStats() const;

private:
    Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    struct Timer {
        std::string name;
        std::chrono::high_resolution_clock::time_point start;
    };

    bool m_enabled = true;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Stats> m_stats;
    std::unordered_map<size_t, Timer> m_activeTimers;
    size_t m_nextTimerId = 0;
};

/**
 * RAII Scoped timer - automatically stops when destroyed
 */
class ScopedTimer {
public:
    ScopedTimer(const std::string& name)
        : m_name(name)
        , m_timerId(Profiler::instance().start(name))
    {}

    ~ScopedTimer() {
        stop();
    }

    double stop() {
        if (!m_stopped) {
            m_stopped = true;
            m_duration = Profiler::instance().stop(m_timerId);
        }
        return m_duration;
    }

    double duration() const { return m_duration; }

private:
    std::string m_name;
    size_t m_timerId;
    bool m_stopped = false;
    double m_duration = 0.0;
};

// Convenience macros
#define PROFILE_SCOPE(name) dataframe::server::ScopedTimer _profiler_##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

} // namespace server
} // namespace dataframe
