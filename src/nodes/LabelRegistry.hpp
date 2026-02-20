#pragma once

#include "nodes/Types.hpp"
#include <string>
#include <unordered_map>
#include <mutex>

namespace nodes {

/**
 * Singleton registry for storing named labels during graph execution.
 *
 * Labels allow data to be named in one part of a graph (via label_define_*)
 * and referenced elsewhere (via label_ref_*) without visible connections.
 *
 * Thread-safe for concurrent access.
 */
class LabelRegistry {
public:
    /**
     * Get the singleton instance
     */
    static LabelRegistry& instance();

    /**
     * Define a label with a name and value.
     * Overwrites any existing label with the same name.
     */
    void defineLabel(const std::string& name, const Workload& value);

    /**
     * Get the value of a label by name.
     * Returns null Workload if label doesn't exist.
     */
    Workload getLabel(const std::string& name) const;

    /**
     * Check if a label exists.
     */
    bool hasLabel(const std::string& name) const;

    /**
     * Clear all labels.
     * Should be called at the start of each graph execution.
     */
    void clear();

    /**
     * Get all label names (for debugging/inspection)
     */
    std::vector<std::string> getLabelNames() const;

private:
    LabelRegistry() = default;
    LabelRegistry(const LabelRegistry&) = delete;
    LabelRegistry& operator=(const LabelRegistry&) = delete;

    std::unordered_map<std::string, Workload> m_labels;
    mutable std::mutex m_mutex;
};

} // namespace nodes
