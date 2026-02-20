#pragma once

#include "nodes/NodeDefinition.hpp"
#include <string>
#include <vector>
#include <initializer_list>

namespace nodes {

// Forward declaration
class NodeRegistry;

/**
 * Fluent API for building node definitions
 *
 * Example usage:
 *   NodeBuilder("add", "math")
 *       .input("a", NodeType::Int)
 *       .input("b", NodeType::Int)
 *       .output("result", NodeType::Int)
 *       .onCompile([](NodeContext& ctx) {
 *           int64_t a = ctx.getInputWorkload("a").getInt();
 *           int64_t b = ctx.getInputWorkload("b").getInt();
 *           ctx.setOutput("result", a + b);
 *       })
 *       .build();
 */
class NodeBuilder {
public:
    /**
     * Create a builder for a node with given name and category
     */
    NodeBuilder(const std::string& name, const std::string& category);

    // === Input Definition ===

    /**
     * Add a single-type input
     */
    NodeBuilder& input(const std::string& name, NodeType type);

    /**
     * Add a multi-type input (accepts any of the given types)
     */
    NodeBuilder& input(const std::string& name, std::initializer_list<NodeType> types);

    /**
     * Add an optional input (not required for execution)
     */
    NodeBuilder& inputOptional(const std::string& name, NodeType type);

    /**
     * Add an optional multi-type input
     */
    NodeBuilder& inputOptional(const std::string& name, std::initializer_list<NodeType> types);

    // === Output Definition ===

    /**
     * Add a single-type output
     */
    NodeBuilder& output(const std::string& name, NodeType type);

    /**
     * Add a multi-type output
     */
    NodeBuilder& output(const std::string& name, std::initializer_list<NodeType> types);

    // === Compile Function ===

    /**
     * Set the compile function (node logic)
     */
    NodeBuilder& onCompile(CompileFunction func);

    // === Options ===

    /**
     * Mark as entry point (no required inputs, starts execution)
     */
    NodeBuilder& entryPoint();

    // === Build ===

    /**
     * Build and return the node definition
     */
    NodeDefinitionPtr build();

    /**
     * Build and register in the global registry
     */
    NodeDefinitionPtr buildAndRegister();

    /**
     * Build and register in a specific registry
     */
    NodeDefinitionPtr buildAndRegister(NodeRegistry& registry);

private:
    std::string m_name;
    std::string m_category;
    std::vector<InputDef> m_inputs;
    std::vector<OutputDef> m_outputs;
    CompileFunction m_compileFunc;
    bool m_isEntryPoint = false;
};

// Convenience alias for cleaner API
using Type = NodeType;

} // namespace nodes
