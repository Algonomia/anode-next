#pragma once

#include "nodes/Types.hpp"
#include "nodes/NodeContext.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace nodes {

/**
 * Compile function signature
 * Called when the node is executed
 */
using CompileFunction = std::function<void(NodeContext&)>;

/**
 * Definition of a node input port
 */
struct InputDef {
    std::string name;
    PortType type;
    bool required = true;

    InputDef(std::string n, PortType t, bool req = true)
        : name(std::move(n)), type(std::move(t)), required(req) {}
};

/**
 * Definition of a node output port
 */
struct OutputDef {
    std::string name;
    PortType type;

    OutputDef(std::string n, PortType t)
        : name(std::move(n)), type(std::move(t)) {}
};

/**
 * Complete node definition - immutable after creation
 *
 * Describes a node type: its name, category, inputs, outputs,
 * and the compile function that implements its logic.
 */
class NodeDefinition {
public:
    NodeDefinition(
        std::string name,
        std::string category,
        std::vector<InputDef> inputs,
        std::vector<OutputDef> outputs,
        CompileFunction compileFunc,
        bool isEntryPoint = false
    );

    // Getters
    const std::string& getName() const { return m_name; }
    const std::string& getCategory() const { return m_category; }
    const std::vector<InputDef>& getInputs() const { return m_inputs; }
    const std::vector<OutputDef>& getOutputs() const { return m_outputs; }
    bool isEntryPoint() const { return m_isEntryPoint; }

    /**
     * Find an input definition by name
     * Returns nullptr if not found
     */
    const InputDef* findInput(const std::string& name) const;

    /**
     * Find an output definition by name
     * Returns nullptr if not found
     */
    const OutputDef* findOutput(const std::string& name) const;

    /**
     * Execute the node's compile function
     */
    void compile(NodeContext& ctx) const;

private:
    std::string m_name;
    std::string m_category;
    std::vector<InputDef> m_inputs;
    std::vector<OutputDef> m_outputs;
    CompileFunction m_compileFunc;
    bool m_isEntryPoint;
};

using NodeDefinitionPtr = std::shared_ptr<const NodeDefinition>;

} // namespace nodes
