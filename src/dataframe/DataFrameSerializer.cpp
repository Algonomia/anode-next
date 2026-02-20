#include "DataFrameSerializer.hpp"
#include "DataFrame.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace dataframe {

std::string DataFrameSerializer::toString(
    size_t rowCount,
    const std::vector<std::string>& columnOrder,
    const ColumnGetter& getColumn,
    size_t maxRows
) {
    std::ostringstream oss;

    if (columnOrder.empty()) {
        oss << "Empty DataFrame\n";
        return oss.str();
    }

    // Headers
    for (const auto& colName : columnOrder) {
        oss << colName << "\t";
    }
    oss << "\n";

    // Rows
    size_t displayRows = std::min(rowCount, maxRows);
    for (size_t i = 0; i < displayRows; ++i) {
        for (const auto& colName : columnOrder) {
            auto col = getColumn(colName);

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                oss << intCol->at(i);
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                oss << doubleCol->at(i);
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                oss << stringCol->at(i);
            }
            oss << "\t";
        }
        oss << "\n";
    }

    if (rowCount > maxRows) {
        oss << "... (" << (rowCount - maxRows) << " more rows)\n";
    }

    return oss.str();
}

json DataFrameSerializer::toJson(
    size_t rowCount,
    const std::vector<std::string>& columnOrder,
    const ColumnGetter& getColumn
) {
    // Format columnar : {"columns": [...], "data": [[...], [...]]}
    json result = json::object();
    result["columns"] = columnOrder;

    json data = json::array();
    for (size_t i = 0; i < rowCount; ++i) {
        json row = json::array();

        for (const auto& colName : columnOrder) {
            auto col = getColumn(colName);

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                row.push_back(intCol->at(i));
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                row.push_back(doubleCol->at(i));
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                row.push_back(stringCol->at(i));
            }
        }

        data.push_back(row);
    }

    result["data"] = data;
    return result;
}

std::string DataFrameSerializer::columnTypeToString(ColumnTypeOpt type) {
    switch (type) {
        case ColumnTypeOpt::INT: return "INT";
        case ColumnTypeOpt::DOUBLE: return "DOUBLE";
        case ColumnTypeOpt::STRING: return "STRING";
        default: return "STRING";
    }
}

ColumnTypeOpt DataFrameSerializer::stringToColumnType(const std::string& typeStr) {
    if (typeStr == "INT") return ColumnTypeOpt::INT;
    if (typeStr == "DOUBLE") return ColumnTypeOpt::DOUBLE;
    return ColumnTypeOpt::STRING;
}

json DataFrameSerializer::toJsonWithSchema(
    size_t rowCount,
    const std::vector<std::string>& columnOrder,
    const ColumnGetter& getColumn
) {
    json result = json::object();
    result["columns"] = columnOrder;

    // Build schema with column types
    json schema = json::array();
    for (const auto& colName : columnOrder) {
        auto col = getColumn(colName);
        json colSchema = json::object();
        colSchema["name"] = colName;
        colSchema["type"] = columnTypeToString(col->getType());
        schema.push_back(colSchema);
    }
    result["schema"] = schema;

    // Build data array
    json data = json::array();
    for (size_t i = 0; i < rowCount; ++i) {
        json row = json::array();

        for (const auto& colName : columnOrder) {
            auto col = getColumn(colName);

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                row.push_back(intCol->at(i));
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                row.push_back(doubleCol->at(i));
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                row.push_back(stringCol->at(i));
            }
        }

        data.push_back(row);
    }

    result["data"] = data;
    return result;
}

DataFramePtr DataFrameSerializer::fromJson(const json& j) {
    auto df = std::make_shared<DataFrame>();

    // Check required fields
    if (!j.contains("columns") || !j.contains("data")) {
        throw std::runtime_error("Invalid DataFrame JSON: missing 'columns' or 'data'");
    }

    const auto& columns = j["columns"];
    const auto& data = j["data"];

    // Determine column types from schema or infer from data
    std::vector<ColumnTypeOpt> columnTypes;

    if (j.contains("schema") && j["schema"].is_array()) {
        // Use schema for types
        for (const auto& colSchema : j["schema"]) {
            std::string typeStr = colSchema.value("type", "STRING");
            columnTypes.push_back(stringToColumnType(typeStr));
        }
    } else if (!data.empty() && data[0].is_array()) {
        // Infer types from first row
        const auto& firstRow = data[0];
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i < firstRow.size()) {
                const auto& val = firstRow[i];
                if (val.is_number_integer()) {
                    columnTypes.push_back(ColumnTypeOpt::INT);
                } else if (val.is_number_float()) {
                    columnTypes.push_back(ColumnTypeOpt::DOUBLE);
                } else {
                    columnTypes.push_back(ColumnTypeOpt::STRING);
                }
            } else {
                columnTypes.push_back(ColumnTypeOpt::STRING);
            }
        }
    } else {
        // Default all to STRING
        columnTypes.resize(columns.size(), ColumnTypeOpt::STRING);
    }

    // Create typed columns
    for (size_t i = 0; i < columns.size(); ++i) {
        std::string colName = columns[i].get<std::string>();
        switch (columnTypes[i]) {
            case ColumnTypeOpt::INT:
                df->addIntColumn(colName);
                break;
            case ColumnTypeOpt::DOUBLE:
                df->addDoubleColumn(colName);
                break;
            case ColumnTypeOpt::STRING:
                df->addStringColumn(colName);
                break;
        }
    }

    // Populate data
    for (const auto& row : data) {
        if (!row.is_array()) continue;

        for (size_t i = 0; i < columns.size() && i < row.size(); ++i) {
            std::string colName = columns[i].get<std::string>();
            auto col = df->getColumn(colName);
            const auto& val = row[i];

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                if (val.is_number_integer()) {
                    intCol->push_back(val.get<int>());
                } else if (val.is_number()) {
                    intCol->push_back(static_cast<int>(val.get<double>()));
                } else if (val.is_string()) {
                    try {
                        intCol->push_back(std::stoi(val.get<std::string>()));
                    } catch (...) {
                        intCol->push_back(0);
                    }
                } else {
                    intCol->push_back(0);
                }
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                if (val.is_number()) {
                    doubleCol->push_back(val.get<double>());
                } else if (val.is_string()) {
                    try {
                        doubleCol->push_back(std::stod(val.get<std::string>()));
                    } catch (...) {
                        doubleCol->push_back(0.0);
                    }
                } else {
                    doubleCol->push_back(0.0);
                }
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                if (val.is_string()) {
                    stringCol->push_back(val.get<std::string>());
                } else if (val.is_number_integer()) {
                    stringCol->push_back(std::to_string(val.get<int>()));
                } else if (val.is_number()) {
                    stringCol->push_back(std::to_string(val.get<double>()));
                } else if (val.is_null()) {
                    stringCol->push_back("");
                } else {
                    stringCol->push_back(val.dump());
                }
            }
        }
    }

    return df;
}

} // namespace dataframe
