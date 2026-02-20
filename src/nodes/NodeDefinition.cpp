#include "nodes/NodeDefinition.hpp"

namespace nodes {

NodeDefinition::NodeDefinition(
    std::string name,
    std::string category,
    std::vector<InputDef> inputs,
    std::vector<OutputDef> outputs,
    CompileFunction compileFunc,
    bool isEntryPoint
)
    : m_name(std::move(name))
    , m_category(std::move(category))
    , m_inputs(std::move(inputs))
    , m_outputs(std::move(outputs))
    , m_compileFunc(std::move(compileFunc))
    , m_isEntryPoint(isEntryPoint)
{}

const InputDef* NodeDefinition::findInput(const std::string& name) const {
    for (const auto& input : m_inputs) {
        if (input.name == name) {
            return &input;
        }
    }
    return nullptr;
}

const OutputDef* NodeDefinition::findOutput(const std::string& name) const {
    for (const auto& output : m_outputs) {
        if (output.name == name) {
            return &output;
        }
    }
    return nullptr;
}

void NodeDefinition::compile(NodeContext& ctx) const {
    if (m_compileFunc) {
        m_compileFunc(ctx);
    }
}

} // namespace nodes
