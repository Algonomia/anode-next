#include "nodes/Types.hpp"
#include "dataframe/Column.hpp"
#include <algorithm>

namespace nodes {

// === Free functions ===

std::string nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::Int:    return "int";
        case NodeType::Double: return "double";
        case NodeType::String: return "string";
        case NodeType::Bool:   return "bool";
        case NodeType::Null:   return "null";
        case NodeType::Field:  return "field";
        case NodeType::Csv:    return "csv";
    }
    return "unknown";
}

NodeType stringToNodeType(const std::string& str) {
    if (str == "int")    return NodeType::Int;
    if (str == "double") return NodeType::Double;
    if (str == "string") return NodeType::String;
    if (str == "bool")   return NodeType::Bool;
    if (str == "null")   return NodeType::Null;
    if (str == "field")  return NodeType::Field;
    if (str == "csv")    return NodeType::Csv;
    throw std::invalid_argument("Unknown node type: " + str);
}

bool isScalarType(NodeType type) {
    return type == NodeType::Int ||
           type == NodeType::Double ||
           type == NodeType::String ||
           type == NodeType::Bool;
}

// === Workload ===

Workload::Workload()
    : m_value(std::monostate{}), m_type(NodeType::Null) {}

Workload::Workload(int64_t value, NodeType type)
    : m_value(value), m_type(type) {}

Workload::Workload(double value, NodeType type)
    : m_value(value), m_type(type) {}

Workload::Workload(const std::string& value, NodeType type)
    : m_value(value), m_type(type) {}

Workload::Workload(const char* value, NodeType type)
    : m_value(std::string(value)), m_type(type) {}

Workload::Workload(bool value)
    : m_value(value), m_type(NodeType::Bool) {}

Workload::Workload(std::shared_ptr<dataframe::DataFrame> value)
    : m_value(std::move(value)), m_type(NodeType::Csv) {}

Workload::Workload(NodeValue value, NodeType type)
    : m_value(std::move(value)), m_type(type) {}

int64_t Workload::getInt() const {
    if (m_type == NodeType::Int) {
        return std::get<int64_t>(m_value);
    }
    if (m_type == NodeType::Double) {
        return static_cast<int64_t>(std::get<double>(m_value));
    }
    if (m_type == NodeType::String) {
        return std::stoll(std::get<std::string>(m_value));
    }
    throw std::runtime_error("Cannot get int from type: " + nodeTypeToString(m_type));
}

double Workload::getDouble() const {
    if (m_type == NodeType::Double) {
        return std::get<double>(m_value);
    }
    if (m_type == NodeType::Int) {
        return static_cast<double>(std::get<int64_t>(m_value));
    }
    if (m_type == NodeType::String) {
        return std::stod(std::get<std::string>(m_value));
    }
    throw std::runtime_error("Cannot get double from type: " + nodeTypeToString(m_type));
}

const std::string& Workload::getString() const {
    if (m_type == NodeType::String || m_type == NodeType::Field) {
        return std::get<std::string>(m_value);
    }
    throw std::runtime_error("Cannot get string from type: " + nodeTypeToString(m_type));
}

bool Workload::getBool() const {
    if (m_type == NodeType::Bool) {
        return std::get<bool>(m_value);
    }
    throw std::runtime_error("Cannot get bool from type: " + nodeTypeToString(m_type));
}

std::shared_ptr<dataframe::DataFrame> Workload::getCsv() const {
    if (m_type == NodeType::Csv) {
        return std::get<std::shared_ptr<dataframe::DataFrame>>(m_value);
    }
    throw std::runtime_error("Cannot get csv from type: " + nodeTypeToString(m_type));
}

int64_t Workload::getIntAtRow(size_t rowIndex,
                              const std::vector<std::string>& header,
                              const std::shared_ptr<dataframe::DataFrame>& csv) const {
    // Scalars: broadcast (return same value for all rows)
    if (m_type == NodeType::Int) {
        return std::get<int64_t>(m_value);
    }
    if (m_type == NodeType::Double) {
        return static_cast<int64_t>(std::get<double>(m_value));
    }
    if (m_type == NodeType::String && m_type != NodeType::Field) {
        return std::stoll(std::get<std::string>(m_value));
    }

    // Field: lookup in CSV
    if (m_type == NodeType::Field) {
        const std::string& columnName = std::get<std::string>(m_value);
        auto column = csv->getColumn(columnName);
        if (!column) {
            std::string availableCols;
            for (const auto& col : header) {
                if (!availableCols.empty()) availableCols += ", ";
                availableCols += "'" + col + "'";
            }
            throw std::runtime_error("Column '" + columnName + "' not found. Available: " + availableCols);
        }

        if (column->getType() == dataframe::ColumnTypeOpt::INT) {
            auto intCol = std::dynamic_pointer_cast<dataframe::IntColumn>(column);
            return intCol->at(rowIndex);
        }
        if (column->getType() == dataframe::ColumnTypeOpt::DOUBLE) {
            auto dblCol = std::dynamic_pointer_cast<dataframe::DoubleColumn>(column);
            return static_cast<int64_t>(dblCol->at(rowIndex));
        }
        if (column->getType() == dataframe::ColumnTypeOpt::STRING) {
            auto strCol = std::dynamic_pointer_cast<dataframe::StringColumn>(column);
            return std::stoll(strCol->at(rowIndex));
        }
    }

    throw std::runtime_error("Cannot get int at row from type: " + nodeTypeToString(m_type));
}

double Workload::getDoubleAtRow(size_t rowIndex,
                                const std::vector<std::string>& header,
                                const std::shared_ptr<dataframe::DataFrame>& csv) const {
    // Scalars: broadcast (return same value for all rows)
    if (m_type == NodeType::Double) {
        return std::get<double>(m_value);
    }
    if (m_type == NodeType::Int) {
        return static_cast<double>(std::get<int64_t>(m_value));
    }
    if (m_type == NodeType::String && m_type != NodeType::Field) {
        return std::stod(std::get<std::string>(m_value));
    }

    // Field: lookup in CSV
    if (m_type == NodeType::Field) {
        const std::string& columnName = std::get<std::string>(m_value);
        auto column = csv->getColumn(columnName);
        if (!column) {
            std::string availableCols;
            for (const auto& col : header) {
                if (!availableCols.empty()) availableCols += ", ";
                availableCols += "'" + col + "'";
            }
            throw std::runtime_error("Column '" + columnName + "' not found. Available: " + availableCols);
        }

        if (column->getType() == dataframe::ColumnTypeOpt::DOUBLE) {
            auto dblCol = std::dynamic_pointer_cast<dataframe::DoubleColumn>(column);
            return dblCol->at(rowIndex);
        }
        if (column->getType() == dataframe::ColumnTypeOpt::INT) {
            auto intCol = std::dynamic_pointer_cast<dataframe::IntColumn>(column);
            return static_cast<double>(intCol->at(rowIndex));
        }
        if (column->getType() == dataframe::ColumnTypeOpt::STRING) {
            auto strCol = std::dynamic_pointer_cast<dataframe::StringColumn>(column);
            return std::stod(strCol->at(rowIndex));
        }
    }

    throw std::runtime_error("Cannot get double at row from type: " + nodeTypeToString(m_type));
}

std::string Workload::getStringAtRow(size_t rowIndex,
                                     const std::vector<std::string>& header,
                                     const std::shared_ptr<dataframe::DataFrame>& csv) const {
    // Scalars: broadcast
    if (m_type == NodeType::String) {
        return std::get<std::string>(m_value);
    }
    if (m_type == NodeType::Int) {
        return std::to_string(std::get<int64_t>(m_value));
    }
    if (m_type == NodeType::Double) {
        return std::to_string(std::get<double>(m_value));
    }
    if (m_type == NodeType::Bool) {
        return std::get<bool>(m_value) ? "true" : "false";
    }

    // Field: lookup in CSV
    if (m_type == NodeType::Field) {
        const std::string& columnName = std::get<std::string>(m_value);
        auto column = csv->getColumn(columnName);
        if (!column) {
            std::string availableCols;
            for (const auto& col : header) {
                if (!availableCols.empty()) availableCols += ", ";
                availableCols += "'" + col + "'";
            }
            throw std::runtime_error("Column '" + columnName + "' not found. Available: " + availableCols);
        }

        if (column->getType() == dataframe::ColumnTypeOpt::STRING) {
            auto strCol = std::dynamic_pointer_cast<dataframe::StringColumn>(column);
            return strCol->at(rowIndex);
        }
        if (column->getType() == dataframe::ColumnTypeOpt::INT) {
            auto intCol = std::dynamic_pointer_cast<dataframe::IntColumn>(column);
            return std::to_string(intCol->at(rowIndex));
        }
        if (column->getType() == dataframe::ColumnTypeOpt::DOUBLE) {
            auto dblCol = std::dynamic_pointer_cast<dataframe::DoubleColumn>(column);
            return std::to_string(dblCol->at(rowIndex));
        }
    }

    throw std::runtime_error("Cannot get string at row from type: " + nodeTypeToString(m_type));
}

bool Workload::isNull() const {
    return m_type == NodeType::Null;
}

bool Workload::isScalar() const {
    return isScalarType(m_type);
}

bool Workload::isField() const {
    return m_type == NodeType::Field;
}

bool Workload::isCsv() const {
    return m_type == NodeType::Csv;
}

// === PortType ===

PortType::PortType(NodeType type) : m_types{type} {}

PortType::PortType(std::initializer_list<NodeType> types)
    : m_types(types) {}

PortType::PortType(std::vector<NodeType> types)
    : m_types(std::move(types)) {}

bool PortType::accepts(NodeType type) const {
    return std::find(m_types.begin(), m_types.end(), type) != m_types.end();
}

bool PortType::accepts(const Workload& workload) const {
    return accepts(workload.getType());
}

} // namespace nodes
