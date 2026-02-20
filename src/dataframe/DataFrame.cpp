#include "DataFrame.hpp"
#include "DataFrameFilter.hpp"
#include "DataFrameSorter.hpp"
#include "DataFrameAggregator.hpp"
#include "DataFrameSerializer.hpp"
#include "DataFrameJoiner.hpp"

namespace dataframe {

// ============================================================================
// Construction
// ============================================================================

void DataFrame::addColumn(IColumnPtr column) {
    if (!column) {
        throw std::invalid_argument("Cannot add null column");
    }

    const auto& name = column->getName();
    if (m_columns.find(name) != m_columns.end()) {
        throw std::invalid_argument("Column '" + name + "' already exists");
    }

    m_columns[name] = column;
    m_columnOrder.push_back(name);
}

void DataFrame::setColumn(IColumnPtr column) {
    if (!column) {
        throw std::invalid_argument("Cannot set null column");
    }

    const auto& name = column->getName();
    auto it = m_columns.find(name);
    if (it != m_columns.end()) {
        // Replace existing column
        it->second = column;
    } else {
        // Add new column
        m_columns[name] = column;
        m_columnOrder.push_back(name);
    }
}

void DataFrame::addIntColumn(const std::string& name) {
    auto col = std::make_shared<IntColumn>(name);
    addColumn(col);
}

void DataFrame::addDoubleColumn(const std::string& name) {
    auto col = std::make_shared<DoubleColumn>(name);
    addColumn(col);
}

void DataFrame::addStringColumn(const std::string& name) {
    auto col = std::make_shared<StringColumn>(name, m_string_pool);
    addColumn(col);
}

void DataFrame::addRow(const std::vector<std::string>& values) {
    if (values.size() != m_columnOrder.size()) {
        throw std::invalid_argument("Row size mismatch");
    }

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& colName = m_columnOrder[i];
        auto col = m_columns[colName];

        if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
            intCol->push_back(std::stoi(values[i]));
        } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
            doubleCol->push_back(std::stod(values[i]));
        } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
            stringCol->push_back(values[i]);
        }
    }
}

// ============================================================================
// Accesseurs
// ============================================================================

IColumnPtr DataFrame::getColumn(const std::string& name) const {
    auto it = m_columns.find(name);
    if (it == m_columns.end()) {
        throw std::out_of_range("Column '" + name + "' not found");
    }
    return it->second;
}

bool DataFrame::hasColumn(const std::string& name) const {
    return m_columns.find(name) != m_columns.end();
}

std::vector<std::string> DataFrame::getColumnNames() const {
    return m_columnOrder;
}

size_t DataFrame::rowCount() const {
    if (m_columns.empty()) return 0;
    return m_columns.begin()->second->size();
}

bool DataFrame::empty() const {
    return m_columns.empty() || rowCount() == 0;
}

// ============================================================================
// Opérations (délégation aux classes spécialisées)
// ============================================================================

std::shared_ptr<DataFrame> DataFrame::filter(const json& filterJson) const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    auto indices = DataFrameFilter::apply(filterJson, rowCount(), columnGetter);

    auto result = std::make_shared<DataFrame>();
    result->m_string_pool = m_string_pool;

    for (const auto& colName : m_columnOrder) {
        auto originalCol = getColumn(colName);
        auto filteredCol = originalCol->filterByIndices(indices);
        result->addColumn(filteredCol);
    }

    return result;
}

std::shared_ptr<DataFrame> DataFrame::orderBy(const json& orderJson) const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    auto indices = DataFrameSorter::getSortedIndices(orderJson, rowCount(), columnGetter);

    auto result = std::make_shared<DataFrame>();
    result->m_string_pool = m_string_pool;

    for (const auto& colName : m_columnOrder) {
        auto originalCol = getColumn(colName);
        auto sortedCol = originalCol->filterByIndices(indices);
        result->addColumn(sortedCol);
    }

    return result;
}

std::shared_ptr<DataFrame> DataFrame::groupBy(const json& groupByJson) const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameAggregator::groupBy(
        groupByJson,
        rowCount(),
        columnGetter,
        m_columnOrder,
        m_string_pool
    );
}

std::shared_ptr<DataFrame> DataFrame::select(
    const std::vector<std::string>& columnNames
) const {
    auto result = std::make_shared<DataFrame>();
    result->m_string_pool = m_string_pool;

    for (const auto& name : columnNames) {
        auto col = getColumn(name);
        result->addColumn(col->clone());
    }

    return result;
}

json DataFrame::groupByTree(const json& groupByJson) const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameAggregator::groupByTree(
        groupByJson,
        rowCount(),
        columnGetter,
        m_columnOrder,
        m_string_pool
    );
}

json DataFrame::pivot(const json& pivotJson) const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameAggregator::pivot(
        pivotJson,
        rowCount(),
        columnGetter,
        m_columnOrder,
        m_string_pool
    );
}

std::shared_ptr<DataFrame> DataFrame::pivotDf(const json& pivotJson) const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameAggregator::pivotToDataFrame(
        pivotJson,
        rowCount(),
        columnGetter,
        m_columnOrder,
        m_string_pool
    );
}

// ============================================================================
// Utilitaires (délégation au Serializer)
// ============================================================================

std::string DataFrame::toString(size_t maxRows) const {
    if (m_columns.empty()) {
        return "Empty DataFrame\n";
    }

    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameSerializer::toString(rowCount(), m_columnOrder, columnGetter, maxRows);
}

json DataFrame::toJson() const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameSerializer::toJson(rowCount(), m_columnOrder, columnGetter);
}

json DataFrame::toJsonWithSchema() const {
    auto columnGetter = [this](const std::string& name) { return getColumn(name); };
    return DataFrameSerializer::toJsonWithSchema(rowCount(), m_columnOrder, columnGetter);
}

std::shared_ptr<DataFrame> DataFrame::innerJoin(
    const std::shared_ptr<DataFrame>& other,
    const json& joinSpec
) const {
    auto leftGetter = [this](const std::string& name) { return getColumn(name); };
    auto rightGetter = [&other](const std::string& name) { return other->getColumn(name); };

    return DataFrameJoiner::innerJoin(
        joinSpec,
        rowCount(),
        leftGetter,
        m_columnOrder,
        m_string_pool,
        other->rowCount(),
        rightGetter,
        other->getColumnNames(),
        other->getStringPool()
    );
}

} // namespace dataframe
