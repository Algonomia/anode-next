#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameFilter.hpp"

using namespace dataframe;
using Catch::Matchers::Equals;

// Helper to create test DataFrame
static DataFrame createTestDataFrame() {
    DataFrame df;

    df.addIntColumn("id");
    df.addDoubleColumn("price");
    df.addStringColumn("name");

    df.addRow({"1", "10.5", "Alice"});
    df.addRow({"2", "20.5", "Bob"});
    df.addRow({"3", "15.0", "Charlie"});
    df.addRow({"4", "20.5", "Alice"});
    df.addRow({"5", "30.0", "David"});

    return df;
}

// =============================================================================
// Equality Operator Tests
// =============================================================================

TEST_CASE("Filter == IntColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "=="}, {"value", 3}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 1);

    auto idCol = std::dynamic_pointer_cast<IntColumn>(filtered->getColumn("id"));
    REQUIRE(idCol->at(0) == 3);
}

TEST_CASE("Filter == DoubleColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "price"}, {"operator", "=="}, {"value", 20.5}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);
}

TEST_CASE("Filter == StringColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "name"}, {"operator", "=="}, {"value", "Alice"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);

    auto nameCol = std::dynamic_pointer_cast<StringColumn>(filtered->getColumn("name"));
    REQUIRE(nameCol->at(0) == "Alice");
    REQUIRE(nameCol->at(1) == "Alice");
}

// =============================================================================
// Not Equal Operator Tests
// =============================================================================

TEST_CASE("Filter != IntColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "!="}, {"value", 1}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 4);
}

TEST_CASE("Filter != DoubleColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "price"}, {"operator", "!="}, {"value", 20.5}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 3);
}

TEST_CASE("Filter != StringColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "name"}, {"operator", "!="}, {"value", "Alice"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 3);
}

// =============================================================================
// Less Than Operator Tests
// =============================================================================

TEST_CASE("Filter < IntColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "<"}, {"value", 3}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);

    auto idCol = std::dynamic_pointer_cast<IntColumn>(filtered->getColumn("id"));
    REQUIRE(idCol->at(0) == 1);
    REQUIRE(idCol->at(1) == 2);
}

TEST_CASE("Filter < DoubleColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "price"}, {"operator", "<"}, {"value", 20.0}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);
}

TEST_CASE("Filter < StringColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "name"}, {"operator", "<"}, {"value", "Charlie"}}});

    auto filtered = df.filter(filterJson);

    // Alice, Bob < Charlie (alphabetically)
    REQUIRE(filtered->rowCount() == 3);  // Alice, Bob, Alice
}

// =============================================================================
// Less Or Equal Operator Tests
// =============================================================================

TEST_CASE("Filter <= IntColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "<="}, {"value", 3}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 3);
}

TEST_CASE("Filter <= DoubleColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "price"}, {"operator", "<="}, {"value", 20.5}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 4);
}

TEST_CASE("Filter <= StringColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "name"}, {"operator", "<="}, {"value", "Bob"}}});

    auto filtered = df.filter(filterJson);

    // Alice, Alice, Bob <= Bob
    REQUIRE(filtered->rowCount() == 3);
}

// =============================================================================
// Greater Than Operator Tests
// =============================================================================

TEST_CASE("Filter > IntColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", ">"}, {"value", 3}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);

    auto idCol = std::dynamic_pointer_cast<IntColumn>(filtered->getColumn("id"));
    REQUIRE(idCol->at(0) == 4);
    REQUIRE(idCol->at(1) == 5);
}

TEST_CASE("Filter > DoubleColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "price"}, {"operator", ">"}, {"value", 20.0}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 3);  // 20.5, 20.5, 30.0
}

TEST_CASE("Filter > StringColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "name"}, {"operator", ">"}, {"value", "Bob"}}});

    auto filtered = df.filter(filterJson);

    // Charlie, David > Bob
    REQUIRE(filtered->rowCount() == 2);
}

// =============================================================================
// Greater Or Equal Operator Tests
// =============================================================================

TEST_CASE("Filter >= IntColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", ">="}, {"value", 3}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 3);
}

TEST_CASE("Filter >= DoubleColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "price"}, {"operator", ">="}, {"value", 20.5}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 3);  // 20.5, 20.5, 30.0
}

TEST_CASE("Filter >= StringColumn", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "name"}, {"operator", ">="}, {"value", "Charlie"}}});

    auto filtered = df.filter(filterJson);

    // Charlie, David >= Charlie
    REQUIRE(filtered->rowCount() == 2);
}

// =============================================================================
// Contains Operator Tests
// =============================================================================

TEST_CASE("Filter contains match", "[DataFrameFilter]") {
    DataFrame df;
    df.addStringColumn("text");
    df.addRow({"hello world"});
    df.addRow({"goodbye"});
    df.addRow({"hello there"});

    json filterJson = json::array({{{"column", "text"}, {"operator", "contains"}, {"value", "hello"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);
}

TEST_CASE("Filter contains no match", "[DataFrameFilter]") {
    DataFrame df;
    df.addStringColumn("text");
    df.addRow({"apple"});
    df.addRow({"banana"});

    json filterJson = json::array({{{"column", "text"}, {"operator", "contains"}, {"value", "xyz"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 0);
}

TEST_CASE("Filter contains at beginning", "[DataFrameFilter]") {
    DataFrame df;
    df.addStringColumn("text");
    df.addRow({"prefix_value"});
    df.addRow({"no_match"});

    json filterJson = json::array({{{"column", "text"}, {"operator", "contains"}, {"value", "prefix"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 1);
}

TEST_CASE("Filter contains in middle", "[DataFrameFilter]") {
    DataFrame df;
    df.addStringColumn("text");
    df.addRow({"start_middle_end"});
    df.addRow({"no_match"});

    json filterJson = json::array({{{"column", "text"}, {"operator", "contains"}, {"value", "middle"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 1);
}

TEST_CASE("Filter contains at end", "[DataFrameFilter]") {
    DataFrame df;
    df.addStringColumn("text");
    df.addRow({"value_suffix"});
    df.addRow({"no_match"});

    json filterJson = json::array({{{"column", "text"}, {"operator", "contains"}, {"value", "suffix"}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 1);
}

// =============================================================================
// Multiple Conditions (AND) Tests
// =============================================================================

TEST_CASE("Filter multiple conditions AND", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({
        {{"column", "name"}, {"operator", "=="}, {"value", "Alice"}},
        {{"column", "id"}, {"operator", ">"}, {"value", 1}}
    });

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 1);

    auto idCol = std::dynamic_pointer_cast<IntColumn>(filtered->getColumn("id"));
    REQUIRE(idCol->at(0) == 4);
}

TEST_CASE("Filter three conditions AND", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({
        {{"column", "id"}, {"operator", ">="}, {"value", 2}},
        {{"column", "id"}, {"operator", "<="}, {"value", 4}},
        {{"column", "price"}, {"operator", ">"}, {"value", 15.0}}
    });

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 2);  // id=2 (20.5), id=4 (20.5)
}

TEST_CASE("Filter incompatible conditions returns empty", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({
        {{"column", "id"}, {"operator", "=="}, {"value", 1}},
        {{"column", "id"}, {"operator", "=="}, {"value", 2}}
    });

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 0);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Filter no match returns empty", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "=="}, {"value", 999}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 0);
    REQUIRE(filtered->empty());
}

TEST_CASE("Filter all match", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", ">="}, {"value", 1}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 5);
}

TEST_CASE("Filter empty DataFrame", "[DataFrameFilter]") {
    DataFrame df;
    df.addIntColumn("value");

    json filterJson = json::array({{{"column", "value"}, {"operator", "=="}, {"value", 1}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->rowCount() == 0);
}

TEST_CASE("Filter with empty filter array", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array();

    auto filtered = df.filter(filterJson);

    // No filters = return all rows
    REQUIRE(filtered->rowCount() == 5);
}

TEST_CASE("Filter preserves all columns", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "=="}, {"value", 1}}});

    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->columnCount() == 3);
    REQUIRE(filtered->hasColumn("id"));
    REQUIRE(filtered->hasColumn("price"));
    REQUIRE(filtered->hasColumn("name"));
}

TEST_CASE("Filter preserves column order", "[DataFrameFilter]") {
    auto df = createTestDataFrame();

    json filterJson = json::array({{{"column", "id"}, {"operator", "=="}, {"value", 1}}});

    auto filtered = df.filter(filterJson);
    auto names = filtered->getColumnNames();

    REQUIRE(names[0] == "id");
    REQUIRE(names[1] == "price");
    REQUIRE(names[2] == "name");
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("Filter nonexistent column throws", "[DataFrameFilter][error]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"1"});

    json filterJson = json::array({{{"column", "nonexistent"}, {"operator", "=="}, {"value", 1}}});

    REQUIRE_THROWS(df.filter(filterJson));
}

TEST_CASE("Filter invalid operator returns empty", "[DataFrameFilter][error]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"1"});

    json filterJson = json::array({{{"column", "value"}, {"operator", "LIKE"}, {"value", 1}}});

    auto filtered = df.filter(filterJson);
    REQUIRE(filtered->rowCount() == 0);
}
