#pragma once

#include "Column.hpp"
#include "StringPool.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace dataframe {

using json = nlohmann::json;

/**
 * DataFrame optimisé pour les performances
 *
 * Optimisations:
 * - Colonnes typées (pas de std::variant)
 * - String interning pour comparaisons rapides
 * - Cache-friendly data layout
 * - Algorithmes optimisés pour chaque type
 *
 * Architecture SRP:
 * - DataFrame: gestion des données et structure
 * - DataFrameFilter: filtrage
 * - DataFrameSorter: tri
 * - DataFrameAggregator: groupBy et agrégations
 * - DataFrameSerializer: toString et toJson
 */
class DataFrame {
public:
    DataFrame() : m_string_pool(std::make_shared<StringPool>()) {}

    // Construction
    void addColumn(IColumnPtr column);
    void setColumn(IColumnPtr column);  // replaces if exists, adds if not
    void addIntColumn(const std::string& name);
    void addDoubleColumn(const std::string& name);
    void addStringColumn(const std::string& name);

    // Accesseurs
    IColumnPtr getColumn(const std::string& name) const;
    bool hasColumn(const std::string& name) const;
    std::vector<std::string> getColumnNames() const;
    size_t rowCount() const;
    size_t columnCount() const { return m_columns.size(); }
    bool empty() const;

    // Opérations (délèguent aux classes spécialisées)
    std::shared_ptr<DataFrame> filter(const json& filterJson) const;
    std::shared_ptr<DataFrame> orderBy(const json& orderJson) const;
    std::shared_ptr<DataFrame> groupBy(const json& groupByJson) const;
    std::shared_ptr<DataFrame> select(const std::vector<std::string>& columnNames) const;

    // GroupBy hiérarchique - retourne JSON avec _children pour tree view
    json groupByTree(const json& groupByJson) const;

    // Pivot - transpose une colonne en plusieurs colonnes
    json pivot(const json& pivotJson) const;

    // Utilitaires (délèguent au Serializer)
    std::string toString(size_t maxRows = 10) const;
    json toJson() const;
    json toJsonWithSchema() const;

    // String pool accessor/mutator
    std::shared_ptr<StringPool> getStringPool() const { return m_string_pool; }
    void setStringPool(std::shared_ptr<StringPool> pool) { m_string_pool = pool; }

    // Pivot - retourne un DataFrame (pour chaînage d'opérations)
    std::shared_ptr<DataFrame> pivotDf(const json& pivotJson) const;

    // Inner join avec un autre DataFrame
    // JSON format: {"keys": [{"left": "col1", "right": "colA"}, ...]}
    std::shared_ptr<DataFrame> innerJoin(
        const std::shared_ptr<DataFrame>& other,
        const json& joinSpec
    ) const;

    // Helper pour ajouter des données
    void addRow(const std::vector<std::string>& values);

private:
    std::unordered_map<std::string, IColumnPtr> m_columns;
    std::vector<std::string> m_columnOrder;
    std::shared_ptr<StringPool> m_string_pool;

    // Friend pour permettre l'accès au string pool par l'aggregator
    friend class DataFrameAggregator;
};

using DataFramePtr = std::shared_ptr<DataFrame>;

} // namespace dataframe
