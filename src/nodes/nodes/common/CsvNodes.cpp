#include "CsvNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameJoiner.hpp"
#include "dataframe/Column.hpp"
#include <nlohmann/json.hpp>

namespace nodes {

using json = nlohmann::json;

void registerCsvNodes() {
    registerCsvSourceNode();
    registerFieldNode();
    registerJoinFlexNode();
    registerOutputNode();
}

void registerCsvSourceNode() {
    NodeBuilder("csv_source", "data")
        .inputOptional("csv", Type::Csv)
        .output("csv", Type::Csv)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            // Priority 1: connected csv input (passthrough)
            auto inputWl = ctx.getInputWorkload("csv");
            if (!inputWl.isNull() && inputWl.getCsv()) {
                ctx.setOutput("csv", inputWl.getCsv());
                return;
            }

            // Priority 2: test data (fallback)
            auto df = std::make_shared<dataframe::DataFrame>();

            df->addIntColumn("id");
            df->addStringColumn("name");
            df->addDoubleColumn("price");

            df->addRow({"1", "Apple", "1.50"});
            df->addRow({"2", "Banana", "0.75"});
            df->addRow({"3", "Orange", "2.00"});
            df->addRow({"4", "Grape", "3.50"});

            ctx.setOutput("csv", df);
        })
        .buildAndRegister();
}

void registerFieldNode() {
    NodeBuilder("field", "csv")
        .input("csv", Type::Csv)
        .output("field", Type::Field)
        .output("csv", Type::Csv)  // Pass-through
        .onCompile([](NodeContext& ctx) {
            // Get CSV input
            auto csvWL = ctx.getInputWorkload("csv");
            if (csvWL.isNull()) {
                ctx.setError("No CSV input");
                return;
            }
            auto csv = csvWL.getCsv();

            // Get column name from property
            auto columnProp = ctx.getInputWorkload("_column");
            if (columnProp.isNull()) {
                ctx.setError("No column specified (set _column property)");
                return;
            }

            std::string columnName = columnProp.getString();

            // Verify column exists
            if (!csv->hasColumn(columnName)) {
                ctx.setError("Column not found: " + columnName);
                return;
            }

            // Output field reference (column name with Field type)
            ctx.setOutput("field", Workload(columnName, Type::Field));

            // Pass through CSV
            ctx.setOutput("csv", csv);
        })
        .buildAndRegister();
}

void registerJoinFlexNode() {
    NodeBuilder("join_flex", "csv")
        // CSV inputs first (convention)
        .input("left_csv", Type::Csv)
        .input("right_csv", Type::Csv)
        // Key columns
        .input("left_field", {Type::Field, Type::String})
        .input("right_field", {Type::Field, Type::String})
        // Outputs
        .output("csv_no_match", Type::Csv)
        .output("csv_single_match", Type::Csv)
        .output("csv_multiple_match", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // Get CSVs
            auto leftCsvWL = ctx.getInputWorkload("left_csv");
            auto rightCsvWL = ctx.getInputWorkload("right_csv");

            if (leftCsvWL.isNull()) {
                ctx.setError("No left CSV input");
                return;
            }
            if (rightCsvWL.isNull()) {
                ctx.setError("No right CSV input");
                return;
            }

            auto leftCsv = leftCsvWL.getCsv();
            auto rightCsv = rightCsvWL.getCsv();

            // Build key mappings from inputs + dynamic properties
            json keys = json::array();

            // First pair from inputs
            auto leftKeyWL = ctx.getInputWorkload("left_field");
            auto rightKeyWL = ctx.getInputWorkload("right_field");

            if (leftKeyWL.isNull() || rightKeyWL.isNull()) {
                ctx.setError("Both left_field and right_field are required");
                return;
            }

            std::string leftKey = leftKeyWL.getString();
            std::string rightKey = rightKeyWL.getString();
            keys.push_back({{"left", leftKey}, {"right", rightKey}});

            // Additional pairs from properties _left_field_0, _right_field_0, etc.
            for (int i = 0; i < 100; ++i) {
                auto leftProp = ctx.getInputWorkload("_left_field_" + std::to_string(i));
                auto rightProp = ctx.getInputWorkload("_right_field_" + std::to_string(i));
                if (leftProp.isNull()) break;
                if (rightProp.isNull()) {
                    ctx.setError("Missing _right_field_" + std::to_string(i));
                    return;
                }
                keys.push_back({
                    {"left", leftProp.getString()},
                    {"right", rightProp.getString()}
                });
            }

            json joinSpec = {{"keys", keys}};

            // Helper to parse JoinMode from string
            auto parseJoinMode = [](const std::string& str, dataframe::JoinMode defaultMode) -> dataframe::JoinMode {
                if (str == "yes") return dataframe::JoinMode::KeepAll;
                if (str == "no_but_keep_header") return dataframe::JoinMode::KeepHeaderOnly;
                if (str == "no") return dataframe::JoinMode::KeepLeftOnly;
                if (str == "skip") return dataframe::JoinMode::Skip;
                return defaultMode;
            };

            // Get options from properties
            dataframe::FlexJoinOptions options;

            auto noMatchOpt = ctx.getInputWorkload("_no_match_keep_jointure");
            auto singleMatchOpt = ctx.getInputWorkload("_single_match_keep_jointure");
            auto multiMatchOpt = ctx.getInputWorkload("_double_match_keep_jointure");

            if (!noMatchOpt.isNull()) {
                options.noMatchMode = parseJoinMode(noMatchOpt.getString(), dataframe::JoinMode::KeepHeaderOnly);
            }
            if (!singleMatchOpt.isNull()) {
                options.singleMatchMode = parseJoinMode(singleMatchOpt.getString(), dataframe::JoinMode::KeepAll);
            }
            if (!multiMatchOpt.isNull()) {
                options.multipleMatchMode = parseJoinMode(multiMatchOpt.getString(), dataframe::JoinMode::KeepAll);
            }

            // Execute flex join
            try {
                auto result = dataframe::DataFrameJoiner::flexJoin(
                    joinSpec,
                    options,
                    leftCsv->rowCount(),
                    [&](const std::string& name) { return leftCsv->getColumn(name); },
                    leftCsv->getColumnNames(),
                    leftCsv->getStringPool(),
                    rightCsv->rowCount(),
                    [&](const std::string& name) { return rightCsv->getColumn(name); },
                    rightCsv->getColumnNames(),
                    rightCsv->getStringPool()
                );

                // Set outputs
                ctx.setOutput("csv_no_match", result.noMatch);
                ctx.setOutput("csv_single_match", result.singleMatch);
                ctx.setOutput("csv_multiple_match", result.multipleMatch);
            }
            catch (const std::exception& e) {
                ctx.setError(std::string("Join error: ") + e.what());
            }
        })
        .buildAndRegister();
}

void registerOutputNode() {
    NodeBuilder("output", "data")
        .input("csv", Type::Csv)
        .output("csv", Type::Csv)
        .output("output_name", Type::String)   // Output the resolved name for persistence
        .onCompile([](NodeContext& ctx) {
            auto csvWL = ctx.getInputWorkload("csv");
            if (csvWL.isNull()) {
                ctx.setError("No CSV input");
                return;
            }
            auto csv = csvWL.getCsv();

            // Get the output name from widget property (or connected value which overrides it)
            auto nameWL = ctx.getInputWorkload("_name");
            std::string outputName = nameWL.isNull() ? "" : nameWL.getString();

            // Pass-through the CSV
            ctx.setOutput("csv", csv);
            // Output the resolved name so it can be used for persistence
            ctx.setOutput("output_name", Workload(outputName, Type::String));
        })
        .buildAndRegister();
}

} // namespace nodes
