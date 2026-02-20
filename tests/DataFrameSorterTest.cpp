#include <catch2/catch_test_macros.hpp>
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameSorter.hpp"

using namespace dataframe;

// =============================================================================
// Single Column Sort Tests
// =============================================================================

TEST_CASE("Sort IntColumn ascending", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"30"});
    df.addRow({"10"});
    df.addRow({"50"});
    df.addRow({"20"});

    json orderJson = json::array({{{"column", "value"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("value"));
    REQUIRE(col->at(0) == 10);
    REQUIRE(col->at(1) == 20);
    REQUIRE(col->at(2) == 30);
    REQUIRE(col->at(3) == 50);
}

TEST_CASE("Sort IntColumn descending", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"30"});
    df.addRow({"10"});
    df.addRow({"50"});
    df.addRow({"20"});

    json orderJson = json::array({{{"column", "value"}, {"order", "desc"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("value"));
    REQUIRE(col->at(0) == 50);
    REQUIRE(col->at(1) == 30);
    REQUIRE(col->at(2) == 20);
    REQUIRE(col->at(3) == 10);
}

TEST_CASE("Sort DoubleColumn ascending", "[DataFrameSorter]") {
    DataFrame df;
    df.addDoubleColumn("price");
    df.addRow({"30.5"});
    df.addRow({"10.1"});
    df.addRow({"50.9"});
    df.addRow({"20.2"});

    json orderJson = json::array({{{"column", "price"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<DoubleColumn>(sorted->getColumn("price"));
    REQUIRE(col->at(0) == 10.1);
    REQUIRE(col->at(1) == 20.2);
    REQUIRE(col->at(2) == 30.5);
    REQUIRE(col->at(3) == 50.9);
}

TEST_CASE("Sort DoubleColumn descending", "[DataFrameSorter]") {
    DataFrame df;
    df.addDoubleColumn("price");
    df.addRow({"30.5"});
    df.addRow({"10.1"});
    df.addRow({"50.9"});
    df.addRow({"20.2"});

    json orderJson = json::array({{{"column", "price"}, {"order", "desc"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<DoubleColumn>(sorted->getColumn("price"));
    REQUIRE(col->at(0) == 50.9);
    REQUIRE(col->at(1) == 30.5);
    REQUIRE(col->at(2) == 20.2);
    REQUIRE(col->at(3) == 10.1);
}

TEST_CASE("Sort StringColumn ascending", "[DataFrameSorter]") {
    DataFrame df;
    df.addStringColumn("name");
    df.addRow({"Charlie"});
    df.addRow({"Alice"});
    df.addRow({"David"});
    df.addRow({"Bob"});

    json orderJson = json::array({{{"column", "name"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<StringColumn>(sorted->getColumn("name"));
    // Sorted alphabetically ascending
    REQUIRE(col->at(0) == "Alice");
    REQUIRE(col->at(1) == "Bob");
    REQUIRE(col->at(2) == "Charlie");
    REQUIRE(col->at(3) == "David");
}

TEST_CASE("Sort StringColumn descending", "[DataFrameSorter]") {
    DataFrame df;
    df.addStringColumn("name");
    df.addRow({"Charlie"});
    df.addRow({"Alice"});
    df.addRow({"David"});
    df.addRow({"Bob"});

    json orderJson = json::array({{{"column", "name"}, {"order", "desc"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<StringColumn>(sorted->getColumn("name"));
    // Sorted alphabetically descending
    REQUIRE(col->at(0) == "David");
    REQUIRE(col->at(1) == "Charlie");
    REQUIRE(col->at(2) == "Bob");
    REQUIRE(col->at(3) == "Alice");
}

// =============================================================================
// Multi-Column Sort Tests
// =============================================================================

TEST_CASE("Sort multi-column primary asc secondary desc", "[DataFrameSorter]") {
    DataFrame df;
    df.addStringColumn("dept");
    df.addIntColumn("salary");
    df.addStringColumn("name");

    df.addRow({"Engineering", "80000", "Alice"});
    df.addRow({"Sales", "60000", "Bob"});
    df.addRow({"Engineering", "90000", "Charlie"});
    df.addRow({"Sales", "70000", "David"});
    df.addRow({"Engineering", "80000", "Eve"});

    json orderJson = json::array({
        {{"column", "dept"}, {"order", "asc"}},
        {{"column", "salary"}, {"order", "desc"}}
    });

    auto sorted = df.orderBy(orderJson);

    auto deptCol = std::dynamic_pointer_cast<StringColumn>(sorted->getColumn("dept"));
    auto salaryCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("salary"));

    // Engineering comes first (alphabetically), then within Engineering, sort by salary desc
    REQUIRE(deptCol->at(0) == "Engineering");
    REQUIRE(salaryCol->at(0) == 90000);  // Charlie

    REQUIRE(deptCol->at(1) == "Engineering");
    REQUIRE(salaryCol->at(1) == 80000);  // Alice or Eve

    REQUIRE(deptCol->at(2) == "Engineering");
    REQUIRE(salaryCol->at(2) == 80000);  // Eve or Alice

    REQUIRE(deptCol->at(3) == "Sales");
    REQUIRE(salaryCol->at(3) == 70000);  // David

    REQUIRE(deptCol->at(4) == "Sales");
    REQUIRE(salaryCol->at(4) == 60000);  // Bob
}

TEST_CASE("Sort multi-column with integers", "[DataFrameSorter]") {
    // Use integers for predictable sorting behavior
    DataFrame df;
    df.addIntColumn("group");
    df.addIntColumn("value");
    df.addStringColumn("name");

    df.addRow({"2", "30", "A"});
    df.addRow({"1", "20", "B"});
    df.addRow({"2", "10", "C"});
    df.addRow({"1", "40", "D"});
    df.addRow({"2", "20", "E"});

    json orderJson = json::array({
        {{"column", "group"}, {"order", "asc"}},
        {{"column", "value"}, {"order", "desc"}}
    });

    auto sorted = df.orderBy(orderJson);

    auto groupCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("group"));
    auto valueCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("value"));

    // Group 1 first, sorted by value desc
    REQUIRE(groupCol->at(0) == 1);
    REQUIRE(valueCol->at(0) == 40);

    REQUIRE(groupCol->at(1) == 1);
    REQUIRE(valueCol->at(1) == 20);

    // Group 2, sorted by value desc
    REQUIRE(groupCol->at(2) == 2);
    REQUIRE(valueCol->at(2) == 30);

    REQUIRE(groupCol->at(3) == 2);
    REQUIRE(valueCol->at(3) == 20);

    REQUIRE(groupCol->at(4) == 2);
    REQUIRE(valueCol->at(4) == 10);
}

// =============================================================================
// Stable Sort Tests
// =============================================================================

TEST_CASE("Sort stable preserves order for equal values", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("group");
    df.addStringColumn("item");

    // Items in original order within same group
    df.addRow({"1", "A"});
    df.addRow({"2", "B"});
    df.addRow({"1", "C"});
    df.addRow({"2", "D"});
    df.addRow({"1", "E"});

    json orderJson = json::array({{{"column", "group"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    auto groupCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("group"));
    auto itemCol = std::dynamic_pointer_cast<StringColumn>(sorted->getColumn("item"));

    // Group 1 items should maintain original order: A, C, E
    REQUIRE(groupCol->at(0) == 1);
    REQUIRE(itemCol->at(0) == "A");

    REQUIRE(groupCol->at(1) == 1);
    REQUIRE(itemCol->at(1) == "C");

    REQUIRE(groupCol->at(2) == 1);
    REQUIRE(itemCol->at(2) == "E");

    // Group 2 items should maintain original order: B, D
    REQUIRE(groupCol->at(3) == 2);
    REQUIRE(itemCol->at(3) == "B");

    REQUIRE(groupCol->at(4) == 2);
    REQUIRE(itemCol->at(4) == "D");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Sort empty DataFrame", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");

    json orderJson = json::array({{{"column", "value"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    REQUIRE(sorted->rowCount() == 0);
}

TEST_CASE("Sort single row", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"42"});

    json orderJson = json::array({{{"column", "value"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    REQUIRE(sorted->rowCount() == 1);
    auto col = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("value"));
    REQUIRE(col->at(0) == 42);
}

TEST_CASE("Sort with empty order array", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"30"});
    df.addRow({"10"});
    df.addRow({"20"});

    json orderJson = json::array();

    auto sorted = df.orderBy(orderJson);

    // No sort = original order
    REQUIRE(sorted->rowCount() == 3);
    auto col = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("value"));
    REQUIRE(col->at(0) == 30);
    REQUIRE(col->at(1) == 10);
    REQUIRE(col->at(2) == 20);
}

TEST_CASE("Sort preserves all columns", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("a");
    df.addIntColumn("b");
    df.addIntColumn("c");

    df.addRow({"3", "30", "300"});
    df.addRow({"1", "10", "100"});
    df.addRow({"2", "20", "200"});

    json orderJson = json::array({{{"column", "a"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    REQUIRE(sorted->columnCount() == 3);
    REQUIRE(sorted->hasColumn("a"));
    REQUIRE(sorted->hasColumn("b"));
    REQUIRE(sorted->hasColumn("c"));

    // Check that all columns are properly reordered
    auto aCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("a"));
    auto bCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("b"));
    auto cCol = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("c"));

    REQUIRE(aCol->at(0) == 1);
    REQUIRE(bCol->at(0) == 10);
    REQUIRE(cCol->at(0) == 100);

    REQUIRE(aCol->at(1) == 2);
    REQUIRE(bCol->at(1) == 20);
    REQUIRE(cCol->at(1) == 200);
}

TEST_CASE("Sort does not modify original", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"30"});
    df.addRow({"10"});
    df.addRow({"20"});

    json orderJson = json::array({{{"column", "value"}, {"order", "asc"}}});

    auto sorted = df.orderBy(orderJson);

    // Original should be unchanged
    auto origCol = std::dynamic_pointer_cast<IntColumn>(df.getColumn("value"));
    REQUIRE(origCol->at(0) == 30);
    REQUIRE(origCol->at(1) == 10);
    REQUIRE(origCol->at(2) == 20);
}

TEST_CASE("Sort ascending keyword variations", "[DataFrameSorter]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"30"});
    df.addRow({"10"});

    // Test "ascending" (full word)
    json orderJson = json::array({{{"column", "value"}, {"order", "ascending"}}});

    auto sorted = df.orderBy(orderJson);

    auto col = std::dynamic_pointer_cast<IntColumn>(sorted->getColumn("value"));
    REQUIRE(col->at(0) == 10);
    REQUIRE(col->at(1) == 30);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("Sort nonexistent column throws", "[DataFrameSorter][error]") {
    DataFrame df;
    df.addIntColumn("value");
    df.addRow({"1"});

    json orderJson = json::array({{{"column", "nonexistent"}, {"order", "asc"}}});

    REQUIRE_THROWS(df.orderBy(orderJson));
}
