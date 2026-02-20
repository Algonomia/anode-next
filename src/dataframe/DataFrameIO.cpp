#include "DataFrameIO.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

namespace dataframe {

std::shared_ptr<DataFrame> DataFrameIO::readCSV(
    const std::string& filepath,
    char delimiter,
    bool hasHeader
) {
    auto df = std::make_shared<DataFrame>();

    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    std::string line;
    std::vector<std::string> headers;
    std::map<std::string, ColumnTypeOpt> columnTypes;
    bool isFirstDataLine = true;
    size_t lineNumber = 0;

    // Pré-réserver de l'espace dans le string pool
    df->getStringPool()->reserve(10000);

    while (std::getline(file, line)) {
        lineNumber++;

        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        auto fields = parseCSVLine(line, delimiter);
        if (fields.empty()) continue;

        // First line: headers
        if (lineNumber == 1) {
            if (hasHeader) {
                headers = fields;
                continue;
            } else {
                for (size_t i = 0; i < fields.size(); ++i) {
                    headers.push_back("col" + std::to_string(i));
                }
            }
        }

        // Detect types from first data line
        if (isFirstDataLine) {
            for (size_t i = 0; i < headers.size() && i < fields.size(); ++i) {
                ColumnTypeOpt type = detectType(fields[i]);
                columnTypes[headers[i]] = type;

                if (type == ColumnTypeOpt::INT) {
                    df->addIntColumn(headers[i]);
                } else if (type == ColumnTypeOpt::DOUBLE) {
                    df->addDoubleColumn(headers[i]);
                } else {
                    df->addStringColumn(headers[i]);
                }
            }
            isFirstDataLine = false;
        }

        // Add row data
        for (size_t i = 0; i < headers.size(); ++i) {
            const std::string& value = (i < fields.size()) ? fields[i] : "";
            auto col = df->getColumn(headers[i]);

            try {
                if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                    intCol->push_back(value.empty() ? 0 : std::stoi(value));
                } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                    doubleCol->push_back(value.empty() ? 0.0 : std::stod(value));
                } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                    stringCol->push_back(value);
                }
            } catch (const std::exception&) {
                // Fallback to default values on error
                if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                    intCol->push_back(0);
                } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                    doubleCol->push_back(0.0);
                } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                    stringCol->push_back(value);
                }
            }
        }
    }

    file.close();
    return df;
}

void DataFrameIO::writeCSV(
    const DataFrame& df,
    const std::string& filepath,
    char delimiter,
    bool includeHeader
) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + filepath);
    }

    auto columnNames = df.getColumnNames();

    // Headers
    if (includeHeader) {
        bool first = true;
        for (const auto& colName : columnNames) {
            if (!first) file << delimiter;
            file << colName;
            first = false;
        }
        file << "\n";
    }

    // Data
    size_t rows = df.rowCount();
    for (size_t i = 0; i < rows; ++i) {
        bool first = true;
        for (const auto& colName : columnNames) {
            if (!first) file << delimiter;

            auto col = df.getColumn(colName);

            if (auto intCol = std::dynamic_pointer_cast<IntColumn>(col)) {
                file << intCol->at(i);
            } else if (auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col)) {
                file << doubleCol->at(i);
            } else if (auto stringCol = std::dynamic_pointer_cast<StringColumn>(col)) {
                file << stringCol->at(i);
            }

            first = false;
        }
        file << "\n";
    }

    file.close();
}

std::vector<std::string> DataFrameIO::parseCSVLine(
    const std::string& line,
    char delimiter
) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    fields.reserve(20);  // Estimation du nombre de colonnes
    field.reserve(256);  // Estimation de la taille d'un champ

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == delimiter && !inQuotes) {
            // Trim whitespace
            size_t start = field.find_first_not_of(" \t\r\n");
            size_t end = field.find_last_not_of(" \t\r\n");

            if (start == std::string::npos) {
                fields.push_back("");
            } else {
                fields.push_back(field.substr(start, end - start + 1));
            }

            field.clear();
        } else {
            field += c;
        }
    }

    // Last field
    size_t start = field.find_first_not_of(" \t\r\n");
    size_t end = field.find_last_not_of(" \t\r\n");

    if (start == std::string::npos) {
        fields.push_back("");
    } else {
        fields.push_back(field.substr(start, end - start + 1));
    }

    return fields;
}

ColumnTypeOpt DataFrameIO::detectType(const std::string& value) {
    if (value.empty()) {
        return ColumnTypeOpt::STRING;
    }

    // Trim
    std::string trimmed = value;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

    if (trimmed.empty()) {
        return ColumnTypeOpt::STRING;
    }

    // Check for integer/double
    bool isInt = true;
    bool hasDecimal = false;
    size_t startIdx = 0;

    if (trimmed[0] == '-' || trimmed[0] == '+') {
        startIdx = 1;
    }

    if (startIdx >= trimmed.size()) {
        return ColumnTypeOpt::STRING;
    }

    for (size_t i = startIdx; i < trimmed.size(); ++i) {
        char c = trimmed[i];

        if (c == '.' || c == ',') {
            if (hasDecimal) {
                return ColumnTypeOpt::STRING;
            }
            hasDecimal = true;
            isInt = false;
        } else if (!std::isdigit(c)) {
            return ColumnTypeOpt::STRING;
        }
    }

    if (isInt) {
        return ColumnTypeOpt::INT;
    } else if (hasDecimal) {
        return ColumnTypeOpt::DOUBLE;
    }

    return ColumnTypeOpt::STRING;
}

} // namespace dataframe