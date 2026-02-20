#include "SelectNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "dataframe/DataFrame.hpp"
#include <algorithm>

namespace nodes {

void registerSelectNodes() {
    registerSelectByNameNode();
    registerSelectByPosNode();
    registerReorderColumnsNode();
    registerCleanTmpColumnsNode();
    registerRemapByNameNode();
    registerRemapByCsvNode();
}

void registerSelectByNameNode() {
    auto builder = NodeBuilder("select_by_name", "select")
        .input("csv", Type::Csv)
        .input("column", Type::Field);  // First column (required)

    // Additional columns (optional, column_1 to column_99)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("column_" + std::to_string(i), Type::Field);
    }

    builder.output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // Get CSV input
            auto csvWorkload = ctx.getInputWorkload("csv");
            if (csvWorkload.isNull()) {
                ctx.setError("CSV input required");
                return;
            }
            auto csv = csvWorkload.getCsv();
            if (!csv) {
                ctx.setError("Invalid CSV input");
                return;
            }

            // Collect column names to keep
            std::vector<std::string> columnsToKeep;

            // First column (required)
            auto column = ctx.getInputWorkload("column");
            if (!column.isNull()) {
                columnsToKeep.push_back(column.getString());
            }

            // Additional columns (optional, column_1 to column_99)
            for (int i = 1; i <= 99; i++) {
                auto col = ctx.getInputWorkload("column_" + std::to_string(i));
                if (!col.isNull()) {
                    columnsToKeep.push_back(col.getString());
                }
            }

            if (columnsToKeep.empty()) {
                ctx.setError("At least one column input required");
                return;
            }

            // Verify all columns exist in CSV
            auto columnNames = csv->getColumnNames();
            for (const auto& col : columnsToKeep) {
                if (std::find(columnNames.begin(), columnNames.end(), col) == columnNames.end()) {
                    ctx.setError("Column not found: " + col);
                    return;
                }
            }

            // Create result DataFrame with only selected columns
            auto result = std::make_shared<dataframe::DataFrame>();

            for (const auto& colName : columnsToKeep) {
                auto srcCol = csv->getColumn(colName);
                if (srcCol) {
                    result->addColumn(srcCol->clone());
                }
            }

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerSelectByPosNode() {
    auto builder = NodeBuilder("select_by_pos", "select")
        .input("csv", Type::Csv);

    // Column selection inputs (optional, col_0 to col_99)
    for (int i = 0; i <= 99; i++) {
        builder.inputOptional("col_" + std::to_string(i), Type::Bool);
    }

    builder.output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // Get CSV input
            auto csvWorkload = ctx.getInputWorkload("csv");
            if (csvWorkload.isNull()) {
                ctx.setError("CSV input required");
                return;
            }
            auto csv = csvWorkload.getCsv();
            if (!csv) {
                ctx.setError("Invalid CSV input");
                return;
            }

            // Get default behavior from property (default: true = keep all)
            auto defaultProp = ctx.getInputWorkload("_default");
            bool defaultKeep = true;
            if (!defaultProp.isNull()) {
                if (defaultProp.getType() == NodeType::Bool) {
                    defaultKeep = defaultProp.getBool();
                } else if (defaultProp.getType() == NodeType::String) {
                    std::string val = defaultProp.getString();
                    defaultKeep = (val == "true" || val == "True" || val == "1");
                }
            }

            auto columnNames = csv->getColumnNames();
            std::vector<std::string> columnsToKeep;

            // Check each column position
            for (size_t i = 0; i < columnNames.size(); i++) {
                auto colInput = ctx.getInputWorkload("col_" + std::to_string(i));

                bool keep = defaultKeep;
                if (!colInput.isNull()) {
                    keep = colInput.getBool();
                }

                if (keep) {
                    columnsToKeep.push_back(columnNames[i]);
                }
            }

            // Create result DataFrame with only selected columns
            auto result = std::make_shared<dataframe::DataFrame>();

            for (const auto& colName : columnsToKeep) {
                auto srcCol = csv->getColumn(colName);
                if (srcCol) {
                    result->addColumn(srcCol->clone());
                }
            }

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerReorderColumnsNode() {
    auto builder = NodeBuilder("reorder_columns", "select")
        .input("csv", Type::Csv)
        .input("column", Type::Field);  // First column (required)

    // Additional columns (optional, column_1 to column_99)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("column_" + std::to_string(i), Type::Field);
    }

    builder.output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // Get CSV input
            auto csvWorkload = ctx.getInputWorkload("csv");
            if (csvWorkload.isNull()) {
                ctx.setError("CSV input required");
                return;
            }
            auto csv = csvWorkload.getCsv();
            if (!csv) {
                ctx.setError("Invalid CSV input");
                return;
            }

            // Collect column names to put first (in order)
            std::vector<std::string> columnsFirst;

            // First column (required)
            auto column = ctx.getInputWorkload("column");
            if (!column.isNull()) {
                columnsFirst.push_back(column.getString());
            }

            // Additional columns (optional, column_1 to column_99)
            for (int i = 1; i <= 99; i++) {
                auto col = ctx.getInputWorkload("column_" + std::to_string(i));
                if (!col.isNull()) {
                    columnsFirst.push_back(col.getString());
                }
            }

            if (columnsFirst.empty()) {
                ctx.setError("At least one column input required");
                return;
            }

            // Get all column names from CSV
            auto allColumnNames = csv->getColumnNames();

            // Verify all specified columns exist in CSV
            for (const auto& col : columnsFirst) {
                if (std::find(allColumnNames.begin(), allColumnNames.end(), col) == allColumnNames.end()) {
                    ctx.setError("Column not found: " + col);
                    return;
                }
            }

            // Build final column order: specified columns first, then remaining in original order
            std::vector<std::string> finalOrder = columnsFirst;

            // Add remaining columns (those not in columnsFirst) in their original order
            for (const auto& colName : allColumnNames) {
                if (std::find(columnsFirst.begin(), columnsFirst.end(), colName) == columnsFirst.end()) {
                    finalOrder.push_back(colName);
                }
            }

            // Create result DataFrame with reordered columns
            auto result = std::make_shared<dataframe::DataFrame>();

            for (const auto& colName : finalOrder) {
                auto srcCol = csv->getColumn(colName);
                if (srcCol) {
                    result->addColumn(srcCol->clone());
                }
            }

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerCleanTmpColumnsNode() {
    NodeBuilder("clean_tmp_columns", "select")
        .input("csv", Type::Csv)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto csvWorkload = ctx.getInputWorkload("csv");
            if (csvWorkload.isNull()) {
                ctx.setError("CSV input required");
                return;
            }
            auto csv = csvWorkload.getCsv();
            if (!csv) {
                ctx.setError("Invalid CSV input");
                return;
            }

            // Create result DataFrame without _tmp_* columns
            auto result = std::make_shared<dataframe::DataFrame>();
            auto columnNames = csv->getColumnNames();

            for (const auto& colName : columnNames) {
                // Skip columns starting with "_tmp_"
                if (colName.rfind("_tmp_", 0) == 0) {
                    continue;
                }
                auto srcCol = csv->getColumn(colName);
                if (srcCol) {
                    result->addColumn(srcCol->clone());
                }
            }

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerRemapByNameNode() {
    auto builder = NodeBuilder("remap_by_name", "select")
        .input("csv", Type::Csv)
        .input("col", Type::Field)      // First old column name (required)
        .input("dest", Type::Field);    // First new column name (required)

    // Additional pairs (optional, col_1/dest_1 to col_99/dest_99)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("col_" + std::to_string(i), Type::Field);
        builder.inputOptional("dest_" + std::to_string(i), Type::Field);
    }

    builder.output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto csvWorkload = ctx.getInputWorkload("csv");
            if (csvWorkload.isNull()) {
                ctx.setError("CSV input required");
                return;
            }
            auto csv = csvWorkload.getCsv();
            if (!csv) {
                ctx.setError("Invalid CSV input");
                return;
            }

            // Read unmapped mode from widget property
            auto modeProp = ctx.getInputWorkload("_unmapped");
            bool keepUnmapped = true; // default: keep
            if (!modeProp.isNull() && modeProp.getString() == "remove") {
                keepUnmapped = false;
            }

            // Collect rename pairs: old name -> new name
            std::vector<std::pair<std::string, std::string>> renamePairs;

            // First pair (required)
            auto col = ctx.getInputWorkload("col");
            auto dest = ctx.getInputWorkload("dest");
            if (!col.isNull() && !dest.isNull()) {
                renamePairs.emplace_back(col.getString(), dest.getString());
            }

            // Additional pairs
            for (int i = 1; i <= 99; i++) {
                auto c = ctx.getInputWorkload("col_" + std::to_string(i));
                auto d = ctx.getInputWorkload("dest_" + std::to_string(i));
                if (!c.isNull() && !d.isNull()) {
                    renamePairs.emplace_back(c.getString(), d.getString());
                }
            }

            if (renamePairs.empty()) {
                ctx.setError("At least one col/dest pair required");
                return;
            }

            // Build a map for quick lookup
            std::unordered_map<std::string, std::string> renameMap;
            for (const auto& [oldName, newName] : renamePairs) {
                renameMap[oldName] = newName;
            }

            // Verify all source columns exist
            auto columnNames = csv->getColumnNames();
            for (const auto& [oldName, newName] : renamePairs) {
                if (std::find(columnNames.begin(), columnNames.end(), oldName) == columnNames.end()) {
                    ctx.setError("Column not found: " + oldName);
                    return;
                }
            }

            // Build result DataFrame
            auto result = std::make_shared<dataframe::DataFrame>();

            for (const auto& colName : columnNames) {
                auto it = renameMap.find(colName);
                if (it != renameMap.end()) {
                    // Mapped column: clone and rename
                    auto cloned = csv->getColumn(colName)->clone();
                    cloned->setName(it->second);
                    result->addColumn(cloned);
                } else if (keepUnmapped) {
                    // Unmapped column: keep as-is
                    result->addColumn(csv->getColumn(colName)->clone());
                }
                // else: remove unmapped -> skip
            }

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerRemapByCsvNode() {
    NodeBuilder("remap_by_csv", "select")
        .input("csv", Type::Csv)
        .input("mapping", Type::Csv)
        .input("col", Type::Field)
        .input("dest", Type::Field)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto csvWorkload = ctx.getInputWorkload("csv");
            if (csvWorkload.isNull()) {
                ctx.setError("CSV input required");
                return;
            }
            auto csv = csvWorkload.getCsv();
            if (!csv) {
                ctx.setError("Invalid CSV input");
                return;
            }

            auto mappingWorkload = ctx.getInputWorkload("mapping");
            if (mappingWorkload.isNull()) {
                ctx.setError("Mapping CSV input required");
                return;
            }
            auto mapping = mappingWorkload.getCsv();
            if (!mapping) {
                ctx.setError("Invalid mapping CSV input");
                return;
            }

            auto colField = ctx.getInputWorkload("col");
            auto destField = ctx.getInputWorkload("dest");
            if (colField.isNull() || destField.isNull()) {
                ctx.setError("Both 'col' and 'dest' field inputs required");
                return;
            }

            std::string colName = colField.getString();
            std::string destName = destField.getString();

            // Read unmapped mode from widget property
            auto modeProp = ctx.getInputWorkload("_unmapped");
            bool keepUnmapped = true;
            if (!modeProp.isNull() && modeProp.getString() == "remove") {
                keepUnmapped = false;
            }

            // Build rename map from the mapping CSV
            auto mappingHeader = mapping->getColumnNames();
            if (std::find(mappingHeader.begin(), mappingHeader.end(), colName) == mappingHeader.end()) {
                ctx.setError("Column not found in mapping CSV: " + colName);
                return;
            }
            if (std::find(mappingHeader.begin(), mappingHeader.end(), destName) == mappingHeader.end()) {
                ctx.setError("Column not found in mapping CSV: " + destName);
                return;
            }

            auto colColumn = mapping->getColumn(colName);
            auto destColumn = mapping->getColumn(destName);
            size_t mappingRows = mapping->rowCount();

            std::unordered_map<std::string, std::string> renameMap;
            for (size_t i = 0; i < mappingRows; ++i) {
                auto srcCol = std::dynamic_pointer_cast<dataframe::StringColumn>(colColumn);
                auto dstCol = std::dynamic_pointer_cast<dataframe::StringColumn>(destColumn);
                if (srcCol && dstCol) {
                    renameMap[srcCol->at(i)] = dstCol->at(i);
                }
            }

            // Build result DataFrame
            auto columnNames = csv->getColumnNames();
            auto result = std::make_shared<dataframe::DataFrame>();

            for (const auto& cn : columnNames) {
                auto it = renameMap.find(cn);
                if (it != renameMap.end()) {
                    auto cloned = csv->getColumn(cn)->clone();
                    cloned->setName(it->second);
                    result->addColumn(cloned);
                } else if (keepUnmapped) {
                    result->addColumn(csv->getColumn(cn)->clone());
                }
            }

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

} // namespace nodes
