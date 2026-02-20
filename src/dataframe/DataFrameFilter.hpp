#pragma once

#include "Column.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace dataframe {

using json = nlohmann::json;

class DataFrame;

/**
 * Responsabilité unique : filtrage des DataFrames
 */
class DataFrameFilter {
public:
    using ColumnGetter = std::function<IColumnPtr(const std::string&)>;

    static std::vector<size_t> apply(
        const json& filterJson,
        size_t rowCount,
        const ColumnGetter& getColumn
    );

private:
    static std::vector<size_t> applyOperator(
        IColumnPtr col,
        const std::string& op,
        const std::string& value
    );
};

} // namespace dataframe