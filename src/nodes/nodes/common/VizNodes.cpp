#include "VizNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "dataframe/DataFrame.hpp"
#include "dataframe/Column.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace nodes {

using json = nlohmann::json;
using namespace dataframe;

void registerVizNodes() {
    registerTimelineOutputNode();
    registerDiffOutputNode();
    registerBarChartOutputNode();
}

void registerTimelineOutputNode() {
    NodeBuilder("timeline_output", "viz")
        .input("csv", Type::Csv)
        .input("start_date", Type::Field)
        .input("name", Type::Field)
        .inputOptional("end_date", Type::Field)
        .inputOptional("parent", Type::Field)
        .inputOptional("color", {Type::Field, Type::String})
        .inputOptional("event", {Type::Field, Type::String})
        .output("csv", Type::Csv)
        .output("output_name", Type::String)
        .output("output_type", Type::String)
        .output("output_metadata", Type::String)
        .onCompile([](NodeContext& ctx) {
            // Validate required inputs
            auto csvWL = ctx.getInputWorkload("csv");
            if (csvWL.isNull()) {
                ctx.setError("No CSV input");
                return;
            }
            auto csv = csvWL.getCsv();

            auto startDateWL = ctx.getInputWorkload("start_date");
            if (startDateWL.isNull()) {
                ctx.setError("start_date field is required");
                return;
            }
            std::string startDateCol = startDateWL.getString();

            auto nameWL = ctx.getInputWorkload("name");
            if (nameWL.isNull()) {
                ctx.setError("name field is required");
                return;
            }
            std::string nameCol = nameWL.getString();

            // Optional inputs
            auto endDateWL = ctx.getInputWorkload("end_date");
            std::string endDateCol = endDateWL.isNull() ? "" : endDateWL.getString();

            auto parentWL = ctx.getInputWorkload("parent");
            std::string parentCol = parentWL.isNull() ? "" : parentWL.getString();

            auto colorWL = ctx.getInputWorkload("color");
            std::string colorVal;
            bool colorIsField = false;
            if (!colorWL.isNull()) {
                colorVal = colorWL.getString();
                colorIsField = (colorWL.getType() == NodeType::Field);
            }

            auto eventWL = ctx.getInputWorkload("event");
            std::string eventVal;
            bool eventIsField = false;
            if (!eventWL.isNull()) {
                eventVal = eventWL.getString();
                eventIsField = (eventWL.getType() == NodeType::Field);
            }

            // Timeline name from widget property
            auto timelineNameWL = ctx.getInputWorkload("_timeline_name");
            std::string outputName = timelineNameWL.isNull() ? "" : timelineNameWL.getString();

            // Build metadata JSON
            json metadata;
            metadata["start_date"] = startDateCol;
            metadata["name"] = nameCol;
            if (!endDateCol.empty()) {
                metadata["end_date"] = endDateCol;
            }
            if (!parentCol.empty()) {
                metadata["parent"] = parentCol;
            }
            if (!colorVal.empty()) {
                metadata["color"] = colorVal;
                metadata["color_is_field"] = colorIsField;
            }
            if (!eventVal.empty()) {
                metadata["event"] = eventVal;
                metadata["event_is_field"] = eventIsField;
            }

            // Set outputs
            ctx.setOutput("csv", csv);
            ctx.setOutput("output_name", Workload(outputName, Type::String));
            ctx.setOutput("output_type", Workload(std::string("timeline"), Type::String));
            ctx.setOutput("output_metadata", Workload(metadata.dump(), Type::String));
        })
        .buildAndRegister();
}

void registerDiffOutputNode() {
    NodeBuilder("diff_output", "viz")
        .input("left", Type::Csv)
        .input("right", Type::Csv)
        .inputOptional("key", Type::Field)
        .output("csv", Type::Csv)
        .output("output_name", Type::String)
        .output("output_type", Type::String)
        .output("output_metadata", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto leftWL = ctx.getInputWorkload("left");
            if (leftWL.isNull()) {
                ctx.setError("No left CSV input");
                return;
            }
            auto rightWL = ctx.getInputWorkload("right");
            if (rightWL.isNull()) {
                ctx.setError("No right CSV input");
                return;
            }

            auto leftDf = leftWL.getCsv();
            auto rightDf = rightWL.getCsv();

            auto keyWL = ctx.getInputWorkload("key");
            std::string keyCol = keyWL.isNull() ? "" : keyWL.getString();

            auto diffNameWL = ctx.getInputWorkload("_diff_name");
            std::string outputName = diffNameWL.isNull() ? "" : diffNameWL.getString();

            // Gather column names from both CSVs (union, preserving order)
            auto leftCols = leftDf->getColumnNames();
            auto rightCols = rightDf->getColumnNames();

            std::vector<std::string> allCols;
            std::unordered_map<std::string, bool> seen;
            for (const auto& c : rightCols) {
                if (!seen.count(c)) { allCols.push_back(c); seen[c] = true; }
            }
            for (const auto& c : leftCols) {
                if (!seen.count(c)) { allCols.push_back(c); seen[c] = true; }
            }

            // Build output DataFrame
            auto pool = std::make_shared<StringPool>();
            auto result = std::make_shared<DataFrame>();
            result->setStringPool(pool);

            // __diff__ column
            auto diffCol = std::make_shared<StringColumn>("__diff__", pool);
            result->addColumn(diffCol);

            // Right-side columns (regular names)
            std::unordered_map<std::string, std::shared_ptr<IColumn>> rightOutCols;
            for (const auto& name : allCols) {
                // Determine type: prefer right, fallback to left
                ColumnTypeOpt colType = ColumnTypeOpt::STRING;
                if (rightDf->hasColumn(name)) {
                    colType = rightDf->getColumn(name)->getType();
                } else if (leftDf->hasColumn(name)) {
                    colType = leftDf->getColumn(name)->getType();
                }

                std::shared_ptr<IColumn> col;
                switch (colType) {
                    case ColumnTypeOpt::INT:
                        col = std::make_shared<IntColumn>(name);
                        break;
                    case ColumnTypeOpt::DOUBLE:
                        col = std::make_shared<DoubleColumn>(name);
                        break;
                    default:
                        col = std::make_shared<StringColumn>(name, pool);
                        break;
                }
                result->addColumn(col);
                rightOutCols[name] = col;
            }

            // __old_* columns (left-side values)
            std::unordered_map<std::string, std::shared_ptr<IColumn>> oldOutCols;
            for (const auto& name : allCols) {
                std::string oldName = "__old_" + name;
                ColumnTypeOpt colType = ColumnTypeOpt::STRING;
                if (leftDf->hasColumn(name)) {
                    colType = leftDf->getColumn(name)->getType();
                } else if (rightDf->hasColumn(name)) {
                    colType = rightDf->getColumn(name)->getType();
                }

                std::shared_ptr<IColumn> col;
                switch (colType) {
                    case ColumnTypeOpt::INT:
                        col = std::make_shared<IntColumn>(oldName);
                        break;
                    case ColumnTypeOpt::DOUBLE:
                        col = std::make_shared<DoubleColumn>(oldName);
                        break;
                    default:
                        col = std::make_shared<StringColumn>(oldName, pool);
                        break;
                }
                result->addColumn(col);
                oldOutCols[name] = col;
            }

            // __changed_* columns (int 0/1)
            std::unordered_map<std::string, std::shared_ptr<IntColumn>> changedCols;
            for (const auto& name : allCols) {
                auto col = std::make_shared<IntColumn>("__changed_" + name);
                result->addColumn(col);
                changedCols[name] = col;
            }

            // Helper lambdas to read cell values generically
            auto pushRightValue = [&](const std::string& colName, size_t row, bool hasValue) {
                auto& outCol = rightOutCols[colName];
                if (!hasValue || !rightDf->hasColumn(colName)) {
                    // Push default
                    switch (outCol->getType()) {
                        case ColumnTypeOpt::INT:
                            std::dynamic_pointer_cast<IntColumn>(outCol)->push_back(0);
                            break;
                        case ColumnTypeOpt::DOUBLE:
                            std::dynamic_pointer_cast<DoubleColumn>(outCol)->push_back(0.0);
                            break;
                        default:
                            std::dynamic_pointer_cast<StringColumn>(outCol)->push_back("");
                            break;
                    }
                    return;
                }
                auto srcCol = rightDf->getColumn(colName);
                switch (srcCol->getType()) {
                    case ColumnTypeOpt::INT:
                        std::dynamic_pointer_cast<IntColumn>(outCol)->push_back(
                            std::dynamic_pointer_cast<IntColumn>(srcCol)->at(row));
                        break;
                    case ColumnTypeOpt::DOUBLE:
                        std::dynamic_pointer_cast<DoubleColumn>(outCol)->push_back(
                            std::dynamic_pointer_cast<DoubleColumn>(srcCol)->at(row));
                        break;
                    default:
                        std::dynamic_pointer_cast<StringColumn>(outCol)->push_back(
                            std::dynamic_pointer_cast<StringColumn>(srcCol)->at(row));
                        break;
                }
            };

            auto pushOldValue = [&](const std::string& colName, size_t row, bool hasValue) {
                auto& outCol = oldOutCols[colName];
                if (!hasValue || !leftDf->hasColumn(colName)) {
                    switch (outCol->getType()) {
                        case ColumnTypeOpt::INT:
                            std::dynamic_pointer_cast<IntColumn>(outCol)->push_back(0);
                            break;
                        case ColumnTypeOpt::DOUBLE:
                            std::dynamic_pointer_cast<DoubleColumn>(outCol)->push_back(0.0);
                            break;
                        default:
                            std::dynamic_pointer_cast<StringColumn>(outCol)->push_back("");
                            break;
                    }
                    return;
                }
                auto srcCol = leftDf->getColumn(colName);
                switch (srcCol->getType()) {
                    case ColumnTypeOpt::INT:
                        std::dynamic_pointer_cast<IntColumn>(outCol)->push_back(
                            std::dynamic_pointer_cast<IntColumn>(srcCol)->at(row));
                        break;
                    case ColumnTypeOpt::DOUBLE:
                        std::dynamic_pointer_cast<DoubleColumn>(outCol)->push_back(
                            std::dynamic_pointer_cast<DoubleColumn>(srcCol)->at(row));
                        break;
                    default:
                        std::dynamic_pointer_cast<StringColumn>(outCol)->push_back(
                            std::dynamic_pointer_cast<StringColumn>(srcCol)->at(row));
                        break;
                }
            };

            // Compare a cell between left row and right row for a given column
            auto cellsEqual = [&](const std::string& colName, size_t leftRow, size_t rightRow) -> bool {
                bool leftHas = leftDf->hasColumn(colName);
                bool rightHas = rightDf->hasColumn(colName);
                if (!leftHas && !rightHas) return true;
                if (!leftHas || !rightHas) return false;

                auto lCol = leftDf->getColumn(colName);
                auto rCol = rightDf->getColumn(colName);

                // Compare by type
                if (lCol->getType() == ColumnTypeOpt::INT && rCol->getType() == ColumnTypeOpt::INT) {
                    return std::dynamic_pointer_cast<IntColumn>(lCol)->at(leftRow) ==
                           std::dynamic_pointer_cast<IntColumn>(rCol)->at(rightRow);
                }
                if (lCol->getType() == ColumnTypeOpt::DOUBLE && rCol->getType() == ColumnTypeOpt::DOUBLE) {
                    return std::dynamic_pointer_cast<DoubleColumn>(lCol)->at(leftRow) ==
                           std::dynamic_pointer_cast<DoubleColumn>(rCol)->at(rightRow);
                }
                if (lCol->getType() == ColumnTypeOpt::STRING && rCol->getType() == ColumnTypeOpt::STRING) {
                    return std::dynamic_pointer_cast<StringColumn>(lCol)->at(leftRow) ==
                           std::dynamic_pointer_cast<StringColumn>(rCol)->at(rightRow);
                }
                // Mixed types: not equal
                return false;
            };

            // Match rows
            struct MatchedRow {
                enum Status { Removed, Modified, Added, Unchanged };
                Status status;
                size_t leftIdx;  // valid for Removed, Modified, Unchanged
                size_t rightIdx; // valid for Added, Modified, Unchanged
                std::vector<bool> changedFlags; // per-column, for Modified rows
            };

            std::vector<MatchedRow> rows;
            int statsAdded = 0, statsRemoved = 0, statsModified = 0, statsUnchanged = 0;

            size_t leftRows = leftDf->rowCount();
            size_t rightRows = rightDf->rowCount();

            if (keyCol.empty()) {
                // Positional matching
                size_t commonRows = std::min(leftRows, rightRows);

                for (size_t i = 0; i < commonRows; ++i) {
                    // Check if any cell differs
                    std::vector<bool> changed(allCols.size(), false);
                    bool anyChanged = false;
                    for (size_t c = 0; c < allCols.size(); ++c) {
                        if (!cellsEqual(allCols[c], i, i)) {
                            changed[c] = true;
                            anyChanged = true;
                        }
                    }
                    MatchedRow mr;
                    mr.leftIdx = i;
                    mr.rightIdx = i;
                    mr.changedFlags = std::move(changed);
                    if (anyChanged) {
                        mr.status = MatchedRow::Modified;
                        statsModified++;
                    } else {
                        mr.status = MatchedRow::Unchanged;
                        statsUnchanged++;
                    }
                    rows.push_back(std::move(mr));
                }

                // Extra left rows = removed
                for (size_t i = commonRows; i < leftRows; ++i) {
                    MatchedRow mr;
                    mr.status = MatchedRow::Removed;
                    mr.leftIdx = i;
                    mr.rightIdx = 0;
                    mr.changedFlags.assign(allCols.size(), false);
                    rows.push_back(std::move(mr));
                    statsRemoved++;
                }

                // Extra right rows = added
                for (size_t i = commonRows; i < rightRows; ++i) {
                    MatchedRow mr;
                    mr.status = MatchedRow::Added;
                    mr.leftIdx = 0;
                    mr.rightIdx = i;
                    mr.changedFlags.assign(allCols.size(), false);
                    rows.push_back(std::move(mr));
                    statsAdded++;
                }
            } else {
                // Key-based matching
                // Build key-to-row index for left
                std::unordered_map<std::string, size_t> leftKeyMap;
                if (leftDf->hasColumn(keyCol)) {
                    auto lkCol = leftDf->getColumn(keyCol);
                    for (size_t i = 0; i < leftRows; ++i) {
                        std::string key;
                        switch (lkCol->getType()) {
                            case ColumnTypeOpt::INT:
                                key = std::to_string(std::dynamic_pointer_cast<IntColumn>(lkCol)->at(i));
                                break;
                            case ColumnTypeOpt::DOUBLE:
                                key = std::to_string(std::dynamic_pointer_cast<DoubleColumn>(lkCol)->at(i));
                                break;
                            default:
                                key = std::dynamic_pointer_cast<StringColumn>(lkCol)->at(i);
                                break;
                        }
                        leftKeyMap[key] = i;
                    }
                }

                // Iterate right rows, match against left
                std::unordered_map<std::string, bool> matchedLeftKeys;

                if (rightDf->hasColumn(keyCol)) {
                    auto rkCol = rightDf->getColumn(keyCol);
                    for (size_t ri = 0; ri < rightRows; ++ri) {
                        std::string key;
                        switch (rkCol->getType()) {
                            case ColumnTypeOpt::INT:
                                key = std::to_string(std::dynamic_pointer_cast<IntColumn>(rkCol)->at(ri));
                                break;
                            case ColumnTypeOpt::DOUBLE:
                                key = std::to_string(std::dynamic_pointer_cast<DoubleColumn>(rkCol)->at(ri));
                                break;
                            default:
                                key = std::dynamic_pointer_cast<StringColumn>(rkCol)->at(ri);
                                break;
                        }

                        auto it = leftKeyMap.find(key);
                        if (it != leftKeyMap.end()) {
                            size_t li = it->second;
                            matchedLeftKeys[key] = true;

                            std::vector<bool> changed(allCols.size(), false);
                            bool anyChanged = false;
                            for (size_t c = 0; c < allCols.size(); ++c) {
                                if (!cellsEqual(allCols[c], li, ri)) {
                                    changed[c] = true;
                                    anyChanged = true;
                                }
                            }
                            MatchedRow mr;
                            mr.leftIdx = li;
                            mr.rightIdx = ri;
                            mr.changedFlags = std::move(changed);
                            if (anyChanged) {
                                mr.status = MatchedRow::Modified;
                                statsModified++;
                            } else {
                                mr.status = MatchedRow::Unchanged;
                                statsUnchanged++;
                            }
                            rows.push_back(std::move(mr));
                        } else {
                            // Added
                            MatchedRow mr;
                            mr.status = MatchedRow::Added;
                            mr.leftIdx = 0;
                            mr.rightIdx = ri;
                            mr.changedFlags.assign(allCols.size(), false);
                            rows.push_back(std::move(mr));
                            statsAdded++;
                        }
                    }
                }

                // Unmatched left rows = removed
                for (const auto& [key, li] : leftKeyMap) {
                    if (!matchedLeftKeys.count(key)) {
                        MatchedRow mr;
                        mr.status = MatchedRow::Removed;
                        mr.leftIdx = li;
                        mr.rightIdx = 0;
                        mr.changedFlags.assign(allCols.size(), false);
                        rows.push_back(std::move(mr));
                        statsRemoved++;
                    }
                }
            }

            // Sort: removed, modified, added, unchanged
            std::stable_sort(rows.begin(), rows.end(), [](const MatchedRow& a, const MatchedRow& b) {
                return static_cast<int>(a.status) < static_cast<int>(b.status);
            });

            // Emit rows to output DataFrame
            for (const auto& mr : rows) {
                // __diff__
                const char* statusStr = "";
                switch (mr.status) {
                    case MatchedRow::Removed:   statusStr = "removed"; break;
                    case MatchedRow::Modified:   statusStr = "modified"; break;
                    case MatchedRow::Added:      statusStr = "added"; break;
                    case MatchedRow::Unchanged:  statusStr = "unchanged"; break;
                }
                diffCol->push_back(statusStr);

                // Right-side values (current)
                bool hasRight = (mr.status != MatchedRow::Removed);
                for (const auto& colName : allCols) {
                    pushRightValue(colName, mr.rightIdx, hasRight);
                }

                // Old-side values (before)
                bool hasLeft = (mr.status != MatchedRow::Added);
                for (const auto& colName : allCols) {
                    pushOldValue(colName, mr.leftIdx, hasLeft);
                }

                // Changed flags
                for (size_t c = 0; c < allCols.size(); ++c) {
                    changedCols[allCols[c]]->push_back(mr.changedFlags[c] ? 1 : 0);
                }
            }

            // Build metadata
            json metadata;
            metadata["left_columns"] = leftCols;
            metadata["right_columns"] = rightCols;
            metadata["all_columns"] = allCols;
            if (!keyCol.empty()) {
                metadata["key_column"] = keyCol;
            }
            metadata["stats"] = {
                {"added", statsAdded},
                {"removed", statsRemoved},
                {"modified", statsModified},
                {"unchanged", statsUnchanged}
            };

            ctx.setOutput("csv", result);
            ctx.setOutput("output_name", Workload(outputName, Type::String));
            ctx.setOutput("output_type", Workload(std::string("diff"), Type::String));
            ctx.setOutput("output_metadata", Workload(metadata.dump(), Type::String));
        })
        .buildAndRegister();
}

void registerBarChartOutputNode() {
    NodeBuilder("bar_chart_output", "viz")
        .input("csv", Type::Csv)
        .inputOptional("category", Type::Field)
        .input("value", Type::Field)
        .inputOptional("color", {Type::Field, Type::String})
        .inputOptional("event", {Type::Field, Type::String})
        .output("csv", Type::Csv)
        .output("output_name", Type::String)
        .output("output_type", Type::String)
        .output("output_metadata", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto csvWL = ctx.getInputWorkload("csv");
            if (csvWL.isNull()) {
                ctx.setError("No CSV input");
                return;
            }
            auto csv = csvWL.getCsv();

            auto categoryWL = ctx.getInputWorkload("category");
            std::string categoryCol = categoryWL.isNull() ? "" : categoryWL.getString();

            auto valueWL = ctx.getInputWorkload("value");
            if (valueWL.isNull()) {
                ctx.setError("value field is required");
                return;
            }
            std::string valueCol = valueWL.getString();

            // Detect tree mode from __tree_path column
            bool treeMode = csv->hasColumn("__tree_path");
            std::string treeAgg = "sum";
            if (treeMode && csv->hasColumn("__tree_agg")) {
                auto aggCol = std::dynamic_pointer_cast<StringColumn>(csv->getColumn("__tree_agg"));
                if (aggCol && aggCol->size() > 0) {
                    treeAgg = aggCol->at(0);
                }
            }

            // Validate: need category OR tree_mode
            if (categoryCol.empty() && !treeMode) {
                ctx.setError("category field is required (or connect tree_group output)");
                return;
            }

            auto colorWL = ctx.getInputWorkload("color");
            std::string colorVal;
            bool colorIsField = false;
            if (!colorWL.isNull()) {
                colorVal = colorWL.getString();
                colorIsField = (colorWL.getType() == NodeType::Field);
            }

            auto eventWL = ctx.getInputWorkload("event");
            std::string eventVal;
            bool eventIsField = false;
            if (!eventWL.isNull()) {
                eventVal = eventWL.getString();
                eventIsField = (eventWL.getType() == NodeType::Field);
            }

            auto chartNameWL = ctx.getInputWorkload("_chart_name");
            std::string outputName = chartNameWL.isNull() ? "" : chartNameWL.getString();

            json metadata;
            metadata["chart_type"] = "bar";
            if (!categoryCol.empty()) {
                metadata["category"] = categoryCol;
            }
            metadata["value"] = valueCol;
            if (treeMode) {
                metadata["tree_mode"] = true;
                metadata["tree_agg"] = treeAgg;
            }
            if (!colorVal.empty()) {
                metadata["color"] = colorVal;
                metadata["color_is_field"] = colorIsField;
            }
            if (!eventVal.empty()) {
                metadata["event"] = eventVal;
                metadata["event_is_field"] = eventIsField;
            }

            ctx.setOutput("csv", csv);
            ctx.setOutput("output_name", Workload(outputName, Type::String));
            ctx.setOutput("output_type", Workload(std::string("chart"), Type::String));
            ctx.setOutput("output_metadata", Workload(metadata.dump(), Type::String));
        })
        .buildAndRegister();
}

} // namespace nodes
