#include "nodes/LabelRegistry.hpp"

namespace nodes {

LabelRegistry& LabelRegistry::instance() {
    static LabelRegistry instance;
    return instance;
}

void LabelRegistry::defineLabel(const std::string& name, const Workload& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_labels[name] = value;
}

Workload LabelRegistry::getLabel(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_labels.find(name);
    if (it != m_labels.end()) {
        return it->second;
    }
    return Workload();  // Return null workload if not found
}

bool LabelRegistry::hasLabel(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_labels.find(name) != m_labels.end();
}

void LabelRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_labels.clear();
}

std::vector<std::string> LabelRegistry::getLabelNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_labels.size());
    for (const auto& [name, _] : m_labels) {
        names.push_back(name);
    }
    return names;
}

} // namespace nodes
