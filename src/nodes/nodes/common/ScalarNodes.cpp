#include "ScalarNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/DateTimeUtil.hpp"
#include "dataframe/DataFrame.hpp"
#include "dataframe/Column.hpp"
#include <nlohmann/json.hpp>
#include <ctime>
#include <sstream>
#include <stdexcept>

namespace nodes {

// ============== Registration ==============

void registerScalarNodes() {
    registerIntValueNode();
    registerDoubleValueNode();
    registerStringValueNode();
    registerBoolValueNode();
    registerNullValueNode();
    registerStringAsFieldNode();
    registerStringAsFieldsNode();
    registerDateValueNode();
    registerCurrentDateNode();
    registerScalarsToCsvNode();
    registerCsvValueNode();
}

// ============== Basic scalar nodes ==============

void registerIntValueNode() {
    NodeBuilder("int_value", "scalar")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setOutput("value", int64_t(0));
            } else {
                ctx.setOutput("value", prop.getInt());
            }
        })
        .buildAndRegister();
}

void registerDoubleValueNode() {
    NodeBuilder("double_value", "scalar")
        .output("value", Type::Double)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setOutput("value", 0.0);
            } else {
                ctx.setOutput("value", prop.getDouble());
            }
        })
        .buildAndRegister();
}

void registerStringValueNode() {
    NodeBuilder("string_value", "scalar")
        .output("value", Type::String)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setOutput("value", std::string(""));
            } else {
                ctx.setOutput("value", prop.getString());
            }
        })
        .buildAndRegister();
}

void registerBoolValueNode() {
    NodeBuilder("bool_value", "scalar")
        .output("value", Type::Bool)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setOutput("value", false);
            } else {
                ctx.setOutput("value", prop.getBool());
            }
        })
        .buildAndRegister();
}

void registerNullValueNode() {
    NodeBuilder("null_value", "scalar")
        .output("value", Type::Null)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("value", Workload());  // Null workload
        })
        .buildAndRegister();
}

// ============== New scalar nodes ==============

void registerStringAsFieldNode() {
    NodeBuilder("string_as_field", "scalar")
        .output("value", Type::Field)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setError("No field name provided");
                return;
            }
            ctx.setOutput("value", Workload(prop.getString(), NodeType::Field));
        })
        .buildAndRegister();
}

void registerStringAsFieldsNode() {
    using json = nlohmann::json;

    NodeBuilder("string_as_fields", "scalar")
        .output("value", Type::Field)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setError("No field names provided");
                return;
            }

            // Parse JSON array: ["col_a", "col_b", "col_c"]
            std::string valStr = prop.getString();
            nlohmann::json arr = nlohmann::json::parse(valStr, nullptr, false);
            if (!arr.is_array() || arr.empty()) {
                ctx.setError("Invalid field list (expected JSON array)");
                return;
            }

            // Set first field as "value"
            ctx.setOutput("value", Workload(arr[0].get<std::string>(), NodeType::Field));

            // Set additional fields as "value_1", "value_2", etc.
            for (size_t i = 1; i < arr.size() && i <= 99; i++) {
                ctx.setOutput("value_" + std::to_string(i),
                              Workload(arr[i].get<std::string>(), NodeType::Field));
            }
        })
        .buildAndRegister();
}

void registerDateValueNode() {
    NodeBuilder("date_value", "scalar")
        .output("value", Type::Int)  // timestamp as int
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (prop.isNull()) {
                ctx.setError("No date provided");
                return;
            }

            try {
                int64_t timestamp = convertDateToTimestamp(prop.getString());
                ctx.setOutput("value", timestamp);
            }
            catch (const std::exception& e) {
                ctx.setError(std::string("Date format error: ") + e.what() +
                             ". Use dd/mm/yyyy or dd month yyyy (e.g., 14/04/1990 or 14 avril 1990)");
            }
        })
        .buildAndRegister();
}

void registerCurrentDateNode() {
    NodeBuilder("current_date", "scalar")
        .output("year", Type::Int)
        .output("month", Type::Int)
        .output("day", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto yearOffsetProp = ctx.getInputWorkload("_year_offset");
            auto monthOffsetProp = ctx.getInputWorkload("_month_offset");
            auto dayOffsetProp = ctx.getInputWorkload("_day_offset");

            int yearOffset = yearOffsetProp.isNull() ? 0 : static_cast<int>(yearOffsetProp.getInt());
            int monthOffset = monthOffsetProp.isNull() ? 0 : static_cast<int>(monthOffsetProp.getInt());
            int dayOffset = dayOffsetProp.isNull() ? 0 : static_cast<int>(dayOffsetProp.getInt());

            // Get current time
            std::time_t now = std::time(nullptr);
            std::tm tm = *std::localtime(&now);

            // Apply offsets
            tm.tm_year += yearOffset;
            tm.tm_mon += monthOffset;
            tm.tm_mday += dayOffset;

            // Normalize the date (handles month/year overflow)
            std::mktime(&tm);

            ctx.setOutput("year", int64_t(tm.tm_year + 1900));
            ctx.setOutput("month", int64_t(tm.tm_mon + 1));  // tm_mon is 0-based
            ctx.setOutput("day", int64_t(tm.tm_mday));
        })
        .buildAndRegister();
}

void registerScalarsToCsvNode() {
    auto builder = NodeBuilder("scalars_to_csv", "scalar")
        .inputOptional("field", {Type::Field, Type::String})
        .inputOptional("value", {Type::Int, Type::Double, Type::String, Type::Bool, Type::Null});

    // Additional pairs (field_1/value_1 to field_99/value_99)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("field_" + std::to_string(i), {Type::Field, Type::String});
        builder.inputOptional("value_" + std::to_string(i), {Type::Int, Type::Double, Type::String, Type::Bool, Type::Null});
    }

    builder.output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            std::vector<std::string> headers;
            std::vector<Workload> values;

            // First pair from inputs
            auto field = ctx.getInputWorkload("field");
            auto value = ctx.getInputWorkload("value");
            if (!field.isNull()) {
                headers.push_back(field.getString());
                values.push_back(value);
            }

            // Additional pairs from dynamic inputs
            for (int i = 1; i <= 99; ++i) {
                auto fieldN = ctx.getInputWorkload("field_" + std::to_string(i));
                if (fieldN.isNull()) break;
                auto valueN = ctx.getInputWorkload("value_" + std::to_string(i));
                headers.push_back(fieldN.getString());
                values.push_back(valueN);
            }

            if (headers.empty()) {
                ctx.setError("No field/value pair provided");
                return;
            }

            // Build DataFrame with 1 row
            auto df = std::make_shared<dataframe::DataFrame>();
            auto stringPool = df->getStringPool();

            for (size_t i = 0; i < headers.size(); ++i) {
                const auto& wl = values[i];
                const auto& name = headers[i];

                if (wl.isNull()) {
                    auto col = std::make_shared<dataframe::StringColumn>(name, stringPool);
                    col->push_back("");
                    df->addColumn(col);
                }
                else {
                    switch (wl.getType()) {
                        case NodeType::Int: {
                            auto col = std::make_shared<dataframe::IntColumn>(name);
                            col->push_back(static_cast<int>(wl.getInt()));
                            df->addColumn(col);
                            break;
                        }
                        case NodeType::Double: {
                            auto col = std::make_shared<dataframe::DoubleColumn>(name);
                            col->push_back(wl.getDouble());
                            df->addColumn(col);
                            break;
                        }
                        case NodeType::String:
                        case NodeType::Field: {
                            auto col = std::make_shared<dataframe::StringColumn>(name, stringPool);
                            col->push_back(wl.getString());
                            df->addColumn(col);
                            break;
                        }
                        case NodeType::Bool: {
                            auto col = std::make_shared<dataframe::IntColumn>(name);
                            col->push_back(wl.getBool() ? 1 : 0);
                            df->addColumn(col);
                            break;
                        }
                        default: {
                            auto col = std::make_shared<dataframe::StringColumn>(name, stringPool);
                            col->push_back("");
                            df->addColumn(col);
                            break;
                        }
                    }
                }
            }

            ctx.setOutput("csv", df);
        })
        .buildAndRegister();
}

void registerCsvValueNode() {
    NodeBuilder("csv_value", "scalar")
        .output("csv", Type::Csv)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            if (!prop.isNull() && prop.getType() == NodeType::Csv) {
                auto csv = prop.getCsv();
                if (csv) {
                    ctx.setOutput("csv", csv);
                    return;
                }
            }
            // Fallback: empty DataFrame
            ctx.setOutput("csv", std::make_shared<dataframe::DataFrame>());
        })
        .buildAndRegister();
}

} // namespace nodes
