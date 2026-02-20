#include "DataFrameAggregator.hpp"
#include "DataFrame.hpp"
#include <cstring>
#include <unordered_set>

namespace dataframe {

DataFrameAggregator::DataFramePtr DataFrameAggregator::groupBy(
    const json& groupByJson,
    size_t rowCount,
    const ColumnGetter& getColumn,
    const std::vector<std::string>& columnOrder,
    std::shared_ptr<StringPool> stringPool
) {
    if (!groupByJson.contains("groupBy") || !groupByJson.contains("aggregations")) {
        return std::make_shared<DataFrame>();
    }

    auto groupByColumns = groupByJson["groupBy"].get<std::vector<std::string>>();
    auto aggregations = groupByJson["aggregations"];

    // Créer les groupes
    auto groups = buildGroups(groupByColumns, rowCount, getColumn);

    // Créer le DataFrame résultant
    auto result = std::make_shared<DataFrame>();

    // Ajouter les colonnes de groupement
    for (const auto& colName : groupByColumns) {
        auto originalCol = getColumn(colName);
        if (std::dynamic_pointer_cast<IntColumn>(originalCol)) {
            result->addIntColumn(colName);
        } else if (std::dynamic_pointer_cast<DoubleColumn>(originalCol)) {
            result->addDoubleColumn(colName);
        } else {
            result->addStringColumn(colName);
        }
    }

    // Ajouter les colonnes d'agrégation
    for (const auto& agg : aggregations) {
        std::string alias = agg["alias"];
        std::string function = agg["function"];

        if (function == "count") {
            result->addIntColumn(alias);
        } else {
            result->addDoubleColumn(alias);
        }
    }

    // Remplir le DataFrame résultant
    fillGroupColumns(result, groupByColumns, groups, getColumn);
    computeAggregations(result, aggregations, groups, getColumn);

    return result;
}

DataFrameAggregator::GroupMap DataFrameAggregator::buildGroups(
    const std::vector<std::string>& groupByColumns,
    size_t rowCount,
    const ColumnGetter& getColumn
) {
    using ExtractorFn = std::function<uint64_t(size_t)>;
    std::vector<ExtractorFn> extractors;
    extractors.reserve(groupByColumns.size());

    for (const auto& colName : groupByColumns) {
        auto col = getColumn(colName);

        if (auto intCol = dynamic_cast<IntColumn*>(col.get())) {
            extractors.push_back([intCol](size_t i) -> uint64_t {
                return static_cast<uint64_t>(intCol->at(i));
            });
        } else if (auto doubleCol = dynamic_cast<DoubleColumn*>(col.get())) {
            extractors.push_back([doubleCol](size_t i) -> uint64_t {
                double val = doubleCol->at(i);
                uint64_t bits;
                std::memcpy(&bits, &val, sizeof(double));
                return bits;
            });
        } else if (auto stringCol = dynamic_cast<StringColumn*>(col.get())) {
            extractors.push_back([stringCol](size_t i) -> uint64_t {
                return static_cast<uint64_t>(stringCol->getId(i));
            });
        }
    }

    GroupMap groups;
    for (size_t i = 0; i < rowCount; ++i) {
        GroupKey groupKey;
        groupKey.values.reserve(extractors.size());

        for (const auto& extract : extractors) {
            groupKey.values.push_back(extract(i));
        }

        groups[groupKey].push_back(i);
    }

    return groups;
}

void DataFrameAggregator::fillGroupColumns(
    DataFramePtr result,
    const std::vector<std::string>& groupByColumns,
    const GroupMap& groups,
    const ColumnGetter& getColumn
) {
    for (const auto& [groupKey, rowIndices] : groups) {
        for (size_t i = 0; i < groupByColumns.size(); ++i) {
            auto col = result->getColumn(groupByColumns[i]);
            auto originalCol = getColumn(groupByColumns[i]);

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                auto origIntCol = std::dynamic_pointer_cast<IntColumn>(originalCol);
                intCol->push_back(origIntCol->at(rowIndices[0]));
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                auto origDoubleCol = std::dynamic_pointer_cast<DoubleColumn>(originalCol);
                doubleCol->push_back(origDoubleCol->at(rowIndices[0]));
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                auto origStringCol = std::dynamic_pointer_cast<StringColumn>(originalCol);
                stringCol->push_back(origStringCol->getId(rowIndices[0]));
            }
        }
    }
}

void DataFrameAggregator::computeAggregations(
    DataFramePtr result,
    const json& aggregations,
    const GroupMap& groups,
    const ColumnGetter& getColumn
) {
    for (const auto& [groupKey, rowIndices] : groups) {
        for (const auto& aggDef : aggregations) {
            std::string column = aggDef["column"];
            std::string function = aggDef["function"];
            std::string alias = aggDef["alias"];

            auto aggCol = result->getColumn(alias);
            auto sourceCol = getColumn(column);

            if (function == "count") {
                auto intCol = std::dynamic_pointer_cast<IntColumn>(aggCol);
                intCol->push_back(static_cast<int>(rowIndices.size()));
            } else if (function == "sum" || function == "avg") {
                double sum = 0.0;

                if (auto intCol = std::dynamic_pointer_cast<IntColumn>(sourceCol)) {
                    for (size_t idx : rowIndices) {
                        sum += intCol->at(idx);
                    }
                } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(sourceCol)) {
                    for (size_t idx : rowIndices) {
                        sum += doubleCol->at(idx);
                    }
                }

                if (function == "avg" && !rowIndices.empty()) {
                    sum /= rowIndices.size();
                }

                auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(aggCol);
                doubleCol->push_back(sum);
            } else if (function == "min" || function == "max") {
                if (rowIndices.empty()) continue;

                double extremeVal = 0.0;
                bool initialized = false;

                if (auto intCol = std::dynamic_pointer_cast<IntColumn>(sourceCol)) {
                    int extreme = intCol->at(rowIndices[0]);
                    for (size_t idx : rowIndices) {
                        int val = intCol->at(idx);
                        if (!initialized || (function == "min" ? val < extreme : val > extreme)) {
                            extreme = val;
                            initialized = true;
                        }
                    }
                    extremeVal = static_cast<double>(extreme);
                } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(sourceCol)) {
                    double extreme = doubleCol->at(rowIndices[0]);
                    for (size_t idx : rowIndices) {
                        double val = doubleCol->at(idx);
                        if (!initialized || (function == "min" ? val < extreme : val > extreme)) {
                            extreme = val;
                            initialized = true;
                        }
                    }
                    extremeVal = extreme;
                }

                auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(aggCol);
                doubleCol->push_back(extremeVal);
            }
        }
    }
}

json DataFrameAggregator::groupByTree(
    const json& groupByJson,
    size_t rowCount,
    const ColumnGetter& getColumn,
    const std::vector<std::string>& allColumnNames,
    std::shared_ptr<StringPool> stringPool
) {
    if (!groupByJson.contains("groupBy")) {
        return json::array();
    }

    auto groupByColumns = groupByJson["groupBy"].get<std::vector<std::string>>();

    // aggregations est maintenant un map colonne -> fonction
    // Ex: {"line_id": "sum", "task_id": "avg", "value": "sum"}
    json aggregations = groupByJson.value("aggregations", json::object());

    // Créer les groupes
    auto groups = buildGroups(groupByColumns, rowCount, getColumn);

    // Helper pour extraire une valeur JSON d'une colonne
    auto getJsonValue = [&](const std::string& colName, size_t rowIdx) -> json {
        auto col = getColumn(colName);
        if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
            return intCol->at(rowIdx);
        } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
            return doubleCol->at(rowIdx);
        } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
            return stringCol->at(rowIdx);
        }
        return nullptr;
    };

    // Helper pour calculer une agrégation
    auto computeAgg = [&](const std::string& function, const std::string& column,
                          const std::vector<size_t>& rowIndices) -> json {
        auto sourceCol = getColumn(column);

        if (function == "blank" || function == "none" || function == "") {
            // Retourne une valeur vide/null
            return nullptr;
        }
        else if (function == "count") {
            return static_cast<int>(rowIndices.size());
        }
        else if (function == "first") {
            // Retourne la première valeur (utile pour les colonnes string)
            if (!rowIndices.empty()) {
                return getJsonValue(column, rowIndices[0]);
            }
            return nullptr;
        }
        else if (function == "sum" || function == "avg") {
            double sum = 0.0;
            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(sourceCol)) {
                for (size_t idx : rowIndices) {
                    sum += intCol->at(idx);
                }
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(sourceCol)) {
                for (size_t idx : rowIndices) {
                    sum += doubleCol->at(idx);
                }
            }
            if (function == "avg" && !rowIndices.empty()) {
                sum /= rowIndices.size();
            }
            return sum;
        }
        else if (function == "min" || function == "max") {
            if (rowIndices.empty()) return nullptr;

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(sourceCol)) {
                int extreme = intCol->at(rowIndices[0]);
                for (size_t idx : rowIndices) {
                    int val = intCol->at(idx);
                    if (function == "min" ? val < extreme : val > extreme) {
                        extreme = val;
                    }
                }
                return extreme;
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(sourceCol)) {
                double extreme = doubleCol->at(rowIndices[0]);
                for (size_t idx : rowIndices) {
                    double val = doubleCol->at(idx);
                    if (function == "min" ? val < extreme : val > extreme) {
                        extreme = val;
                    }
                }
                return extreme;
            }
        }
        return nullptr;
    };

    // Set des colonnes de groupement pour lookup rapide
    std::unordered_set<std::string> groupBySet(groupByColumns.begin(), groupByColumns.end());

    // Format columnar: {"columns": [...], "data": [[val1, val2, ...], ...]}
    // Les _children sont aussi en format columnar intégré dans la ligne parente
    json result = json::object();
    result["columns"] = allColumnNames;

    json data = json::array();

    for (const auto& [groupKey, rowIndices] : groups) {
        json groupRow = json::array();

        // Pour chaque colonne
        for (const auto& colName : allColumnNames) {
            if (groupBySet.count(colName)) {
                // Colonne de groupement: prendre la valeur du groupe
                groupRow.push_back(getJsonValue(colName, rowIndices[0]));
            } else {
                // Colonne non-groupée: appliquer l'agrégation
                std::string aggFunc = "blank"; // default: blank (null)
                if (aggregations.is_object() && aggregations.contains(colName)) {
                    aggFunc = aggregations[colName].get<std::string>();
                }
                groupRow.push_back(computeAgg(aggFunc, colName, rowIndices));
            }
        }

        // _children : tableau de lignes en format array
        json children = json::array();
        for (size_t rowIdx : rowIndices) {
            json childRow = json::array();
            for (const auto& colName : allColumnNames) {
                childRow.push_back(getJsonValue(colName, rowIdx));
            }
            children.push_back(childRow);
        }
        groupRow.push_back(children); // _children est la dernière "colonne"

        data.push_back(groupRow);
    }

    result["data"] = data;
    return result;
}

json DataFrameAggregator::pivot(
    const json& pivotJson,
    size_t rowCount,
    const ColumnGetter& getColumn,
    const std::vector<std::string>& allColumnNames,
    std::shared_ptr<StringPool> stringPool
) {
    // Required params:
    // - pivotColumn: colonne dont les valeurs deviennent des noms de colonnes
    // - valueColumn: colonne dont les valeurs remplissent les nouvelles colonnes
    // - indexColumns: colonnes qui identifient chaque ligne pivot (optionnel, sinon toutes les autres)

    if (!pivotJson.contains("pivotColumn") || !pivotJson.contains("valueColumn")) {
        return json::array();
    }

    std::string pivotColumn = pivotJson["pivotColumn"].get<std::string>();
    std::string valueColumn = pivotJson["valueColumn"].get<std::string>();

    // Colonnes d'index (identifient une ligne dans le résultat)
    std::vector<std::string> indexColumns;
    if (pivotJson.contains("indexColumns")) {
        indexColumns = pivotJson["indexColumns"].get<std::vector<std::string>>();
    } else {
        // Par défaut: toutes les colonnes sauf pivot et value
        for (const auto& col : allColumnNames) {
            if (col != pivotColumn && col != valueColumn) {
                indexColumns.push_back(col);
            }
        }
    }

    // Préfixe optionnel pour les colonnes pivotées (vide par défaut)
    std::string prefix = pivotJson.value("prefix", "");

    // Helper pour extraire une valeur JSON d'une colonne
    auto getJsonValue = [&](const std::string& colName, size_t rowIdx) -> json {
        auto col = getColumn(colName);
        if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
            return intCol->at(rowIdx);
        } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
            return doubleCol->at(rowIdx);
        } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
            return stringCol->at(rowIdx);
        }
        return nullptr;
    };

    // Helper pour convertir une valeur en string (pour le nom de colonne)
    auto valueToString = [&](const std::string& colName, size_t rowIdx) -> std::string {
        auto col = getColumn(colName);
        if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
            return std::to_string(intCol->at(rowIdx));
        } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
            return std::to_string(static_cast<int>(doubleCol->at(rowIdx)));
        } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
            return stringCol->at(rowIdx);
        }
        return "";
    };

    // 1. Collecter toutes les valeurs uniques de pivotColumn (pour créer les colonnes)
    std::vector<std::string> pivotValues;
    std::unordered_set<std::string> pivotValuesSet;
    for (size_t i = 0; i < rowCount; ++i) {
        std::string val = valueToString(pivotColumn, i);
        if (pivotValuesSet.find(val) == pivotValuesSet.end()) {
            pivotValuesSet.insert(val);
            pivotValues.push_back(val);
        }
    }

    // 2. Grouper par indexColumns
    auto groups = buildGroups(indexColumns, rowCount, getColumn);

    // 3. Construire le résultat
    json result = json::array();

    for (const auto& [groupKey, rowIndices] : groups) {
        json row = json::object();

        // Colonnes d'index (prendre la première valeur du groupe)
        for (const auto& colName : indexColumns) {
            row[colName] = getJsonValue(colName, rowIndices[0]);
        }

        // Initialiser les colonnes pivotées à null
        for (const auto& pv : pivotValues) {
            row[prefix + pv] = nullptr;
        }

        // Remplir les colonnes pivotées
        for (size_t rowIdx : rowIndices) {
            std::string pivotVal = valueToString(pivotColumn, rowIdx);
            row[prefix + pivotVal] = getJsonValue(valueColumn, rowIdx);
        }

        result.push_back(row);
    }

    return result;
}

DataFrameAggregator::DataFramePtr DataFrameAggregator::pivotToDataFrame(
    const json& pivotJson,
    size_t rowCount,
    const ColumnGetter& getColumn,
    const std::vector<std::string>& allColumnNames,
    std::shared_ptr<StringPool> stringPool
) {
    if (!pivotJson.contains("pivotColumn") || !pivotJson.contains("valueColumn")) {
        return std::make_shared<DataFrame>();
    }

    std::string pivotColumn = pivotJson["pivotColumn"].get<std::string>();
    std::string valueColumn = pivotJson["valueColumn"].get<std::string>();

    // Colonnes d'index (identifient une ligne dans le résultat)
    std::vector<std::string> indexColumns;
    if (pivotJson.contains("indexColumns")) {
        indexColumns = pivotJson["indexColumns"].get<std::vector<std::string>>();
    } else {
        for (const auto& col : allColumnNames) {
            if (col != pivotColumn && col != valueColumn) {
                indexColumns.push_back(col);
            }
        }
    }

    std::string prefix = pivotJson.value("prefix", "");

    // Déterminer le type de la colonne value
    auto valueCol = getColumn(valueColumn);
    ColumnTypeOpt valueType = valueCol->getType();

    // Helper pour convertir une valeur en string (pour le nom de colonne)
    auto valueToString = [&](const std::string& colName, size_t rowIdx) -> std::string {
        auto col = getColumn(colName);
        if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
            return std::to_string(intCol->at(rowIdx));
        } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
            return std::to_string(static_cast<int>(doubleCol->at(rowIdx)));
        } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
            return stringCol->at(rowIdx);
        }
        return "";
    };

    // 1. Collecter toutes les valeurs uniques de pivotColumn
    std::vector<std::string> pivotValues;
    std::unordered_set<std::string> pivotValuesSet;
    for (size_t i = 0; i < rowCount; ++i) {
        std::string val = valueToString(pivotColumn, i);
        if (pivotValuesSet.find(val) == pivotValuesSet.end()) {
            pivotValuesSet.insert(val);
            pivotValues.push_back(val);
        }
    }

    // 2. Grouper par indexColumns
    auto groups = buildGroups(indexColumns, rowCount, getColumn);

    // 3. Créer le DataFrame résultat
    auto result = std::make_shared<DataFrame>();
    result->setStringPool(stringPool);

    // Ajouter les colonnes d'index (clonées du type original)
    for (const auto& colName : indexColumns) {
        auto srcCol = getColumn(colName);
        switch (srcCol->getType()) {
            case ColumnTypeOpt::INT:
                result->addColumn(std::make_shared<IntColumn>(colName));
                break;
            case ColumnTypeOpt::DOUBLE:
                result->addColumn(std::make_shared<DoubleColumn>(colName));
                break;
            case ColumnTypeOpt::STRING:
                result->addColumn(std::make_shared<StringColumn>(colName, stringPool));
                break;
        }
    }

    // Ajouter les colonnes pivotées (même type que valueColumn)
    for (const auto& pv : pivotValues) {
        std::string colName = prefix + pv;
        switch (valueType) {
            case ColumnTypeOpt::INT:
                result->addColumn(std::make_shared<IntColumn>(colName));
                break;
            case ColumnTypeOpt::DOUBLE:
                result->addColumn(std::make_shared<DoubleColumn>(colName));
                break;
            case ColumnTypeOpt::STRING:
                result->addColumn(std::make_shared<StringColumn>(colName, stringPool));
                break;
        }
    }

    // 4. Remplir les données
    for (const auto& [groupKey, rowIndices] : groups) {
        // Colonnes d'index
        for (const auto& colName : indexColumns) {
            auto srcCol = getColumn(colName);
            auto dstCol = result->getColumn(colName);
            size_t srcIdx = rowIndices[0];

            if (auto intSrc = std::dynamic_pointer_cast<IntColumn>(srcCol)) {
                std::dynamic_pointer_cast<IntColumn>(dstCol)->push_back(intSrc->at(srcIdx));
            } else if (auto doubleSrc = std::dynamic_pointer_cast<DoubleColumn>(srcCol)) {
                std::dynamic_pointer_cast<DoubleColumn>(dstCol)->push_back(doubleSrc->at(srcIdx));
            } else if (auto stringSrc = std::dynamic_pointer_cast<StringColumn>(srcCol)) {
                std::dynamic_pointer_cast<StringColumn>(dstCol)->push_back(stringSrc->at(srcIdx));
            }
        }

        // Initialiser les colonnes pivotées avec des valeurs par défaut
        for (const auto& pv : pivotValues) {
            std::string colName = prefix + pv;
            auto dstCol = result->getColumn(colName);
            switch (valueType) {
                case ColumnTypeOpt::INT:
                    std::dynamic_pointer_cast<IntColumn>(dstCol)->push_back(0);
                    break;
                case ColumnTypeOpt::DOUBLE:
                    std::dynamic_pointer_cast<DoubleColumn>(dstCol)->push_back(0.0);
                    break;
                case ColumnTypeOpt::STRING:
                    std::dynamic_pointer_cast<StringColumn>(dstCol)->push_back("");
                    break;
            }
        }

        // Remplir les colonnes pivotées avec les vraies valeurs
        size_t currentRow = result->rowCount() - 1;
        for (size_t srcIdx : rowIndices) {
            std::string pivotVal = valueToString(pivotColumn, srcIdx);
            std::string colName = prefix + pivotVal;
            auto dstCol = result->getColumn(colName);

            if (auto intDst = std::dynamic_pointer_cast<IntColumn>(dstCol)) {
                auto intSrc = std::dynamic_pointer_cast<IntColumn>(valueCol);
                intDst->set(currentRow, intSrc->at(srcIdx));
            } else if (auto doubleDst = std::dynamic_pointer_cast<DoubleColumn>(dstCol)) {
                auto doubleSrc = std::dynamic_pointer_cast<DoubleColumn>(valueCol);
                doubleDst->set(currentRow, doubleSrc->at(srcIdx));
            } else if (auto stringDst = std::dynamic_pointer_cast<StringColumn>(dstCol)) {
                auto stringSrc = std::dynamic_pointer_cast<StringColumn>(valueCol);
                stringDst->set(currentRow, stringSrc->at(srcIdx));
            }
        }
    }

    return result;
}

} // namespace dataframe
