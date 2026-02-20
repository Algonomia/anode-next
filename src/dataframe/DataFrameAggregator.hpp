#pragma once

#include "Column.hpp"
#include "StringPool.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace dataframe {

using json = nlohmann::json;

class DataFrame;

/**
 * Responsabilité unique : agrégation et groupBy des DataFrames
 */
class DataFrameAggregator {
public:
    using ColumnGetter = std::function<IColumnPtr(const std::string&)>;
    using DataFramePtr = std::shared_ptr<DataFrame>;

    static DataFramePtr groupBy(
        const json& groupByJson,
        size_t rowCount,
        const ColumnGetter& getColumn,
        const std::vector<std::string>& columnOrder,
        std::shared_ptr<StringPool> stringPool
    );

    /**
     * GroupBy hiérarchique pour Tabulator tree mode
     * Retourne du JSON avec _children pour chaque groupe
     */
    static json groupByTree(
        const json& groupByJson,
        size_t rowCount,
        const ColumnGetter& getColumn,
        const std::vector<std::string>& allColumnNames,
        std::shared_ptr<StringPool> stringPool
    );

    /**
     * Pivot: transpose une colonne en plusieurs colonnes (retourne JSON)
     * Ex: pivotColumn="question_id", valueColumn="value", indexColumns=["line_id", "task_id"]
     * Crée une colonne par valeur unique de pivotColumn, remplie avec les valeurs de valueColumn
     */
    static json pivot(
        const json& pivotJson,
        size_t rowCount,
        const ColumnGetter& getColumn,
        const std::vector<std::string>& allColumnNames,
        std::shared_ptr<StringPool> stringPool
    );

    /**
     * Pivot: transpose une colonne en plusieurs colonnes (retourne DataFrame)
     * Permet de chaîner avec d'autres opérations (filter, orderby, groupbytree)
     */
    static DataFramePtr pivotToDataFrame(
        const json& pivotJson,
        size_t rowCount,
        const ColumnGetter& getColumn,
        const std::vector<std::string>& allColumnNames,
        std::shared_ptr<StringPool> stringPool
    );

private:
    struct GroupKey {
        std::vector<uint64_t> values;

        bool operator==(const GroupKey& other) const {
            return values == other.values;
        }
    };

    struct GroupKeyHash {
        size_t operator()(const GroupKey& key) const {
            size_t hash = 0;
            for (auto v : key.values) {
                hash ^= std::hash<uint64_t>{}(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    using GroupMap = std::unordered_map<GroupKey, std::vector<size_t>, GroupKeyHash>;

    static GroupMap buildGroups(
        const std::vector<std::string>& groupByColumns,
        size_t rowCount,
        const ColumnGetter& getColumn
    );

    static void fillGroupColumns(
        DataFramePtr result,
        const std::vector<std::string>& groupByColumns,
        const GroupMap& groups,
        const ColumnGetter& getColumn
    );

    static void computeAggregations(
        DataFramePtr result,
        const json& aggregations,
        const GroupMap& groups,
        const ColumnGetter& getColumn
    );
};

} // namespace dataframe
