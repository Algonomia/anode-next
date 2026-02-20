#include "nodes/DynRequest.hpp"
#include "nodes/DateTimeUtil.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <cctype>

namespace nodes {

DynRequest& DynRequest::func(const std::string& functionName) {
    m_functionName = functionName;
    return *this;
}

void DynRequest::reset() {
    m_functionName.clear();
    m_parameters.clear();
}

std::string DynRequest::nextParamName(char prefix) {
    return std::string(1, prefix) + std::to_string(m_parameters.size());
}

std::string DynRequest::escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 10);
    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

// ========== Paramètres scalaires ==========

DynRequest& DynRequest::addIntParam(int64_t value) {
    m_parameters.push_back({nextParamName('i'), std::to_string(value)});
    return *this;
}

DynRequest& DynRequest::addDoubleParam(double value) {
    std::ostringstream oss;
    oss << std::fixed << value;
    m_parameters.push_back({nextParamName('d'), oss.str()});
    return *this;
}

DynRequest& DynRequest::addStringParam(const std::string& value) {
    m_parameters.push_back({nextParamName('s'), "\"" + value + "\""});
    return *this;
}

DynRequest& DynRequest::addBoolParam(bool value) {
    m_parameters.push_back({nextParamName('b'), value ? "true" : "false"});
    return *this;
}

DynRequest& DynRequest::addNullParam() {
    m_parameters.push_back({nextParamName('n'), "null"});
    return *this;
}

// ========== Paramètres tableaux ==========

DynRequest& DynRequest::addIntArrayParam(const std::vector<int64_t>& values) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << values[i];
    }
    oss << "]";
    m_parameters.push_back({nextParamName('I'), oss.str()});
    return *this;
}

DynRequest& DynRequest::addDoubleArrayParam(const std::vector<double>& values) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << std::fixed << values[i];
    }
    oss << "]";
    m_parameters.push_back({nextParamName('D'), oss.str()});
    return *this;
}

DynRequest& DynRequest::addStringArrayParam(const std::vector<std::string>& values) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << values[i] << "\"";
    }
    oss << "]";
    m_parameters.push_back({nextParamName('S'), oss.str()});
    return *this;
}

DynRequest& DynRequest::addIntArray2DParam(const std::vector<std::vector<int64_t>>& values) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "[";
        for (size_t j = 0; j < values[i].size(); ++j) {
            if (j > 0) oss << ",";
            oss << values[i][j];
        }
        oss << "]";
    }
    oss << "]";
    m_parameters.push_back({nextParamName('J'), oss.str()});
    return *this;
}

// ========== Paramètres depuis workload ==========

DynRequest& DynRequest::addIntArrayFromWorkload(
    const nodes::Workload& workload,
    const std::shared_ptr<dataframe::DataFrame>& csv,
    bool nullIfNotDefined)
{
    if (workload.isNull()) {
        if (nullIfNotDefined) {
            m_parameters.push_back({nextParamName('I'), "null"});
        }
        return *this;
    }

    std::vector<int64_t> values;
    size_t rowCount = csv ? csv->rowCount() : 1;

    if (workload.isField() && csv) {
        // Extraire les valeurs de la colonne
        auto header = csv->getColumnNames();
        for (size_t i = 0; i < rowCount; ++i) {
            values.push_back(workload.getIntAtRow(i, header, csv));
        }
    } else {
        // Broadcast le scalaire sur toutes les lignes
        int64_t val = workload.getInt();
        values.assign(rowCount, val);
    }

    return addIntArrayParam(values);
}

DynRequest& DynRequest::addStringArrayFromWorkload(
    const nodes::Workload& workload,
    const std::shared_ptr<dataframe::DataFrame>& csv,
    bool nullIfNotDefined)
{
    if (workload.isNull()) {
        if (nullIfNotDefined) {
            m_parameters.push_back({nextParamName('S'), "null"});
        }
        return *this;
    }

    std::vector<std::string> values;
    size_t rowCount = csv ? csv->rowCount() : 1;

    if (workload.isField() && csv) {
        // Extraire les valeurs de la colonne
        auto header = csv->getColumnNames();
        for (size_t i = 0; i < rowCount; ++i) {
            values.push_back(workload.getStringAtRow(i, header, csv));
        }
    } else {
        // Broadcast le scalaire sur toutes les lignes
        std::string val = workload.getString();
        values.assign(rowCount, val);
    }

    return addStringArrayParam(values);
}

DynRequest& DynRequest::addDoubleArrayFromWorkload(
    const nodes::Workload& workload,
    const std::shared_ptr<dataframe::DataFrame>& csv,
    bool nullIfNotDefined)
{
    if (workload.isNull()) {
        if (nullIfNotDefined) {
            m_parameters.push_back({nextParamName('D'), "null"});
        }
        return *this;
    }

    std::vector<double> values;
    size_t rowCount = csv ? csv->rowCount() : 1;

    if (workload.isField() && csv) {
        // Extraire les valeurs de la colonne
        auto header = csv->getColumnNames();
        for (size_t i = 0; i < rowCount; ++i) {
            values.push_back(workload.getDoubleAtRow(i, header, csv));
        }
    } else {
        // Broadcast le scalaire sur toutes les lignes
        double val = workload.getDouble();
        values.assign(rowCount, val);
    }

    return addDoubleArrayParam(values);
}

DynRequest& DynRequest::addIntFromWorkload(
    const nodes::Workload& workload,
    const std::shared_ptr<dataframe::DataFrame>& csv,
    bool nullIfNotDefined)
{
    if (workload.isNull()) {
        if (nullIfNotDefined) {
            return addNullParam();
        }
        return *this;
    }

    if (workload.isField() && csv) {
        // Prendre la première valeur de la colonne
        auto header = csv->getColumnNames();
        return addIntParam(workload.getIntAtRow(0, header, csv));
    }

    return addIntParam(workload.getInt());
}

DynRequest& DynRequest::addStringFromWorkload(
    const nodes::Workload& workload,
    const std::shared_ptr<dataframe::DataFrame>& csv,
    bool nullIfNotDefined)
{
    if (workload.isNull()) {
        if (nullIfNotDefined) {
            return addNullParam();
        }
        return *this;
    }

    if (workload.isField() && csv) {
        // Prendre la première valeur de la colonne
        auto header = csv->getColumnNames();
        return addStringParam(workload.getStringAtRow(0, header, csv));
    }

    return addStringParam(workload.getString());
}

DynRequest& DynRequest::addTimestampFromWorkload(
    const nodes::Workload& workload,
    const std::shared_ptr<dataframe::DataFrame>& csv,
    bool nullIfNotDefined)
{
    if (workload.isNull()) {
        if (nullIfNotDefined) {
            return addNullParam();
        }
        return *this;
    }

    NodeType type = workload.getType();

    // Int or already a timestamp value
    if (type == NodeType::Int) {
        return addIntParam(workload.getInt());
    }

    // String - need to convert to timestamp
    if (type == NodeType::String) {
        int64_t timestamp = convertDateToTimestamp(workload.getString());
        return addIntParam(timestamp);
    }

    // Field - extract from CSV and verify all values are equal
    if (type == NodeType::Field && csv) {
        auto header = csv->getColumnNames();
        size_t rowCount = csv->rowCount();

        if (rowCount == 0) {
            if (nullIfNotDefined) {
                return addNullParam();
            }
            throw std::runtime_error("Empty CSV for field timestamp");
        }

        // Get first value
        std::string firstValueStr = workload.getStringAtRow(0, header, csv);
        int64_t firstTimestamp;

        // Try to parse as number first
        try {
            firstTimestamp = std::stoll(firstValueStr);
        } catch (...) {
            firstTimestamp = convertDateToTimestamp(firstValueStr);
        }

        // Verify all rows have the same value
        for (size_t i = 1; i < rowCount; ++i) {
            std::string valueStr = workload.getStringAtRow(i, header, csv);
            int64_t timestamp;
            try {
                timestamp = std::stoll(valueStr);
            } catch (...) {
                timestamp = convertDateToTimestamp(valueStr);
            }

            if (timestamp != firstTimestamp) {
                throw std::runtime_error(
                    "addTimestampFromWorkload: Not all elements are equal for field '" +
                    workload.getString() + "'");
            }
        }

        return addIntParam(firstTimestamp);
    }

    throw std::runtime_error("Cannot convert workload to timestamp");
}

// ========== Construction SQL ==========

std::string DynRequest::paramToSQL(const DynParameter& param) const {
    char prefix = param.name[0];
    const std::string& value = param.value;

    switch (prefix) {
        case 'i':  // Entier scalaire
            if (value == "null") return "NULL";
            return value;

        case 'd':  // Double scalaire
            if (value == "null") return "NULL";
            return value;

        case 's': {  // String scalaire
            if (value == "null") return "NULL";
            // Enlever les guillemets JSON et échapper
            std::string str = value.substr(1, value.size() - 2);
            return "'" + escapeString(str) + "'";
        }

        case 'b':  // Boolean
            return value;

        case 'n':  // NULL
            return "NULL";

        case 'I': {  // Tableau d'entiers
            if (value == "null") return "NULL::INT[]";
            // [1,2,3] -> ARRAY[1, 2, 3]::INT[]
            std::string inner = value.substr(1, value.size() - 2);
            return "ARRAY[" + inner + "]::INT[]";
        }

        case 'D': {  // Tableau de doubles
            if (value == "null") return "NULL::DOUBLE PRECISION[]";
            std::string inner = value.substr(1, value.size() - 2);
            return "ARRAY[" + inner + "]::DOUBLE PRECISION[]";
        }

        case 'S': {  // Tableau de strings
            if (value == "null") return "NULL::TEXT[]";
            // ["a","b"] -> ARRAY['a', 'b']::TEXT[]
            std::ostringstream oss;
            oss << "ARRAY[";

            // Parser le JSON array de strings
            bool first = true;
            size_t pos = 1;  // Skip '['
            while (pos < value.size() - 1) {
                // Skip whitespace and commas
                while (pos < value.size() - 1 && (value[pos] == ' ' || value[pos] == ',')) {
                    pos++;
                }
                if (pos >= value.size() - 1) break;

                // Expect quote
                if (value[pos] == '"') {
                    pos++;  // Skip opening quote
                    size_t start = pos;
                    while (pos < value.size() && value[pos] != '"') {
                        pos++;
                    }
                    std::string str = value.substr(start, pos - start);
                    pos++;  // Skip closing quote

                    if (!first) oss << ", ";
                    first = false;
                    oss << "'" << escapeString(str) << "'";
                } else {
                    pos++;
                }
            }

            oss << "]::TEXT[]";
            return oss.str();
        }

        case 'J': {  // Tableau 2D d'entiers
            if (value == "null") return "NULL::INT[][]";
            // [[1,2],[3,4]] -> ARRAY[ARRAY[1, 2], ARRAY[3, 4]]::INT[][]
            std::ostringstream oss;
            oss << "ARRAY[";

            // Simple parsing for 2D array
            size_t depth = 0;
            bool first = true;
            std::string current;

            for (size_t i = 1; i < value.size() - 1; ++i) {
                char c = value[i];
                if (c == '[') {
                    if (depth == 0) {
                        if (!first) oss << ", ";
                        first = false;
                        oss << "ARRAY[";
                    }
                    depth++;
                } else if (c == ']') {
                    depth--;
                    if (depth == 0) {
                        oss << current << "]";
                        current.clear();
                    }
                } else if (depth > 0) {
                    current += c;
                }
            }

            oss << "]::INT[][]";
            return oss.str();
        }

        default:
            throw std::runtime_error("Unknown parameter prefix: " + std::string(1, prefix));
    }
}

std::string DynRequest::buildSQL() const {
    if (m_functionName.empty()) {
        throw std::runtime_error("Function name not set");
    }

    std::ostringstream sql;
    sql << "SELECT * FROM " << m_functionName << "(";

    for (size_t i = 0; i < m_parameters.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << paramToSQL(m_parameters[i]);
    }

    sql << ")";
    return sql.str();
}

} // namespace nodes
