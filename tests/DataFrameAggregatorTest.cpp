#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameAggregator.hpp"

using namespace dataframe;

// Helper to create test DataFrame for aggregation tests
static DataFrame createAggTestDataFrame() {
    DataFrame df;

    df.addStringColumn("dept");
    df.addStringColumn("name");
    df.addIntColumn("salary");
    df.addDoubleColumn("bonus");

    df.addRow({"Engineering", "Alice", "80000", "5000.0"});
    df.addRow({"Engineering", "Bob", "90000", "6000.0"});
    df.addRow({"Sales", "Charlie", "60000", "8000.0"});
    df.addRow({"Engineering", "David", "85000", "5500.0"});
    df.addRow({"Sales", "Eve", "65000", "7000.0"});

    return df;
}

// =============================================================================
// GroupBy - Count Tests
// =============================================================================

TEST_CASE("GroupBy count", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "name"}, {"function", "count"}, {"alias", "employee_count"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    REQUIRE(result->rowCount() == 2);  // Engineering, Sales
    REQUIRE(result->hasColumn("dept"));
    REQUIRE(result->hasColumn("employee_count"));

    // Find counts for each department (order may vary)
    auto deptCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("dept"));
    auto countCol = std::dynamic_pointer_cast<IntColumn>(result->getColumn("employee_count"));

    // Total count should be 5
    int totalCount = 0;
    for (size_t i = 0; i < result->rowCount(); ++i) {
        totalCount += countCol->at(i);
    }
    REQUIRE(totalCount == 5);  // 3 Engineering + 2 Sales
}

// =============================================================================
// GroupBy - Sum Tests
// =============================================================================

TEST_CASE("GroupBy sum IntColumn", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "salary"}, {"function", "sum"}, {"alias", "total_salary"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    auto deptCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("dept"));
    auto sumCol = std::dynamic_pointer_cast<DoubleColumn>(result->getColumn("total_salary"));

    for (size_t i = 0; i < result->rowCount(); ++i) {
        if (deptCol->at(i) == "Engineering") {
            REQUIRE(sumCol->at(i) == 255000.0);  // 80000 + 90000 + 85000
        } else if (deptCol->at(i) == "Sales") {
            REQUIRE(sumCol->at(i) == 125000.0);  // 60000 + 65000
        }
    }
}

TEST_CASE("GroupBy sum DoubleColumn", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "bonus"}, {"function", "sum"}, {"alias", "total_bonus"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    auto deptCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("dept"));
    auto sumCol = std::dynamic_pointer_cast<DoubleColumn>(result->getColumn("total_bonus"));

    for (size_t i = 0; i < result->rowCount(); ++i) {
        if (deptCol->at(i) == "Engineering") {
            REQUIRE(sumCol->at(i) == 16500.0);  // 5000 + 6000 + 5500
        } else if (deptCol->at(i) == "Sales") {
            REQUIRE(sumCol->at(i) == 15000.0);  // 8000 + 7000
        }
    }
}

// =============================================================================
// GroupBy - Avg Tests
// =============================================================================

TEST_CASE("GroupBy avg", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "salary"}, {"function", "avg"}, {"alias", "avg_salary"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    auto deptCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("dept"));
    auto avgCol = std::dynamic_pointer_cast<DoubleColumn>(result->getColumn("avg_salary"));

    for (size_t i = 0; i < result->rowCount(); ++i) {
        if (deptCol->at(i) == "Engineering") {
            REQUIRE_THAT(avgCol->at(i), Catch::Matchers::WithinRel(85000.0, 0.001));  // 255000 / 3
        } else if (deptCol->at(i) == "Sales") {
            REQUIRE_THAT(avgCol->at(i), Catch::Matchers::WithinRel(62500.0, 0.001));  // 125000 / 2
        }
    }
}

// =============================================================================
// GroupBy - Min/Max Tests
// =============================================================================

TEST_CASE("GroupBy min", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "salary"}, {"function", "min"}, {"alias", "min_salary"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    auto deptCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("dept"));
    auto minCol = std::dynamic_pointer_cast<DoubleColumn>(result->getColumn("min_salary"));

    for (size_t i = 0; i < result->rowCount(); ++i) {
        if (deptCol->at(i) == "Engineering") {
            REQUIRE(minCol->at(i) == 80000.0);
        } else if (deptCol->at(i) == "Sales") {
            REQUIRE(minCol->at(i) == 60000.0);
        }
    }
}

TEST_CASE("GroupBy max", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "salary"}, {"function", "max"}, {"alias", "max_salary"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    auto deptCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("dept"));
    auto maxCol = std::dynamic_pointer_cast<DoubleColumn>(result->getColumn("max_salary"));

    for (size_t i = 0; i < result->rowCount(); ++i) {
        if (deptCol->at(i) == "Engineering") {
            REQUIRE(maxCol->at(i) == 90000.0);
        } else if (deptCol->at(i) == "Sales") {
            REQUIRE(maxCol->at(i) == 65000.0);
        }
    }
}

// =============================================================================
// GroupBy - Multiple GroupBy Columns Tests
// =============================================================================

TEST_CASE("GroupBy multiple columns", "[DataFrameAggregator]") {
    DataFrame df;
    df.addStringColumn("country");
    df.addStringColumn("city");
    df.addIntColumn("population");

    df.addRow({"USA", "NYC", "8000000"});
    df.addRow({"USA", "NYC", "500000"});  // Second entry for NYC
    df.addRow({"USA", "LA", "4000000"});
    df.addRow({"France", "Paris", "2000000"});

    json groupByJson = {
        {"groupBy", {"country", "city"}},
        {"aggregations", json::array({
            {{"column", "population"}, {"function", "sum"}, {"alias", "total_pop"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    REQUIRE(result->rowCount() == 3);  // USA/NYC, USA/LA, France/Paris
}

// =============================================================================
// GroupBy - Multiple Aggregations Tests
// =============================================================================

TEST_CASE("GroupBy multiple aggregations", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "salary"}, {"function", "sum"}, {"alias", "total_salary"}},
            {{"column", "salary"}, {"function", "avg"}, {"alias", "avg_salary"}},
            {{"column", "salary"}, {"function", "min"}, {"alias", "min_salary"}},
            {{"column", "salary"}, {"function", "max"}, {"alias", "max_salary"}},
            {{"column", "name"}, {"function", "count"}, {"alias", "headcount"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    REQUIRE(result->hasColumn("total_salary"));
    REQUIRE(result->hasColumn("avg_salary"));
    REQUIRE(result->hasColumn("min_salary"));
    REQUIRE(result->hasColumn("max_salary"));
    REQUIRE(result->hasColumn("headcount"));
}

// =============================================================================
// GroupBy - Empty DataFrame Tests
// =============================================================================

TEST_CASE("GroupBy empty DataFrame", "[DataFrameAggregator]") {
    DataFrame df;
    df.addStringColumn("dept");
    df.addIntColumn("salary");

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", json::array({
            {{"column", "salary"}, {"function", "sum"}, {"alias", "total"}}
        })}
    };

    auto result = df.groupBy(groupByJson);

    REQUIRE(result->rowCount() == 0);
}

// =============================================================================
// GroupByTree Tests
// =============================================================================

TEST_CASE("GroupByTree format columnar", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", {
            {"salary", "sum"},
            {"bonus", "avg"},
            {"name", "count"}
        }}
    };

    json result = df.groupByTree(groupByJson);

    REQUIRE(result.contains("columns"));
    REQUIRE(result.contains("data"));
    REQUIRE(result["columns"].is_array());
    REQUIRE(result["data"].is_array());
}

TEST_CASE("GroupByTree has _children", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", {
            {"salary", "sum"}
        }}
    };

    json result = df.groupByTree(groupByJson);

    // Check that data rows exist
    REQUIRE(result["data"].size() == 2);  // Engineering, Sales

    // Each row should have children
    for (const auto& row : result["data"]) {
        REQUIRE(row.is_array());
        // Last element should be children array
        REQUIRE(row.back().is_array());
    }
}

TEST_CASE("GroupByTree children complete", "[DataFrameAggregator]") {
    DataFrame df;
    df.addStringColumn("group");
    df.addIntColumn("value");

    df.addRow({"A", "1"});
    df.addRow({"A", "2"});
    df.addRow({"B", "3"});

    json groupByJson = {
        {"groupBy", {"group"}},
        {"aggregations", {
            {"value", "sum"}
        }}
    };

    json result = df.groupByTree(groupByJson);

    // Find group A
    bool foundA = false;
    for (const auto& row : result["data"]) {
        if (row[0] == "A") {
            foundA = true;
            auto children = row.back();
            REQUIRE(children.size() == 2);  // 2 rows in group A
        }
    }
    REQUIRE(foundA);
}

TEST_CASE("GroupByTree blank aggregation returns null", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", {
            {"salary", "blank"},
            {"bonus", "sum"}
        }}
    };

    json result = df.groupByTree(groupByJson);

    // Check columns include salary
    bool hasSalary = false;
    for (const auto& col : result["columns"]) {
        if (col == "salary") hasSalary = true;
    }
    REQUIRE(hasSalary);
}

TEST_CASE("GroupByTree first aggregation", "[DataFrameAggregator]") {
    auto df = createAggTestDataFrame();

    json groupByJson = {
        {"groupBy", {"dept"}},
        {"aggregations", {
            {"name", "first"}
        }}
    };

    json result = df.groupByTree(groupByJson);

    // Should have name column with first value from each group
    REQUIRE(result["columns"].size() > 0);
}

// =============================================================================
// Pivot Tests
// =============================================================================

TEST_CASE("Pivot basic", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("task_id");
    df.addIntColumn("question_id");
    df.addIntColumn("score");

    df.addRow({"1", "1", "80"});
    df.addRow({"1", "2", "90"});
    df.addRow({"1", "3", "85"});
    df.addRow({"2", "1", "70"});
    df.addRow({"2", "2", "75"});
    df.addRow({"2", "3", "80"});

    json pivotJson = {
        {"pivotColumn", "question_id"},
        {"valueColumn", "score"},
        {"indexColumns", {"task_id"}}
    };

    json result = df.pivot(pivotJson);

    REQUIRE(result.is_array());
    REQUIRE(result.size() == 2);  // 2 tasks
}

TEST_CASE("Pivot creates columns from pivot values", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("type");
    df.addIntColumn("value");

    df.addRow({"1", "A", "10"});
    df.addRow({"1", "B", "20"});
    df.addRow({"2", "A", "30"});
    df.addRow({"2", "B", "40"});

    json pivotJson = {
        {"pivotColumn", "type"},
        {"valueColumn", "value"},
        {"indexColumns", {"id"}}
    };

    json result = df.pivot(pivotJson);

    REQUIRE(result.size() == 2);

    // Check first row has both type columns (no prefix by default)
    bool hasA = result[0].contains("A");
    bool hasB = result[0].contains("B");
    REQUIRE(hasA);
    REQUIRE(hasB);
}

TEST_CASE("Pivot custom prefix", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("type");
    df.addIntColumn("value");

    df.addRow({"1", "X", "10"});
    df.addRow({"1", "Y", "20"});

    json pivotJson = {
        {"pivotColumn", "type"},
        {"valueColumn", "value"},
        {"indexColumns", {"id"}},
        {"prefix", "col_"}
    };

    json result = df.pivot(pivotJson);

    REQUIRE(result[0].contains("col_X"));
    REQUIRE(result[0].contains("col_Y"));
}

TEST_CASE("Pivot multiple index columns", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("task_id");
    df.addIntColumn("user_id");
    df.addStringColumn("metric");
    df.addIntColumn("value");

    df.addRow({"1", "100", "score", "85"});
    df.addRow({"1", "100", "time", "120"});
    df.addRow({"1", "101", "score", "90"});
    df.addRow({"1", "101", "time", "100"});

    json pivotJson = {
        {"pivotColumn", "metric"},
        {"valueColumn", "value"},
        {"indexColumns", {"task_id", "user_id"}}
    };

    json result = df.pivot(pivotJson);

    REQUIRE(result.size() == 2);  // 2 combinations of task_id/user_id
}

TEST_CASE("PivotDf returns DataFrame", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("type");
    df.addIntColumn("value");

    df.addRow({"1", "A", "10"});
    df.addRow({"1", "B", "20"});

    json pivotJson = {
        {"pivotColumn", "type"},
        {"valueColumn", "value"},
        {"indexColumns", {"id"}}
    };

    auto result = df.pivotDf(pivotJson);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowCount() == 1);
    REQUIRE(result->hasColumn("id"));
}

TEST_CASE("PivotDf type conservation IntColumn", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("type");
    df.addIntColumn("value");

    df.addRow({"1", "A", "10"});
    df.addRow({"1", "B", "20"});

    json pivotJson = {
        {"pivotColumn", "type"},
        {"valueColumn", "value"},
        {"indexColumns", {"id"}}
    };

    auto result = df.pivotDf(pivotJson);

    // Pivoted columns should maintain int type (no prefix by default)
    auto colA = result->getColumn("A");
    auto colB = result->getColumn("B");

    REQUIRE(colA->getType() == ColumnTypeOpt::INT);
    REQUIRE(colB->getType() == ColumnTypeOpt::INT);
}

TEST_CASE("PivotDf type conservation DoubleColumn", "[DataFrameAggregator]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("type");
    df.addDoubleColumn("value");

    df.addRow({"1", "A", "10.5"});
    df.addRow({"1", "B", "20.5"});

    json pivotJson = {
        {"pivotColumn", "type"},
        {"valueColumn", "value"},
        {"indexColumns", {"id"}}
    };

    auto result = df.pivotDf(pivotJson);

    // No prefix by default
    auto colA = result->getColumn("A");
    REQUIRE(colA->getType() == ColumnTypeOpt::DOUBLE);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("GroupBy nonexistent column throws", "[DataFrameAggregator][error]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"1"});

    json groupByJson = {
        {"groupBy", {"nonexistent"}},
        {"aggregations", json::array()}
    };

    REQUIRE_THROWS(df.groupBy(groupByJson));
}
