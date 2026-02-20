#include "nodes/NodeContext.hpp"

namespace nodes {

Workload NodeContext::getInputWorkload(const std::string& name) const {
    auto it = m_inputs.find(name);
    if (it == m_inputs.end()) {
        return Workload();  // Return null workload if not found
    }
    return it->second;
}

bool NodeContext::hasInput(const std::string& name) const {
    auto it = m_inputs.find(name);
    return it != m_inputs.end() && !it->second.isNull();
}

bool NodeContext::hasInputEntry(const std::string& name) const {
    return m_inputs.find(name) != m_inputs.end();
}

void NodeContext::setOutput(const std::string& name, const Workload& workload) {
    m_outputs[name] = workload;
}

void NodeContext::setOutput(const std::string& name, int64_t value) {
    m_outputs[name] = Workload(value, NodeType::Int);
}

void NodeContext::setOutput(const std::string& name, double value) {
    m_outputs[name] = Workload(value, NodeType::Double);
}

void NodeContext::setOutput(const std::string& name, const std::string& value) {
    m_outputs[name] = Workload(value, NodeType::String);
}

void NodeContext::setOutput(const std::string& name, const char* value) {
    m_outputs[name] = Workload(std::string(value), NodeType::String);
}

void NodeContext::setOutput(const std::string& name, bool value) {
    m_outputs[name] = Workload(value);
}

void NodeContext::setOutput(const std::string& name, std::shared_ptr<dataframe::DataFrame> value) {
    m_outputs[name] = Workload(std::move(value));
}

int64_t NodeContext::getIntAtRow(const std::string& inputName, size_t rowIndex) const {
    auto workload = getInputWorkload(inputName);
    if (!m_activeCsv) {
        return workload.getInt();  // No CSV, just return scalar
    }
    auto header = m_activeCsv->getColumnNames();
    return workload.getIntAtRow(rowIndex, header, m_activeCsv);
}

double NodeContext::getDoubleAtRow(const std::string& inputName, size_t rowIndex) const {
    auto workload = getInputWorkload(inputName);
    if (!m_activeCsv) {
        return workload.getDouble();  // No CSV, just return scalar
    }
    auto header = m_activeCsv->getColumnNames();
    return workload.getDoubleAtRow(rowIndex, header, m_activeCsv);
}

std::string NodeContext::getStringAtRow(const std::string& inputName, size_t rowIndex) const {
    auto workload = getInputWorkload(inputName);
    if (!m_activeCsv) {
        return workload.getString();  // No CSV, just return scalar
    }
    auto header = m_activeCsv->getColumnNames();
    return workload.getStringAtRow(rowIndex, header, m_activeCsv);
}

void NodeContext::setError(const std::string& message) {
    m_hasError = true;
    m_errorMessage = message;
}

void NodeContext::setInput(const std::string& name, const Workload& workload) {
    m_inputs[name] = workload;

    // Auto-detect CSV for active CSV
    if (workload.getType() == NodeType::Csv && !m_activeCsv) {
        m_activeCsv = workload.getCsv();
    }
}

Workload NodeContext::getOutput(const std::string& name) const {
    auto it = m_outputs.find(name);
    if (it == m_outputs.end()) {
        return Workload();  // Return null workload if not found
    }
    return it->second;
}

} // namespace nodes
