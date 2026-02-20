#include "nodes/NodeRegistry.hpp"
#include <algorithm>
#include <set>

namespace nodes {

NodeRegistry& NodeRegistry::instance() {
    static NodeRegistry instance;
    return instance;
}

void NodeRegistry::registerNode(NodeDefinitionPtr definition) {
    if (definition) {
        m_nodes[definition->getName()] = std::move(definition);
    }
}

void NodeRegistry::unregisterNode(const std::string& name) {
    m_nodes.erase(name);
}

NodeDefinitionPtr NodeRegistry::getNode(const std::string& name) const {
    // Try exact match first
    auto it = m_nodes.find(name);
    if (it != m_nodes.end()) {
        return it->second;
    }

    // If name contains '/', try matching just the part after '/'
    // This handles "category/nodename" format from LiteGraph
    size_t slashPos = name.find('/');
    if (slashPos != std::string::npos) {
        std::string shortName = name.substr(slashPos + 1);
        it = m_nodes.find(shortName);
        if (it != m_nodes.end()) {
            return it->second;
        }
    }

    return nullptr;
}

bool NodeRegistry::hasNode(const std::string& name) const {
    return m_nodes.find(name) != m_nodes.end();
}

std::vector<std::string> NodeRegistry::getNodeNames() const {
    std::vector<std::string> names;
    names.reserve(m_nodes.size());
    for (const auto& [name, def] : m_nodes) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> NodeRegistry::getNodeNamesInCategory(const std::string& category) const {
    std::vector<std::string> names;
    for (const auto& [name, def] : m_nodes) {
        if (def->getCategory() == category) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> NodeRegistry::getCategories() const {
    std::set<std::string> categories;
    for (const auto& [name, def] : m_nodes) {
        categories.insert(def->getCategory());
    }
    return std::vector<std::string>(categories.begin(), categories.end());
}

void NodeRegistry::clear() {
    m_nodes.clear();
}

} // namespace nodes
