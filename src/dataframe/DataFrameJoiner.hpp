#pragma once

#include "Column.hpp"
#include "StringPool.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace dataframe {

using json = nlohmann::json;

class DataFrame;

/**
 * Mode de jointure pour flexJoin
 *
 * Contrôle le format de sortie pour chaque catégorie (noMatch, singleMatch, multipleMatch).
 */
enum class JoinMode {
    KeepAll,         ///< "yes" - colonnes left + right avec données complètes
    KeepHeaderOnly,  ///< "no_but_keep_header" - colonnes left + right, valeurs right vides
    KeepLeftOnly,    ///< "no" - colonnes left seulement (pas de colonnes right)
    Skip             ///< "skip" - ne rien écrire (DataFrame vide, optimisation performance)
};

// Options pour flexJoin
struct FlexJoinOptions {
    JoinMode noMatchMode = JoinMode::KeepHeaderOnly;    // Défaut: garder headers vides
    JoinMode singleMatchMode = JoinMode::KeepAll;       // Défaut: garder la jointure
    JoinMode multipleMatchMode = JoinMode::KeepAll;     // Défaut: garder toutes les lignes
};

/**
 * Responsabilité unique : opérations de jointure entre DataFrames
 */
class DataFrameJoiner {
public:
    using ColumnGetter = std::function<IColumnPtr(const std::string&)>;
    using DataFramePtr = std::shared_ptr<DataFrame>;

    // Structure de retour pour flexJoin
    struct FlexJoinResult {
        DataFramePtr noMatch;       // Lignes sans correspondance
        DataFramePtr singleMatch;   // Lignes avec 1 correspondance
        DataFramePtr multipleMatch; // Lignes avec plusieurs correspondances
    };

    /**
     * Inner join entre deux DataFrames
     *
     * Format JSON pour les clefs:
     * {
     *   "keys": [
     *     {"left": "col1", "right": "colA"},
     *     {"left": "col2", "right": "colB"}
     *   ]
     * }
     *
     * Retourne un nouveau DataFrame avec:
     * - Colonnes clefs (noms du left, sans duplication)
     * - Colonnes non-clefs du left DataFrame
     * - Colonnes non-clefs du right DataFrame (suffixe _right en cas de collision)
     */
    static DataFramePtr innerJoin(
        const json& joinSpec,
        // Left DataFrame info
        size_t leftRowCount,
        const ColumnGetter& getLeftColumn,
        const std::vector<std::string>& leftColumnOrder,
        std::shared_ptr<StringPool> leftStringPool,
        // Right DataFrame info
        size_t rightRowCount,
        const ColumnGetter& getRightColumn,
        const std::vector<std::string>& rightColumnOrder,
        std::shared_ptr<StringPool> rightStringPool
    );

    /**
     * Flex join entre deux DataFrames avec 3 sorties séparées
     *
     * Retourne:
     * - noMatch: lignes left sans correspondance dans right
     * - singleMatch: lignes left avec exactement 1 correspondance
     * - multipleMatch: lignes left avec plusieurs correspondances
     *
     * Les options contrôlent le format de chaque sortie (JoinMode):
     * - KeepAll: colonnes left + right avec données
     * - KeepHeaderOnly: colonnes left + right (valeurs right vides)
     * - KeepLeftOnly: colonnes left seulement
     * - Skip: ne rien écrire (DataFrame vide, optimal pour performances)
     *
     * Performance: avec Skip sur noMatch/multipleMatch, performances
     * équivalentes à innerJoin (~100ms pour 20k lignes).
     */
    static FlexJoinResult flexJoin(
        const json& joinSpec,
        const FlexJoinOptions& options,
        // Left DataFrame info
        size_t leftRowCount,
        const ColumnGetter& getLeftColumn,
        const std::vector<std::string>& leftColumnOrder,
        std::shared_ptr<StringPool> leftStringPool,
        // Right DataFrame info
        size_t rightRowCount,
        const ColumnGetter& getRightColumn,
        const std::vector<std::string>& rightColumnOrder,
        std::shared_ptr<StringPool> rightStringPool
    );

private:
    // Structure pour mapper left -> right
    struct KeyMapping {
        std::string leftName;
        std::string rightName;
    };

    // Clef de jointure pour le hash table
    struct JoinKey {
        std::vector<uint64_t> values;

        bool operator==(const JoinKey& other) const {
            return values == other.values;
        }
    };

    struct JoinKeyHash {
        size_t operator()(const JoinKey& key) const {
            size_t hash = 0;
            for (auto v : key.values) {
                hash ^= std::hash<uint64_t>{}(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    using JoinHashTable = std::unordered_map<JoinKey, std::vector<size_t>, JoinKeyHash>;

    // Parse les mappings de clefs depuis le JSON
    static std::vector<KeyMapping> parseKeyMappings(const json& joinSpec);

    // Construit la hash table depuis un côté du join
    static JoinHashTable buildHashTable(
        const std::vector<std::string>& keyColumns,
        size_t rowCount,
        const ColumnGetter& getColumn,
        std::shared_ptr<StringPool> sourcePool,
        std::shared_ptr<StringPool> targetPool
    );

    // Extrait une valeur de clef comme uint64_t pour le hashing
    static uint64_t extractKeyValue(
        IColumnPtr column,
        size_t rowIndex,
        std::shared_ptr<StringPool> sourcePool,
        std::shared_ptr<StringPool> targetPool
    );

    // Informations sur une colonne résultat
    struct ResultColumnInfo {
        std::string resultName;
        std::string sourceName;
        bool fromLeft;
        bool isKey;
    };
};

} // namespace dataframe
