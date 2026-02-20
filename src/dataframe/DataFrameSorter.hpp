#pragma once

#include "Column.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace dataframe {

using json = nlohmann::json;

/**
 * Responsabilité unique : tri des DataFrames
 */
class DataFrameSorter {
public:
    using ColumnGetter = std::function<IColumnPtr(const std::string&)>;

    static std::vector<size_t> getSortedIndices(
        const json& orderJson,
        size_t rowCount,
        const ColumnGetter& getColumn
    );
};

} // namespace dataframe