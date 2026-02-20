#include "Profiler.hpp"
#include <iomanip>

namespace dataframe {
namespace server {

Profiler& Profiler::instance() {
    static Profiler instance;
    return instance;
}

size_t Profiler::start(const std::string& name) {
    if (!m_enabled) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t id = ++m_nextTimerId;
    m_activeTimers[id] = Timer{name, std::chrono::high_resolution_clock::now()};
    return id;
}

double Profiler::stop(size_t timerId) {
    if (!m_enabled || timerId == 0) return 0.0;

    auto endTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeTimers.find(timerId);
    if (it == m_activeTimers.end()) {
        return 0.0;
    }

    const Timer& timer = it->second;
    double durationMs = std::chrono::duration<double, std::milli>(
        endTime - timer.start).count();

    // Update stats
    Stats& stats = m_stats[timer.name];
    stats.count++;
    stats.totalMs += durationMs;
    if (durationMs < stats.minMs) stats.minMs = durationMs;
    if (durationMs > stats.maxMs) stats.maxMs = durationMs;

    m_activeTimers.erase(it);
    return durationMs;
}

Profiler::Stats Profiler::getStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stats.find(name);
    if (it != m_stats.end()) {
        return it->second;
    }
    return Stats{};
}

std::unordered_map<std::string, Profiler::Stats> Profiler::getAllStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void Profiler::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.clear();
    m_activeTimers.clear();
}

std::string Profiler::formatStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_stats.empty()) {
        return "No profiling data available.";
    }

    std::ostringstream oss;
    oss << "\n========== PROFILER STATS ==========\n";
    oss << std::left << std::setw(30) << "Operation"
        << std::right << std::setw(10) << "Count"
        << std::setw(12) << "Total(ms)"
        << std::setw(12) << "Avg(ms)"
        << std::setw(12) << "Min(ms)"
        << std::setw(12) << "Max(ms)"
        << "\n";
    oss << std::string(88, '-') << "\n";

    // Sort by total time (descending)
    std::vector<std::pair<std::string, Stats>> sorted(m_stats.begin(), m_stats.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second.totalMs > b.second.totalMs; });

    for (const auto& [name, stats] : sorted) {
        oss << std::left << std::setw(30) << name
            << std::right << std::setw(10) << stats.count
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.totalMs
            << std::setw(12) << stats.avgMs()
            << std::setw(12) << (stats.count > 0 ? stats.minMs : 0.0)
            << std::setw(12) << stats.maxMs
            << "\n";
    }
    oss << "=====================================\n";

    return oss.str();
}

} // namespace server
} // namespace dataframe
