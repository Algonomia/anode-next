#include <catch2/catch_test_macros.hpp>
#include "dataframe/DataFrame.hpp"

using namespace dataframe;

TEST_CASE("DataFrame empty", "[DataFrame]") {
    DataFrame df;

    REQUIRE(df.rowCount() == 0);
    REQUIRE(df.columnCount() == 0);
    REQUIRE(df.empty());
    REQUIRE(df.getColumnNames().empty());
}

TEST_CASE("DataFrame addIntColumn", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("age");

    REQUIRE(df.columnCount() == 1);
    REQUIRE(df.hasColumn("age"));

    auto col = df.getColumn("age");
    REQUIRE(col->getType() == ColumnTypeOpt::INT);
    REQUIRE(col->getName() == "age");
}

TEST_CASE("DataFrame addDoubleColumn", "[DataFrame]") {
    DataFrame df;

    df.addDoubleColumn("salary");

    REQUIRE(df.columnCount() == 1);
    REQUIRE(df.hasColumn("salary"));

    auto col = df.getColumn("salary");
    REQUIRE(col->getType() == ColumnTypeOpt::DOUBLE);
}

TEST_CASE("DataFrame addStringColumn", "[DataFrame]") {
    DataFrame df;

    df.addStringColumn("name");

    REQUIRE(df.columnCount() == 1);
    REQUIRE(df.hasColumn("name"));

    auto col = df.getColumn("name");
    REQUIRE(col->getType() == ColumnTypeOpt::STRING);
}

TEST_CASE("DataFrame addColumn with IColumnPtr", "[DataFrame]") {
    DataFrame df;

    auto intCol = std::make_shared<IntColumn>("custom_int");
    intCol->push_back(42);

    df.addColumn(intCol);

    REQUIRE(df.hasColumn("custom_int"));
    auto retrieved = df.getColumn("custom_int");
    auto* intPtr = dynamic_cast<IntColumn*>(retrieved.get());
    REQUIRE(intPtr->at(0) == 42);
}

TEST_CASE("DataFrame addColumn duplicate throws", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("col1");

    REQUIRE_THROWS(df.addIntColumn("col1"));
}

TEST_CASE("DataFrame getColumn existing", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("age");

    auto col = df.getColumn("age");
    REQUIRE(col != nullptr);
    REQUIRE(col->getName() == "age");
}

TEST_CASE("DataFrame getColumn missing throws", "[DataFrame]") {
    DataFrame df;

    REQUIRE_THROWS(df.getColumn("nonexistent"));
}

TEST_CASE("DataFrame hasColumn", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("exists");

    REQUIRE(df.hasColumn("exists"));
    REQUIRE_FALSE(df.hasColumn("does_not_exist"));
}

TEST_CASE("DataFrame getColumnNames order", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("first");
    df.addDoubleColumn("second");
    df.addStringColumn("third");

    auto names = df.getColumnNames();

    REQUIRE(names.size() == 3);
    REQUIRE(names[0] == "first");
    REQUIRE(names[1] == "second");
    REQUIRE(names[2] == "third");
}

TEST_CASE("DataFrame addRow", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("id");
    df.addDoubleColumn("value");
    df.addStringColumn("name");

    df.addRow({"1", "10.5", "Alice"});
    df.addRow({"2", "20.5", "Bob"});

    REQUIRE(df.rowCount() == 2);
    REQUIRE_FALSE(df.empty());

    auto idCol = std::dynamic_pointer_cast<IntColumn>(df.getColumn("id"));
    auto valueCol = std::dynamic_pointer_cast<DoubleColumn>(df.getColumn("value"));
    auto nameCol = std::dynamic_pointer_cast<StringColumn>(df.getColumn("name"));

    REQUIRE(idCol->at(0) == 1);
    REQUIRE(idCol->at(1) == 2);
    REQUIRE(valueCol->at(0) == 10.5);
    REQUIRE(valueCol->at(1) == 20.5);
    REQUIRE(nameCol->at(0) == "Alice");
    REQUIRE(nameCol->at(1) == "Bob");
}

TEST_CASE("DataFrame addRow wrong count throws", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("a");
    df.addIntColumn("b");

    REQUIRE_THROWS(df.addRow({"1"}));  // Too few
    REQUIRE_THROWS(df.addRow({"1", "2", "3"}));  // Too many
}

TEST_CASE("DataFrame addRow type parsing", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("int_col");
    df.addDoubleColumn("double_col");
    df.addStringColumn("string_col");

    df.addRow({"42", "3.14", "hello"});

    auto intCol = std::dynamic_pointer_cast<IntColumn>(df.getColumn("int_col"));
    auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(df.getColumn("double_col"));
    auto stringCol = std::dynamic_pointer_cast<StringColumn>(df.getColumn("string_col"));

    REQUIRE(intCol->at(0) == 42);
    REQUIRE(doubleCol->at(0) == 3.14);
    REQUIRE(stringCol->at(0) == "hello");
}

TEST_CASE("DataFrame select", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("a");
    df.addIntColumn("b");
    df.addIntColumn("c");

    df.addRow({"1", "2", "3"});

    auto selected = df.select({"a", "c"});

    REQUIRE(selected->columnCount() == 2);
    REQUIRE(selected->hasColumn("a"));
    REQUIRE(selected->hasColumn("c"));
    REQUIRE_FALSE(selected->hasColumn("b"));
}

TEST_CASE("DataFrame select order", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("first");
    df.addIntColumn("second");
    df.addIntColumn("third");

    df.addRow({"1", "2", "3"});

    // Select in different order
    auto selected = df.select({"third", "first"});
    auto names = selected->getColumnNames();

    REQUIRE(names.size() == 2);
    REQUIRE(names[0] == "third");
    REQUIRE(names[1] == "first");
}

TEST_CASE("DataFrame rowCount with data", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("col");

    REQUIRE(df.rowCount() == 0);

    df.addRow({"1"});
    REQUIRE(df.rowCount() == 1);

    df.addRow({"2"});
    df.addRow({"3"});
    REQUIRE(df.rowCount() == 3);
}

TEST_CASE("DataFrame getStringPool", "[DataFrame]") {
    DataFrame df;

    auto pool = df.getStringPool();
    REQUIRE(pool != nullptr);

    df.addStringColumn("name");
    df.addRow({"test"});

    // The string should be in the pool
    REQUIRE(pool->size() >= 1);
}

TEST_CASE("DataFrame setStringPool", "[DataFrame]") {
    auto customPool = std::make_shared<StringPool>();
    customPool->intern("preexisting");

    DataFrame df;
    df.setStringPool(customPool);

    REQUIRE(df.getStringPool() == customPool);
    REQUIRE(df.getStringPool()->size() == 1);
}

TEST_CASE("DataFrame chaining filter and orderBy", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("id");
    df.addStringColumn("name");

    df.addRow({"1", "Charlie"});
    df.addRow({"2", "Alice"});
    df.addRow({"3", "Bob"});
    df.addRow({"4", "Alice"});

    // Filter then sort
    json filterJson = json::array({{{"column", "name"}, {"operator", "!="}, {"value", "Charlie"}}});
    json orderJson = json::array({{{"column", "name"}, {"order", "asc"}}});

    auto filtered = df.filter(filterJson);
    auto sorted = filtered->orderBy(orderJson);

    REQUIRE(sorted->rowCount() == 3);

    auto nameCol = std::dynamic_pointer_cast<StringColumn>(sorted->getColumn("name"));
    REQUIRE(nameCol->at(0) == "Alice");
    REQUIRE(nameCol->at(1) == "Alice");
    REQUIRE(nameCol->at(2) == "Bob");
}

TEST_CASE("DataFrame filter returns new DataFrame", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("value");
    df.addRow({"10"});
    df.addRow({"20"});
    df.addRow({"30"});

    json filterJson = json::array({{{"column", "value"}, {"operator", ">="}, {"value", 20}}});

    auto filtered = df.filter(filterJson);

    // Original unchanged
    REQUIRE(df.rowCount() == 3);

    // Filtered has less rows
    REQUIRE(filtered->rowCount() == 2);
}

TEST_CASE("DataFrame empty after operations", "[DataFrame]") {
    DataFrame df;

    df.addIntColumn("value");
    df.addRow({"10"});

    REQUIRE_FALSE(df.empty());

    json filterJson = json::array({{{"column", "value"}, {"operator", "=="}, {"value", 999}}});
    auto filtered = df.filter(filterJson);

    REQUIRE(filtered->empty());
    REQUIRE(filtered->rowCount() == 0);
}
