#pragma once

#include "StringPool.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <cstring>

namespace dataframe {

enum class ColumnTypeOpt {
    INT,
    DOUBLE,
    STRING
};

/**
 * Interface de base pour les colonnes optimisées
 */
class IColumn {
public:
    virtual ~IColumn() = default;

    virtual const std::string& getName() const = 0;
    virtual void setName(const std::string& name) = 0;
    virtual ColumnTypeOpt getType() const = 0;
    virtual size_t size() const = 0;
    virtual void reserve(size_t capacity) = 0;
    virtual void clear() = 0;

    // Pour le filtrage : retourne les indices des lignes qui matchent
    virtual std::vector<size_t> filterEqual(const std::string& value) const = 0;
    virtual std::vector<size_t> filterNotEqual(const std::string& value) const = 0;
    virtual std::vector<size_t> filterLessThan(const std::string& value) const = 0;
    virtual std::vector<size_t> filterLessOrEqual(const std::string& value) const = 0;
    virtual std::vector<size_t> filterGreaterThan(const std::string& value) const = 0;
    virtual std::vector<size_t> filterGreaterOrEqual(const std::string& value) const = 0;
    virtual std::vector<size_t> filterContains(const std::string& substring) const = 0;

    // Pour créer une colonne filtrée
    virtual std::shared_ptr<IColumn> filterByIndices(const std::vector<size_t>& indices) const = 0;

    // Pour le tri : remplit un vecteur d'indices triés
    virtual void getSortedIndices(std::vector<size_t>& indices, bool ascending) const = 0;

    // Clone
    virtual std::shared_ptr<IColumn> clone() const = 0;
};

/**
 * Colonne d'entiers optimisée
 * - Stockage contigu en mémoire → cache friendly
 * - Comparaisons directes → pas de branches
 */
class IntColumn : public IColumn {
public:
    explicit IntColumn(const std::string& name) : m_name(name) {
        m_data.reserve(1024);
    }

    const std::string& getName() const override { return m_name; }
    void setName(const std::string& name) override { m_name = name; }
    ColumnTypeOpt getType() const override { return ColumnTypeOpt::INT; }
    size_t size() const override { return m_data.size(); }

    void reserve(size_t capacity) override { m_data.reserve(capacity); }
    void clear() override { m_data.clear(); }

    void push_back(int value) { m_data.push_back(value); }
    void set(size_t index, int value) { m_data[index] = value; }
    int at(size_t index) const { return m_data[index]; }
    const std::vector<int>& data() const { return m_data; }

    std::vector<size_t> filterEqual(const std::string& value) const override {
        int target = std::stoi(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 10);  // Estimation

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] == target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterNotEqual(const std::string& value) const override {
        int target = std::stoi(value);
        std::vector<size_t> result;
        result.reserve(m_data.size());

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] != target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterLessThan(const std::string& value) const override {
        int target = std::stoi(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] < target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterLessOrEqual(const std::string& value) const override {
        int target = std::stoi(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] <= target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterGreaterThan(const std::string& value) const override {
        int target = std::stoi(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] > target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterGreaterOrEqual(const std::string& value) const override {
        int target = std::stoi(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] >= target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterContains(const std::string&) const override {
        return {};  // Not applicable for int
    }

    std::shared_ptr<IColumn> filterByIndices(const std::vector<size_t>& indices) const override {
        auto newCol = std::make_shared<IntColumn>(m_name);
        newCol->reserve(indices.size());
        for (size_t idx : indices) {
            if (idx < m_data.size()) {
                newCol->push_back(m_data[idx]);
            }
        }
        return newCol;
    }

    void getSortedIndices(std::vector<size_t>& indices, bool ascending) const override {
        // Tri par comptage (counting sort) si la plage est petite
        // Sinon std::sort optimisé
        if (ascending) {
            std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
                return m_data[a] < m_data[b];
            });
        } else {
            std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
                return m_data[a] > m_data[b];
            });
        }
    }

    std::shared_ptr<IColumn> clone() const override {
        auto newCol = std::make_shared<IntColumn>(m_name);
        newCol->m_data = m_data;
        return newCol;
    }

private:
    std::string m_name;
    std::vector<int> m_data;
};

/**
 * Colonne de doubles optimisée
 */
class DoubleColumn : public IColumn {
public:
    explicit DoubleColumn(const std::string& name) : m_name(name) {
        m_data.reserve(1024);
    }

    const std::string& getName() const override { return m_name; }
    void setName(const std::string& name) override { m_name = name; }
    ColumnTypeOpt getType() const override { return ColumnTypeOpt::DOUBLE; }
    size_t size() const override { return m_data.size(); }

    void reserve(size_t capacity) override { m_data.reserve(capacity); }
    void clear() override { m_data.clear(); }

    void push_back(double value) { m_data.push_back(value); }
    void set(size_t index, double value) { m_data[index] = value; }
    double at(size_t index) const { return m_data[index]; }
    const std::vector<double>& data() const { return m_data; }

    std::vector<size_t> filterEqual(const std::string& value) const override {
        double target = std::stod(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 10);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] == target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterNotEqual(const std::string& value) const override {
        double target = std::stod(value);
        std::vector<size_t> result;
        result.reserve(m_data.size());

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] != target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterLessThan(const std::string& value) const override {
        double target = std::stod(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] < target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterLessOrEqual(const std::string& value) const override {
        double target = std::stod(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] <= target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterGreaterThan(const std::string& value) const override {
        double target = std::stod(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] > target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterGreaterOrEqual(const std::string& value) const override {
        double target = std::stod(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] >= target) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterContains(const std::string&) const override {
        return {};  // Not applicable
    }

    std::shared_ptr<IColumn> filterByIndices(const std::vector<size_t>& indices) const override {
        auto newCol = std::make_shared<DoubleColumn>(m_name);
        newCol->reserve(indices.size());
        for (size_t idx : indices) {
            if (idx < m_data.size()) {
                newCol->push_back(m_data[idx]);
            }
        }
        return newCol;
    }

    void getSortedIndices(std::vector<size_t>& indices, bool ascending) const override {
        if (ascending) {
            std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
                return m_data[a] < m_data[b];
            });
        } else {
            std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
                return m_data[a] > m_data[b];
            });
        }
    }

    std::shared_ptr<IColumn> clone() const override {
        auto newCol = std::make_shared<DoubleColumn>(m_name);
        newCol->m_data = m_data;
        return newCol;
    }

private:
    std::string m_name;
    std::vector<double> m_data;
};

/**
 * Colonne de strings optimisée avec dictionary encoding
 * - Stocke des indices (uint32_t) au lieu de strings
 * - Comparaisons ultra rapides (comparaison d'entiers)
 * - Hash ultra rapide
 * - Cache friendly
 */
class StringColumn : public IColumn {
public:
    using StringId = StringPool::StringId;

    explicit StringColumn(const std::string& name, std::shared_ptr<StringPool> pool)
        : m_name(name), m_string_pool(pool) {
        m_data.reserve(1024);
    }

    const std::string& getName() const override { return m_name; }
    void setName(const std::string& name) override { m_name = name; }
    ColumnTypeOpt getType() const override { return ColumnTypeOpt::STRING; }
    size_t size() const override { return m_data.size(); }

    void reserve(size_t capacity) override { m_data.reserve(capacity); }
    void clear() override { m_data.clear(); }

    void push_back(const std::string& value) {
        StringId id = m_string_pool->intern(value);
        m_data.push_back(id);
    }

    void push_back(StringId id) {
        m_data.push_back(id);
    }

    void set(size_t index, const std::string& value) {
        StringId id = m_string_pool->intern(value);
        m_data[index] = id;
    }

    void set(size_t index, StringId id) {
        m_data[index] = id;
    }

    const std::string& at(size_t index) const {
        return m_string_pool->getString(m_data[index]);
    }

    StringId getId(size_t index) const {
        return m_data[index];
    }

    const std::vector<StringId>& data() const { return m_data; }
    std::shared_ptr<StringPool> getStringPool() const { return m_string_pool; }

    std::vector<size_t> filterEqual(const std::string& value) const override {
        StringId targetId = m_string_pool->intern(value);
        std::vector<size_t> result;
        result.reserve(m_data.size() / 10);

        // Comparaison d'entiers → ultra rapide !
        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] == targetId) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterNotEqual(const std::string& value) const override {
        StringId targetId = m_string_pool->intern(value);
        std::vector<size_t> result;
        result.reserve(m_data.size());

        for (size_t i = 0; i < m_data.size(); ++i) {
            if (m_data[i] != targetId) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterLessThan(const std::string& value) const override {
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            const std::string& str = m_string_pool->getString(m_data[i]);
            if (str < value) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterLessOrEqual(const std::string& value) const override {
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            const std::string& str = m_string_pool->getString(m_data[i]);
            if (str <= value) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterGreaterThan(const std::string& value) const override {
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            const std::string& str = m_string_pool->getString(m_data[i]);
            if (str > value) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterGreaterOrEqual(const std::string& value) const override {
        std::vector<size_t> result;
        result.reserve(m_data.size() / 2);

        for (size_t i = 0; i < m_data.size(); ++i) {
            const std::string& str = m_string_pool->getString(m_data[i]);
            if (str >= value) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<size_t> filterContains(const std::string& substring) const override {
        std::vector<size_t> result;
        result.reserve(m_data.size() / 10);

        for (size_t i = 0; i < m_data.size(); ++i) {
            const std::string& str = m_string_pool->getString(m_data[i]);
            if (str.find(substring) != std::string::npos) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::shared_ptr<IColumn> filterByIndices(const std::vector<size_t>& indices) const override {
        auto newCol = std::make_shared<StringColumn>(m_name, m_string_pool);
        newCol->reserve(indices.size());
        for (size_t idx : indices) {
            if (idx < m_data.size()) {
                newCol->push_back(m_data[idx]);
            }
        }
        return newCol;
    }

    void getSortedIndices(std::vector<size_t>& indices, bool ascending) const override {
        // Tri sur les IDs : ultra rapide car comparaison d'entiers
        // Les strings sont déjà ordonnées dans le pool
        if (ascending) {
            std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
                // Fallback: si on veut vraiment trier alphabétiquement
                const auto& strA = m_string_pool->getString(m_data[a]);
                const auto& strB = m_string_pool->getString(m_data[b]);
                return strA < strB;
            });
        } else {
            std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
                const auto& strA = m_string_pool->getString(m_data[a]);
                const auto& strB = m_string_pool->getString(m_data[b]);
                return strA > strB;
            });
        }
    }

    std::shared_ptr<IColumn> clone() const override {
        auto newCol = std::make_shared<StringColumn>(m_name, m_string_pool);
        newCol->m_data = m_data;
        return newCol;
    }

private:
    std::string m_name;
    std::shared_ptr<StringPool> m_string_pool;
    std::vector<StringId> m_data;  // Indices dans le string pool
};

using IColumnPtr = std::shared_ptr<IColumn>;

} // namespace dataframe