#pragma once

#include "DataFrame.hpp"
#include <string>
#include <memory>

namespace dataframe {

/**
 * IO pour DataFrame
 */
class DataFrameIO {
public:
    /**
     * Charge un CSV dans un DataFrame
     * Détecte automatiquement les types de colonnes
     */
    static std::shared_ptr<DataFrame> readCSV(
        const std::string& filepath,
        char delimiter = ',',
        bool hasHeader = true
    );

    /**
     * Sauvegarde un DataFrame en CSV
     */
    static void writeCSV(
        const DataFrame& df,
        const std::string& filepath,
        char delimiter = ',',
        bool includeHeader = true
    );

private:
    static std::vector<std::string> parseCSVLine(
        const std::string& line,
        char delimiter
    );

    static ColumnTypeOpt detectType(const std::string& value);
};

} // namespace dataframe