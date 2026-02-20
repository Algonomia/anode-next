#pragma once

#include "dataframe/DataFrame.hpp"
#include <variant>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <initializer_list>

namespace nodes {

/**
 * Node port types - based on ARCHITECTURE-NODE.md
 *
 * 3 families:
 * - Scalars: Int, Double, String, Bool (broadcast to all rows)
 * - Vector: Field (reference to a CSV column)
 * - CSV: Csv (full DataFrame)
 */
enum class NodeType {
    // Scalars
    Int,
    Double,
    String,
    Bool,
    Null,

    // Vector
    Field,

    // CSV
    Csv
};

/**
 * Convert NodeType to string for display/serialization
 */
std::string nodeTypeToString(NodeType type);

/**
 * Convert string to NodeType
 */
NodeType stringToNodeType(const std::string& str);

/**
 * Check if a type is scalar (broadcasts to all rows)
 */
bool isScalarType(NodeType type);

/**
 * Value storage using variant for type safety
 */
using NodeValue = std::variant<
    std::monostate,                           // Null
    int64_t,                                  // Int
    double,                                   // Double
    std::string,                              // String, Field
    bool,                                     // Bool
    std::shared_ptr<dataframe::DataFrame>     // Csv
>;

/**
 * Workload = {value, type}
 *
 * Essential for multi-type ports to know what type was connected.
 * Also handles broadcasting: scalars return the same value for all rows,
 * while fields look up values in the CSV at the given row index.
 */
class Workload {
public:
    // Default constructor - creates Null workload
    Workload();

    // Type-specific constructors
    Workload(int64_t value, NodeType type = NodeType::Int);
    Workload(double value, NodeType type = NodeType::Double);
    Workload(const std::string& value, NodeType type = NodeType::String);
    Workload(const char* value, NodeType type = NodeType::String);
    Workload(bool value);
    Workload(std::shared_ptr<dataframe::DataFrame> value);

    // Generic constructor
    Workload(NodeValue value, NodeType type);

    // Getters
    NodeType getType() const { return m_type; }
    const NodeValue& getValue() const { return m_value; }

    // Type-safe value extraction (throws on wrong type)
    int64_t getInt() const;
    double getDouble() const;
    const std::string& getString() const;
    bool getBool() const;
    std::shared_ptr<dataframe::DataFrame> getCsv() const;

    // Broadcasting support - get value at row index
    // For scalars: returns same value regardless of row (broadcasting)
    // For fields: looks up value in CSV at given row
    int64_t getIntAtRow(size_t rowIndex,
                        const std::vector<std::string>& header,
                        const std::shared_ptr<dataframe::DataFrame>& csv) const;

    double getDoubleAtRow(size_t rowIndex,
                          const std::vector<std::string>& header,
                          const std::shared_ptr<dataframe::DataFrame>& csv) const;

    std::string getStringAtRow(size_t rowIndex,
                               const std::vector<std::string>& header,
                               const std::shared_ptr<dataframe::DataFrame>& csv) const;

    // Validity checks
    bool isNull() const;
    bool isScalar() const;
    bool isField() const;
    bool isCsv() const;

private:
    NodeValue m_value;
    NodeType m_type;
};

/**
 * Port type definition - supports single type or multiple types
 *
 * Examples:
 *   PortType(NodeType::Int)                              // Single type
 *   PortType({NodeType::Int, NodeType::Double})          // Multi-type
 */
class PortType {
public:
    // Single type
    explicit PortType(NodeType type);

    // Multiple types
    explicit PortType(std::initializer_list<NodeType> types);
    explicit PortType(std::vector<NodeType> types);

    // Check if a type is accepted by this port
    bool accepts(NodeType type) const;
    bool accepts(const Workload& workload) const;

    // Getters
    const std::vector<NodeType>& getTypes() const { return m_types; }
    bool isMultiType() const { return m_types.size() > 1; }
    NodeType getPrimaryType() const { return m_types.empty() ? NodeType::Null : m_types[0]; }

private:
    std::vector<NodeType> m_types;
};

} // namespace nodes
