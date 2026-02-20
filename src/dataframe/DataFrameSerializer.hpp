#pragma once

#include "Column.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace dataframe {

class DataFrame;
using DataFramePtr = std::shared_ptr<DataFrame>;

using json = nlohmann::json;

/**
 * Responsabilité unique : sérialisation des DataFrames
 */
class DataFrameSerializer {
public:
    using ColumnGetter = std::function<IColumnPtr(const std::string&)>;

    static std::string toString(
        size_t rowCount,
        const std::vector<std::string>& columnOrder,
        const ColumnGetter& getColumn,
        size_t maxRows = 10
    );

    static json toJson(
        size_t rowCount,
        const std::vector<std::string>& columnOrder,
        const ColumnGetter& getColumn
    );

    /**
     * Serialize DataFrame with schema (column types) for persistence
     * Format:
     * {
     *   "columns": ["col1", "col2"],
     *   "schema": [{"name": "col1", "type": "INT"}, {"name": "col2", "type": "STRING"}],
     *   "data": [[1, "hello"], [2, "world"]]
     * }
     */
    static json toJsonWithSchema(
        size_t rowCount,
        const std::vector<std::string>& columnOrder,
        const ColumnGetter& getColumn
    );

    /**
     * Deserialize DataFrame from JSON with schema
     * Reconstructs typed columns based on schema information
     */
    static DataFramePtr fromJson(const json& j);

    /**
     * Helper: convert ColumnTypeOpt to string
     */
    static std::string columnTypeToString(ColumnTypeOpt type);

    /**
     * Helper: convert string to ColumnTypeOpt
     */
    static ColumnTypeOpt stringToColumnType(const std::string& typeStr);
};

} // namespace dataframe
