#include "DataFrameFilter.hpp"
#include <algorithm>
#include <set>

namespace dataframe {

std::vector<size_t> DataFrameFilter::apply(
    const json& filterJson,
    size_t rowCount,
    const ColumnGetter& getColumn
) {
    std::vector<size_t> result;

    if (!filterJson.is_array()) {
        return result;
    }

    result.reserve(rowCount);

    // Initialiser avec tous les indices
    for (size_t i = 0; i < rowCount; ++i) {
        result.push_back(i);
    }

    // Appliquer chaque filtre successivement
    for (const auto& filterItem : filterJson) {
        std::string column = filterItem["column"];
        std::string op = filterItem["operator"];
        std::string value = filterItem["value"].dump();

        // Remove quotes if it's a string value
        if (value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        auto col = getColumn(column);
        std::vector<size_t> matchingIndices = applyOperator(col, op, value);

        // Intersection avec les indices actuels
        std::vector<size_t> newResult;
        newResult.reserve(std::min(result.size(), matchingIndices.size()));

        std::set_intersection(
            result.begin(), result.end(),
            matchingIndices.begin(), matchingIndices.end(),
            std::back_inserter(newResult)
        );

        result = std::move(newResult);
    }

    return result;
}

std::vector<size_t> DataFrameFilter::applyOperator(
    IColumnPtr col,
    const std::string& op,
    const std::string& value
) {
    if (op == "==") {
        return col->filterEqual(value);
    } else if (op == "!=") {
        return col->filterNotEqual(value);
    } else if (op == "<") {
        return col->filterLessThan(value);
    } else if (op == "<=") {
        return col->filterLessOrEqual(value);
    } else if (op == ">") {
        return col->filterGreaterThan(value);
    } else if (op == ">=") {
        return col->filterGreaterOrEqual(value);
    } else if (op == "contains") {
        return col->filterContains(value);
    }

    return {};
}

} // namespace dataframe