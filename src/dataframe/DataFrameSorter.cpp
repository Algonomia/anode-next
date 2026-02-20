#include "DataFrameSorter.hpp"
#include <algorithm>
#include <numeric>

namespace dataframe {

std::vector<size_t> DataFrameSorter::getSortedIndices(
    const json& orderJson,
    size_t rowCount,
    const ColumnGetter& getColumn
) {
    std::vector<size_t> indices(rowCount);
    std::iota(indices.begin(), indices.end(), 0);

    if (!orderJson.is_array() || orderJson.empty()) {
        return indices;
    }

    // Comparateurs inline sans branches
    auto cmp_int_asc = [](int a, int b) -> int {
        return (a > b) - (a < b);
    };
    auto cmp_int_desc = [](int a, int b) -> int {
        return (a < b) - (a > b);
    };
    auto cmp_double_asc = [](double a, double b) -> int {
        return (a > b) - (a < b);
    };
    auto cmp_double_desc = [](double a, double b) -> int {
        return (a < b) - (a > b);
    };

    using CompareFn = std::function<int(size_t, size_t)>;
    std::vector<CompareFn> comparators;
    comparators.reserve(orderJson.size());

    // Garder les shared_ptr vivants pendant le tri
    std::vector<IColumnPtr> columnPtrs;
    columnPtrs.reserve(orderJson.size());

    for (const auto& orderItem : orderJson) {
        std::string colName = orderItem["column"];
        std::string order = orderItem["order"];
        bool ascending = (order == "asc" || order == "ascending");

        auto col = getColumn(colName);
        columnPtrs.push_back(col);

        // Créer le comparateur spécialisé selon le type
        if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
            if (ascending) {
                comparators.push_back([intCol, cmp_int_asc](size_t a, size_t b) -> int {
                    return cmp_int_asc(intCol->at(a), intCol->at(b));
                });
            } else {
                comparators.push_back([intCol, cmp_int_desc](size_t a, size_t b) -> int {
                    return cmp_int_desc(intCol->at(a), intCol->at(b));
                });
            }
        } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
            if (ascending) {
                comparators.push_back([doubleCol, cmp_double_asc](size_t a, size_t b) -> int {
                    return cmp_double_asc(doubleCol->at(a), doubleCol->at(b));
                });
            } else {
                comparators.push_back([doubleCol, cmp_double_desc](size_t a, size_t b) -> int {
                    return cmp_double_desc(doubleCol->at(a), doubleCol->at(b));
                });
            }
        } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
            // Comparaison alphabétique via le StringPool
            if (ascending) {
                comparators.push_back([stringCol](size_t a, size_t b) -> int {
                    const auto& strA = stringCol->at(a);
                    const auto& strB = stringCol->at(b);
                    return (strA > strB) - (strA < strB);
                });
            } else {
                comparators.push_back([stringCol](size_t a, size_t b) -> int {
                    const auto& strA = stringCol->at(a);
                    const auto& strB = stringCol->at(b);
                    return (strA < strB) - (strA > strB);
                });
            }
        }
    }

    // Tri stable optimisé
    std::stable_sort(indices.begin(), indices.end(), [&comparators](size_t a, size_t b) -> bool {
        for (const auto& cmp : comparators) {
            int result = cmp(a, b);
            if (result != 0) {
                return result < 0;
            }
        }
        return false;
    });

    return indices;
}

} // namespace dataframe