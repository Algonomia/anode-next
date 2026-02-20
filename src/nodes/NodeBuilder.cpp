#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"

namespace nodes {

NodeBuilder::NodeBuilder(const std::string& name, const std::string& category)
    : m_name(name)
    , m_category(category)
{}

NodeBuilder& NodeBuilder::input(const std::string& name, NodeType type) {
    m_inputs.emplace_back(name, PortType(type), true);
    return *this;
}

NodeBuilder& NodeBuilder::input(const std::string& name, std::initializer_list<NodeType> types) {
    m_inputs.emplace_back(name, PortType(types), true);
    return *this;
}

NodeBuilder& NodeBuilder::inputOptional(const std::string& name, NodeType type) {
    m_inputs.emplace_back(name, PortType(type), false);
    return *this;
}

NodeBuilder& NodeBuilder::inputOptional(const std::string& name, std::initializer_list<NodeType> types) {
    m_inputs.emplace_back(name, PortType(types), false);
    return *this;
}

NodeBuilder& NodeBuilder::output(const std::string& name, NodeType type) {
    m_outputs.emplace_back(name, PortType(type));
    return *this;
}

NodeBuilder& NodeBuilder::output(const std::string& name, std::initializer_list<NodeType> types) {
    m_outputs.emplace_back(name, PortType(types));
    return *this;
}

NodeBuilder& NodeBuilder::onCompile(CompileFunction func) {
    m_compileFunc = std::move(func);
    return *this;
}

NodeBuilder& NodeBuilder::entryPoint() {
    m_isEntryPoint = true;
    return *this;
}

NodeDefinitionPtr NodeBuilder::build() {
    return std::make_shared<NodeDefinition>(
        m_name,
        m_category,
        std::move(m_inputs),
        std::move(m_outputs),
        std::move(m_compileFunc),
        m_isEntryPoint
    );
}

NodeDefinitionPtr NodeBuilder::buildAndRegister() {
    return buildAndRegister(NodeRegistry::instance());
}

NodeDefinitionPtr NodeBuilder::buildAndRegister(NodeRegistry& registry) {
    auto def = build();
    registry.registerNode(def);
    return def;
}

} // namespace nodes
