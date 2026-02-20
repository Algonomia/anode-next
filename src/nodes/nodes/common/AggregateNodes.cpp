#include "AggregateNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "dataframe/DataFrame.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace nodes {

using json = nlohmann::json;

void registerAggregateNodes() {
    registerGroupNode();
    registerPivotNode();
    registerTreeGroupNode();
}

void registerGroupNode() {
    auto builder = NodeBuilder("group", "aggregate")
        .input("csv", Type::Csv)
        .input("field", Type::Field);             // First field (required)

    // Additional fields (optional, field_1 to field_99)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("field_" + std::to_string(i), Type::Field);
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

            // Get aggregation function from property (default: sum)
            auto aggProp = ctx.getInputWorkload("_aggregation");
            std::string aggFunction = aggProp.isNull() ? "sum" : aggProp.getString();

            // Validate aggregation function
            static const std::vector<std::string> validFunctions = {
                "sum", "avg", "min", "max", "first", "count"
            };
            if (std::find(validFunctions.begin(), validFunctions.end(), aggFunction)
                == validFunctions.end()) {
                ctx.setError("Invalid aggregation function: " + aggFunction);
                return;
            }

            // Collect groupBy columns from field inputs
            std::vector<std::string> groupByColumns;

            // First field (required)
            auto field = ctx.getInputWorkload("field");
            if (!field.isNull()) {
                groupByColumns.push_back(field.getString());
            }

            // Additional fields (optional, field_1 to field_99)
            for (int i = 1; i <= 99; i++) {
                auto f = ctx.getInputWorkload("field_" + std::to_string(i));
                if (!f.isNull()) {
                    groupByColumns.push_back(f.getString());
                }
            }

            if (groupByColumns.empty()) {
                ctx.setError("At least one field input required");
                return;
            }

            // Verify all groupBy columns exist in CSV
            auto columnNames = csv->getColumnNames();
            for (const auto& col : groupByColumns) {
                if (std::find(columnNames.begin(), columnNames.end(), col)
                    == columnNames.end()) {
                    ctx.setError("Column not found: " + col);
                    return;
                }
            }

            // Build groupBy JSON spec
            json groupByJson;
            groupByJson["groupBy"] = groupByColumns;
            groupByJson["aggregations"] = json::array();

            // Apply aggregation to all non-group columns
            for (const auto& colName : columnNames) {
                bool isGroupCol = std::find(groupByColumns.begin(),
                    groupByColumns.end(), colName) != groupByColumns.end();
                if (!isGroupCol) {
                    groupByJson["aggregations"].push_back({
                        {"column", colName},
                        {"function", aggFunction},
                        {"alias", colName}
                    });
                }
            }

            // Execute groupBy
            auto result = csv->groupBy(groupByJson);
            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerPivotNode() {
    auto builder = NodeBuilder("pivot", "aggregate")
        .input("csv", Type::Csv)
        .input("pivot_column", Type::Field)
        .input("value_column", Type::Field)
        .inputOptional("index_column", Type::Field);

    // Additional index columns (optional, index_column_1 to index_column_99)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("index_column_" + std::to_string(i), Type::Field);
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

            // Get required columns
            auto pivotCol = ctx.getInputWorkload("pivot_column");
            if (pivotCol.isNull()) {
                ctx.setError("pivot_column input required");
                return;
            }

            auto valueCol = ctx.getInputWorkload("value_column");
            if (valueCol.isNull()) {
                ctx.setError("value_column input required");
                return;
            }

            // Collect index columns (optional)
            std::vector<std::string> indexCols;
            auto indexInput = ctx.getInputWorkload("index_column");
            if (!indexInput.isNull()) {
                indexCols.push_back(indexInput.getString());
            }
            for (int i = 1; i <= 99; i++) {
                auto col = ctx.getInputWorkload("index_column_" + std::to_string(i));
                if (col.isNull()) break;
                indexCols.push_back(col.getString());
            }

            // Get optional prefix property
            auto prefixProp = ctx.getInputWorkload("_prefix");

            // Build pivot JSON spec
            json spec;
            spec["pivotColumn"] = pivotCol.getString();
            spec["valueColumn"] = valueCol.getString();

            if (!indexCols.empty()) {
                spec["indexColumns"] = indexCols;
            }
            if (!prefixProp.isNull()) {
                spec["prefix"] = prefixProp.getString();
            }

            // Execute pivot
            auto result = csv->pivotDf(spec);
            if (!result) {
                ctx.setError("Pivot operation failed");
                return;
            }
            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

void registerTreeGroupNode() {
    auto builder = NodeBuilder("tree_group", "aggregate")
        .input("csv", Type::Csv)
        .input("field", Type::Field);

    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("field_" + std::to_string(i), Type::Field);
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

            // Get aggregation function from property (default: sum)
            auto aggProp = ctx.getInputWorkload("_aggregation");
            std::string aggFunction = aggProp.isNull() ? "sum" : aggProp.getString();

            static const std::vector<std::string> validFunctions = {
                "sum", "avg", "min", "max", "first", "count"
            };
            if (std::find(validFunctions.begin(), validFunctions.end(), aggFunction)
                == validFunctions.end()) {
                ctx.setError("Invalid aggregation function: " + aggFunction);
                return;
            }

            // Collect hierarchy columns from field inputs (ordered root->leaf)
            std::vector<std::string> hierarchyColumns;

            auto field = ctx.getInputWorkload("field");
            if (!field.isNull()) {
                hierarchyColumns.push_back(field.getString());
            }

            for (int i = 1; i <= 99; i++) {
                auto f = ctx.getInputWorkload("field_" + std::to_string(i));
                if (!f.isNull()) {
                    hierarchyColumns.push_back(f.getString());
                }
            }

            if (hierarchyColumns.empty()) {
                ctx.setError("At least one field input required");
                return;
            }

            // Verify all hierarchy columns exist in CSV
            auto columnNames = csv->getColumnNames();
            for (const auto& col : hierarchyColumns) {
                if (std::find(columnNames.begin(), columnNames.end(), col)
                    == columnNames.end()) {
                    ctx.setError("Column not found: " + col);
                    return;
                }
            }

            size_t rowCount = csv->rowCount();

            // Create result DataFrame (zero-copy of existing columns)
            auto result = std::make_shared<dataframe::DataFrame>();
            result->setStringPool(csv->getStringPool());
            for (const auto& colName : columnNames) {
                result->addColumn(csv->getColumn(colName));
            }

            // Build __tree_path column: JSON array string per row
            auto pool = csv->getStringPool();
            auto pathCol = std::make_shared<dataframe::StringColumn>("__tree_path", pool);
            pathCol->reserve(rowCount);

            for (size_t row = 0; row < rowCount; ++row) {
                json pathArray = json::array();
                for (const auto& hierCol : hierarchyColumns) {
                    auto col = csv->getColumn(hierCol);
                    if (auto sc = std::dynamic_pointer_cast<dataframe::StringColumn>(col)) {
                        pathArray.push_back(sc->at(row));
                    } else if (auto ic = std::dynamic_pointer_cast<dataframe::IntColumn>(col)) {
                        pathArray.push_back(std::to_string(ic->at(row)));
                    } else if (auto dc = std::dynamic_pointer_cast<dataframe::DoubleColumn>(col)) {
                        pathArray.push_back(std::to_string(dc->at(row)));
                    }
                }
                pathCol->push_back(pathArray.dump());
            }
            result->addColumn(pathCol);

            // Build __tree_agg column (constant = aggFunction, cheap with string pool)
            auto aggCol = std::make_shared<dataframe::StringColumn>("__tree_agg", pool);
            aggCol->reserve(rowCount);
            for (size_t row = 0; row < rowCount; ++row) {
                aggCol->push_back(aggFunction);
            }
            result->addColumn(aggCol);

            ctx.setOutput("csv", result);
        })
        .buildAndRegister();
}

} // namespace nodes
