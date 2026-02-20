#include "DataFrameJoiner.hpp"
#include "DataFrame.hpp"
#include <cstring>
#include <stdexcept>

namespace dataframe {

std::vector<DataFrameJoiner::KeyMapping> DataFrameJoiner::parseKeyMappings(const json& joinSpec) {
    std::vector<KeyMapping> mappings;

    if (!joinSpec.contains("keys")) {
        throw std::invalid_argument("Join spec must contain 'keys'");
    }

    const auto& keys = joinSpec["keys"];
    if (!keys.is_array() || keys.empty()) {
        throw std::invalid_argument("'keys' must be a non-empty array");
    }

    for (const auto& keyDef : keys) {
        KeyMapping km;
        if (keyDef.is_string()) {
            // Shorthand: même nom pour les deux côtés
            km.leftName = keyDef.get<std::string>();
            km.rightName = km.leftName;
        } else if (keyDef.is_object()) {
            if (!keyDef.contains("left") || !keyDef.contains("right")) {
                throw std::invalid_argument("Key definition must have 'left' and 'right' fields");
            }
            km.leftName = keyDef["left"].get<std::string>();
            km.rightName = keyDef["right"].get<std::string>();
        } else {
            throw std::invalid_argument("Invalid key definition: must be string or object");
        }
        mappings.push_back(km);
    }

    return mappings;
}

uint64_t DataFrameJoiner::extractKeyValue(
    IColumnPtr column,
    size_t rowIndex,
    std::shared_ptr<StringPool> /*sourcePool*/,
    std::shared_ptr<StringPool> targetPool
) {
    switch (column->getType()) {
        case ColumnTypeOpt::INT: {
            auto intCol = std::static_pointer_cast<IntColumn>(column);
            return static_cast<uint64_t>(static_cast<uint32_t>(intCol->at(rowIndex)));
        }
        case ColumnTypeOpt::DOUBLE: {
            auto doubleCol = std::static_pointer_cast<DoubleColumn>(column);
            double val = doubleCol->at(rowIndex);
            uint64_t bits;
            std::memcpy(&bits, &val, sizeof(double));
            return bits;
        }
        case ColumnTypeOpt::STRING: {
            auto strCol = std::static_pointer_cast<StringColumn>(column);
            const std::string& str = strCol->at(rowIndex);
            // Intern dans le pool cible pour garantir une comparaison cohérente
            return static_cast<uint64_t>(targetPool->intern(str));
        }
    }
    return 0;
}

DataFrameJoiner::JoinHashTable DataFrameJoiner::buildHashTable(
    const std::vector<std::string>& keyColumns,
    size_t rowCount,
    const ColumnGetter& getColumn,
    std::shared_ptr<StringPool> sourcePool,
    std::shared_ptr<StringPool> targetPool
) {
    // Récupérer les colonnes de clef
    std::vector<IColumnPtr> keyCols;
    keyCols.reserve(keyColumns.size());
    for (const auto& colName : keyColumns) {
        keyCols.push_back(getColumn(colName));
    }

    JoinHashTable table;
    table.reserve(rowCount);

    for (size_t i = 0; i < rowCount; ++i) {
        JoinKey key;
        key.values.reserve(keyCols.size());

        for (const auto& col : keyCols) {
            key.values.push_back(extractKeyValue(col, i, sourcePool, targetPool));
        }

        table[key].push_back(i);
    }

    return table;
}

DataFrameJoiner::DataFramePtr DataFrameJoiner::innerJoin(
    const json& joinSpec,
    size_t leftRowCount,
    const ColumnGetter& getLeftColumn,
    const std::vector<std::string>& leftColumnOrder,
    std::shared_ptr<StringPool> leftStringPool,
    size_t rightRowCount,
    const ColumnGetter& getRightColumn,
    const std::vector<std::string>& rightColumnOrder,
    std::shared_ptr<StringPool> rightStringPool
) {
    // 1. Parser les mappings de clefs
    auto keyMappings = parseKeyMappings(joinSpec);

    // 2. Valider que les types des clefs correspondent
    for (const auto& km : keyMappings) {
        auto leftCol = getLeftColumn(km.leftName);
        auto rightCol = getRightColumn(km.rightName);

        if (!leftCol) {
            throw std::invalid_argument("Left key column not found: " + km.leftName);
        }
        if (!rightCol) {
            throw std::invalid_argument("Right key column not found: " + km.rightName);
        }
        if (leftCol->getType() != rightCol->getType()) {
            throw std::invalid_argument(
                "Key type mismatch: '" + km.leftName + "' vs '" + km.rightName + "'"
            );
        }
    }

    // 3. Créer le StringPool résultat
    auto resultPool = std::make_shared<StringPool>();

    // 4. Décider quel côté construire (le plus petit = build)
    bool buildFromLeft = (leftRowCount <= rightRowCount);

    // 5. Extraire les noms de colonnes clefs pour chaque côté
    std::vector<std::string> leftKeys, rightKeys;
    for (const auto& km : keyMappings) {
        leftKeys.push_back(km.leftName);
        rightKeys.push_back(km.rightName);
    }

    // 6. Construire la hash table depuis le côté le plus petit
    JoinHashTable hashTable;
    if (buildFromLeft) {
        hashTable = buildHashTable(leftKeys, leftRowCount, getLeftColumn, leftStringPool, resultPool);
    } else {
        hashTable = buildHashTable(rightKeys, rightRowCount, getRightColumn, rightStringPool, resultPool);
    }

    // 7. Déterminer le schéma résultat
    std::unordered_set<std::string> leftKeySet(leftKeys.begin(), leftKeys.end());
    std::unordered_set<std::string> rightKeySet(rightKeys.begin(), rightKeys.end());

    auto result = std::make_shared<DataFrame>();
    result->setStringPool(resultPool);

    std::vector<ResultColumnInfo> resultColumns;
    std::unordered_set<std::string> usedNames;

    // Ajouter les colonnes clefs (noms du left)
    for (const auto& km : keyMappings) {
        auto col = getLeftColumn(km.leftName);
        switch (col->getType()) {
            case ColumnTypeOpt::INT:
                result->addColumn(std::make_shared<IntColumn>(km.leftName));
                break;
            case ColumnTypeOpt::DOUBLE:
                result->addColumn(std::make_shared<DoubleColumn>(km.leftName));
                break;
            case ColumnTypeOpt::STRING:
                result->addColumn(std::make_shared<StringColumn>(km.leftName, resultPool));
                break;
        }
        resultColumns.push_back({km.leftName, km.leftName, true, true});
        usedNames.insert(km.leftName);
    }

    // Ajouter les colonnes non-clefs du left
    for (const auto& colName : leftColumnOrder) {
        if (leftKeySet.count(colName) == 0) {
            auto col = getLeftColumn(colName);
            std::string finalName = colName;
            if (usedNames.count(colName) > 0) {
                finalName = colName + "_left";
            }
            switch (col->getType()) {
                case ColumnTypeOpt::INT:
                    result->addColumn(std::make_shared<IntColumn>(finalName));
                    break;
                case ColumnTypeOpt::DOUBLE:
                    result->addColumn(std::make_shared<DoubleColumn>(finalName));
                    break;
                case ColumnTypeOpt::STRING:
                    result->addColumn(std::make_shared<StringColumn>(finalName, resultPool));
                    break;
            }
            resultColumns.push_back({finalName, colName, true, false});
            usedNames.insert(finalName);
        }
    }

    // Ajouter les colonnes non-clefs du right (avec gestion des collisions)
    for (const auto& colName : rightColumnOrder) {
        if (rightKeySet.count(colName) == 0) {
            auto col = getRightColumn(colName);
            std::string finalName = colName;
            if (usedNames.count(colName) > 0) {
                finalName = colName + "_right";
            }
            switch (col->getType()) {
                case ColumnTypeOpt::INT:
                    result->addColumn(std::make_shared<IntColumn>(finalName));
                    break;
                case ColumnTypeOpt::DOUBLE:
                    result->addColumn(std::make_shared<DoubleColumn>(finalName));
                    break;
                case ColumnTypeOpt::STRING:
                    result->addColumn(std::make_shared<StringColumn>(finalName, resultPool));
                    break;
            }
            resultColumns.push_back({finalName, colName, false, false});
            usedNames.insert(finalName);
        }
    }

    // 8. Probe et émettre les correspondances
    const auto& probeKeys = buildFromLeft ? rightKeys : leftKeys;
    const auto& probeGetter = buildFromLeft ? getRightColumn : getLeftColumn;
    auto probePool = buildFromLeft ? rightStringPool : leftStringPool;
    size_t probeRowCount = buildFromLeft ? rightRowCount : leftRowCount;

    // Pré-charger les colonnes probe
    std::vector<IColumnPtr> probeCols;
    for (const auto& keyName : probeKeys) {
        probeCols.push_back(probeGetter(keyName));
    }

    for (size_t probeIdx = 0; probeIdx < probeRowCount; ++probeIdx) {
        // Construire la clef probe
        JoinKey probeKey;
        probeKey.values.reserve(probeKeys.size());
        for (const auto& col : probeCols) {
            probeKey.values.push_back(extractKeyValue(col, probeIdx, probePool, resultPool));
        }

        // Chercher dans la hash table
        auto it = hashTable.find(probeKey);
        if (it != hashTable.end()) {
            // Émettre une ligne résultat par correspondance
            for (size_t buildIdx : it->second) {
                size_t leftIdx = buildFromLeft ? buildIdx : probeIdx;
                size_t rightIdx = buildFromLeft ? probeIdx : buildIdx;

                // Copier les valeurs dans les colonnes résultat
                for (const auto& rc : resultColumns) {
                    auto resultCol = result->getColumn(rc.resultName);
                    IColumnPtr sourceCol;
                    size_t sourceIdx;

                    if (rc.isKey || rc.fromLeft) {
                        sourceCol = getLeftColumn(rc.sourceName);
                        sourceIdx = leftIdx;
                    } else {
                        sourceCol = getRightColumn(rc.sourceName);
                        sourceIdx = rightIdx;
                    }

                    // Copier selon le type
                    switch (sourceCol->getType()) {
                        case ColumnTypeOpt::INT: {
                            auto src = std::static_pointer_cast<IntColumn>(sourceCol);
                            auto dst = std::static_pointer_cast<IntColumn>(resultCol);
                            dst->push_back(src->at(sourceIdx));
                            break;
                        }
                        case ColumnTypeOpt::DOUBLE: {
                            auto src = std::static_pointer_cast<DoubleColumn>(sourceCol);
                            auto dst = std::static_pointer_cast<DoubleColumn>(resultCol);
                            dst->push_back(src->at(sourceIdx));
                            break;
                        }
                        case ColumnTypeOpt::STRING: {
                            auto src = std::static_pointer_cast<StringColumn>(sourceCol);
                            auto dst = std::static_pointer_cast<StringColumn>(resultCol);
                            // Intern la string dans le pool résultat
                            dst->push_back(src->at(sourceIdx));
                            break;
                        }
                    }
                }
            }
        }
    }

    return result;
}

DataFrameJoiner::FlexJoinResult DataFrameJoiner::flexJoin(
    const json& joinSpec,
    const FlexJoinOptions& options,
    size_t leftRowCount,
    const ColumnGetter& getLeftColumn,
    const std::vector<std::string>& leftColumnOrder,
    std::shared_ptr<StringPool> leftStringPool,
    size_t rightRowCount,
    const ColumnGetter& getRightColumn,
    const std::vector<std::string>& rightColumnOrder,
    std::shared_ptr<StringPool> rightStringPool
) {
    // 1. Parser les mappings de clefs
    auto keyMappings = parseKeyMappings(joinSpec);

    // 2. Valider que les types des clefs correspondent
    for (const auto& km : keyMappings) {
        auto leftCol = getLeftColumn(km.leftName);
        auto rightCol = getRightColumn(km.rightName);

        if (!leftCol) {
            throw std::invalid_argument("Left key column not found: " + km.leftName);
        }
        if (!rightCol) {
            throw std::invalid_argument("Right key column not found: " + km.rightName);
        }
        if (leftCol->getType() != rightCol->getType()) {
            throw std::invalid_argument(
                "Key type mismatch: '" + km.leftName + "' vs '" + km.rightName + "'"
            );
        }
    }

    // 3. Créer le StringPool résultat
    auto resultPool = std::make_shared<StringPool>();

    // 4. Extraire les noms de colonnes clefs pour chaque côté
    std::vector<std::string> leftKeys, rightKeys;
    for (const auto& km : keyMappings) {
        leftKeys.push_back(km.leftName);
        rightKeys.push_back(km.rightName);
    }

    // 5. Construire la hash table depuis RIGHT (pour flexJoin, on probe toujours depuis left)
    JoinHashTable hashTable = buildHashTable(rightKeys, rightRowCount, getRightColumn, rightStringPool, resultPool);

    // 6. Déterminer le schéma résultat
    std::unordered_set<std::string> leftKeySet(leftKeys.begin(), leftKeys.end());
    std::unordered_set<std::string> rightKeySet(rightKeys.begin(), rightKeys.end());

    std::vector<ResultColumnInfo> resultColumns;
    std::unordered_set<std::string> usedNames;

    // Colonnes clefs (noms du left)
    for (const auto& km : keyMappings) {
        resultColumns.push_back({km.leftName, km.leftName, true, true});
        usedNames.insert(km.leftName);
    }

    // Colonnes non-clefs du left
    for (const auto& colName : leftColumnOrder) {
        if (leftKeySet.count(colName) == 0) {
            std::string finalName = colName;
            if (usedNames.count(colName) > 0) {
                finalName = colName + "_left";
            }
            resultColumns.push_back({finalName, colName, true, false});
            usedNames.insert(finalName);
        }
    }

    // Colonnes non-clefs du right
    for (const auto& colName : rightColumnOrder) {
        if (rightKeySet.count(colName) == 0) {
            std::string finalName = colName;
            if (usedNames.count(colName) > 0) {
                finalName = colName + "_right";
            }
            resultColumns.push_back({finalName, colName, false, false});
            usedNames.insert(finalName);
        }
    }

    // 7. Helper pour créer un DataFrame avec le bon schéma selon le mode
    auto createResultDF = [&](JoinMode mode) -> DataFramePtr {
        auto df = std::make_shared<DataFrame>();
        df->setStringPool(resultPool);

        // Mode Skip: DataFrame vide (pas de colonnes)
        if (mode == JoinMode::Skip) {
            return df;
        }

        for (const auto& rc : resultColumns) {
            // Skip colonnes right si KeepLeftOnly
            if (!rc.fromLeft && !rc.isKey && mode == JoinMode::KeepLeftOnly) {
                continue;
            }

            auto sourceCol = rc.fromLeft ? getLeftColumn(rc.sourceName) : getRightColumn(rc.sourceName);
            switch (sourceCol->getType()) {
                case ColumnTypeOpt::INT:
                    df->addColumn(std::make_shared<IntColumn>(rc.resultName));
                    break;
                case ColumnTypeOpt::DOUBLE:
                    df->addColumn(std::make_shared<DoubleColumn>(rc.resultName));
                    break;
                case ColumnTypeOpt::STRING:
                    df->addColumn(std::make_shared<StringColumn>(rc.resultName, resultPool));
                    break;
            }
        }
        return df;
    };

    // 8. Créer les 3 DataFrames résultat
    auto noMatch = createResultDF(options.noMatchMode);
    auto singleMatch = createResultDF(options.singleMatchMode);
    auto multipleMatch = createResultDF(options.multipleMatchMode);

    // 9. Pré-charger les colonnes probe
    std::vector<IColumnPtr> probeCols;
    for (const auto& keyName : leftKeys) {
        probeCols.push_back(getLeftColumn(keyName));
    }

    // 10. Boucle principale
    for (size_t leftIdx = 0; leftIdx < leftRowCount; ++leftIdx) {
        // Construire la clef
        JoinKey probeKey;
        probeKey.values.reserve(leftKeys.size());
        for (const auto& col : probeCols) {
            probeKey.values.push_back(extractKeyValue(col, leftIdx, leftStringPool, resultPool));
        }

        // Chercher dans la hash table
        auto it = hashTable.find(probeKey);

        // Déterminer le type de match
        DataFramePtr targetDF;
        JoinMode targetMode;
        std::vector<size_t> matchIndices;

        if (it == hashTable.end() || it->second.empty()) {
            targetDF = noMatch;
            targetMode = options.noMatchMode;
            matchIndices.push_back(0);  // dummy
        }
        else if (it->second.size() == 1) {
            targetDF = singleMatch;
            targetMode = options.singleMatchMode;
            matchIndices.push_back(it->second[0]);
        }
        else {
            targetDF = multipleMatch;
            targetMode = options.multipleMatchMode;
            if (targetMode == JoinMode::KeepAll) {
                matchIndices = it->second;
            } else {
                matchIndices.push_back(0);  // Une seule ligne
            }
        }

        // Mode Skip: ne rien écrire du tout
        if (targetMode == JoinMode::Skip) {
            continue;
        }

        // Émettre les lignes
        bool isNoMatch = (it == hashTable.end() || it->second.empty());

        for (size_t rightIdx : matchIndices) {
            for (const auto& rc : resultColumns) {
                // Skip colonnes right si KeepLeftOnly
                if (!rc.fromLeft && !rc.isKey && targetMode == JoinMode::KeepLeftOnly) {
                    continue;
                }

                auto resultCol = targetDF->getColumn(rc.resultName);
                IColumnPtr sourceCol;
                size_t sourceIdx;

                if (rc.isKey || rc.fromLeft) {
                    sourceCol = getLeftColumn(rc.sourceName);
                    sourceIdx = leftIdx;
                } else {
                    sourceCol = getRightColumn(rc.sourceName);
                    sourceIdx = rightIdx;
                }

                // Copier selon le type
                switch (sourceCol->getType()) {
                    case ColumnTypeOpt::INT: {
                        auto dst = std::static_pointer_cast<IntColumn>(resultCol);
                        if (!rc.fromLeft && !rc.isKey && (isNoMatch || targetMode == JoinMode::KeepHeaderOnly)) {
                            dst->push_back(0);
                        } else {
                            auto src = std::static_pointer_cast<IntColumn>(sourceCol);
                            dst->push_back(src->at(sourceIdx));
                        }
                        break;
                    }
                    case ColumnTypeOpt::DOUBLE: {
                        auto dst = std::static_pointer_cast<DoubleColumn>(resultCol);
                        if (!rc.fromLeft && !rc.isKey && (isNoMatch || targetMode == JoinMode::KeepHeaderOnly)) {
                            dst->push_back(0.0);
                        } else {
                            auto src = std::static_pointer_cast<DoubleColumn>(sourceCol);
                            dst->push_back(src->at(sourceIdx));
                        }
                        break;
                    }
                    case ColumnTypeOpt::STRING: {
                        auto dst = std::static_pointer_cast<StringColumn>(resultCol);
                        if (!rc.fromLeft && !rc.isKey && (isNoMatch || targetMode == JoinMode::KeepHeaderOnly)) {
                            dst->push_back("");
                        } else {
                            auto src = std::static_pointer_cast<StringColumn>(sourceCol);
                            dst->push_back(src->at(sourceIdx));
                        }
                        break;
                    }
                }
            }
        }
    }

    return FlexJoinResult{noMatch, singleMatch, multipleMatch};
}

} // namespace dataframe
