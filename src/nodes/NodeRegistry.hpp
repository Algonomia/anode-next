#pragma once

#include "nodes/NodeDefinition.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace nodes {

/**
 * Central registry for all node definitions
 *
 * Supports both singleton access (global registry) and custom instances
 * for testing or isolated environments.
 *
 * Usage:
 *   // Global registry
 *   NodeRegistry::instance().registerNode(def);
 *   auto node = NodeRegistry::instance().getNode("add");
 *
 *   // Custom registry
 *   NodeRegistry myRegistry;
 *   myRegistry.registerNode(def);
 */
class NodeRegistry {
public:
    NodeRegistry() = default;

    // Non-copyable
    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

    // Movable
    NodeRegistry(NodeRegistry&&) = default;
    NodeRegistry& operator=(NodeRegistry&&) = default;

    /**
     * Get the global singleton instance
     */
    static NodeRegistry& instance();

    // === Registration ===

    /**
     * Register a node definition
     * Overwrites if a node with the same name already exists
     */
    void registerNode(NodeDefinitionPtr definition);

    /**
     * Unregister a node by name
     */
    void unregisterNode(const std::string& name);

    // === Lookup ===

    /**
     * Get a node definition by name
     * Returns nullptr if not found
     */
    NodeDefinitionPtr getNode(const std::string& name) const;

    /**
     * Check if a node exists
     */
    bool hasNode(const std::string& name) const;

    // === Enumeration ===

    /**
     * Get all registered node names
     */
    std::vector<std::string> getNodeNames() const;

    /**
     * Get node names in a specific category
     */
    std::vector<std::string> getNodeNamesInCategory(const std::string& category) const;

    /**
     * Get all categories
     */
    std::vector<std::string> getCategories() const;

    /**
     * Get number of registered nodes
     */
    size_t size() const { return m_nodes.size(); }

    // === Clear ===

    /**
     * Remove all registered nodes
     */
    void clear();

private:
    std::unordered_map<std::string, NodeDefinitionPtr> m_nodes;
};

} // namespace nodes
