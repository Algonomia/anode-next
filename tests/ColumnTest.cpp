#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include "dataframe/Column.hpp"

using namespace dataframe;
using Catch::Matchers::Equals;

// =============================================================================
// IntColumn Tests
// =============================================================================

TEST_CASE("IntColumn push_back and at", "[IntColumn]") {
    IntColumn col("numbers");

    col.push_back(10);
    col.push_back(20);
    col.push_back(30);

    REQUIRE(col.at(0) == 10);
    REQUIRE(col.at(1) == 20);
    REQUIRE(col.at(2) == 30);
}

TEST_CASE("IntColumn set", "[IntColumn]") {
    IntColumn col("numbers");

    col.push_back(1);
    col.push_back(2);

    col.set(0, 100);
    col.set(1, 200);

    REQUIRE(col.at(0) == 100);
    REQUIRE(col.at(1) == 200);
}

TEST_CASE("IntColumn size", "[IntColumn]") {
    IntColumn col("numbers");

    REQUIRE(col.size() == 0);

    col.push_back(1);
    REQUIRE(col.size() == 1);

    col.push_back(2);
    col.push_back(3);
    REQUIRE(col.size() == 3);
}

TEST_CASE("IntColumn getName and getType", "[IntColumn]") {
    IntColumn col("my_column");

    REQUIRE(col.getName() == "my_column");
    REQUIRE(col.getType() == ColumnTypeOpt::INT);
}

TEST_CASE("IntColumn clone", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(1);
    col.push_back(2);
    col.push_back(3);

    auto cloned = col.clone();

    REQUIRE(cloned->size() == 3);
    REQUIRE(cloned->getName() == "numbers");

    // Modify original, clone should be independent
    col.set(0, 999);
    auto* intCloned = dynamic_cast<IntColumn*>(cloned.get());
    REQUIRE(intCloned->at(0) == 1);
}

TEST_CASE("IntColumn filterEqual", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);
    col.push_back(10);
    col.push_back(30);
    col.push_back(10);

    auto result = col.filterEqual("10");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 2, 4}));
}

TEST_CASE("IntColumn filterNotEqual", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);
    col.push_back(10);
    col.push_back(30);

    auto result = col.filterNotEqual("10");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{1, 3}));
}

TEST_CASE("IntColumn filterLessThan", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);
    col.push_back(30);
    col.push_back(40);
    col.push_back(50);

    auto result = col.filterLessThan("30");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 1}));
}

TEST_CASE("IntColumn filterLessOrEqual", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);
    col.push_back(30);
    col.push_back(40);

    auto result = col.filterLessOrEqual("30");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 1, 2}));
}

TEST_CASE("IntColumn filterGreaterThan", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);
    col.push_back(30);
    col.push_back(40);

    auto result = col.filterGreaterThan("20");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{2, 3}));
}

TEST_CASE("IntColumn filterGreaterOrEqual", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);
    col.push_back(30);
    col.push_back(40);

    auto result = col.filterGreaterOrEqual("20");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{1, 2, 3}));
}

TEST_CASE("IntColumn filterContains returns empty", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(10);
    col.push_back(20);

    auto result = col.filterContains("1");

    REQUIRE(result.empty());
}

TEST_CASE("IntColumn filterByIndices", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(100);
    col.push_back(200);
    col.push_back(300);
    col.push_back(400);
    col.push_back(500);

    auto filtered = col.filterByIndices({1, 3, 4});
    auto* intFiltered = dynamic_cast<IntColumn*>(filtered.get());

    REQUIRE(intFiltered->size() == 3);
    REQUIRE(intFiltered->at(0) == 200);
    REQUIRE(intFiltered->at(1) == 400);
    REQUIRE(intFiltered->at(2) == 500);
}

TEST_CASE("IntColumn getSortedIndices ascending", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(30);
    col.push_back(10);
    col.push_back(50);
    col.push_back(20);

    std::vector<size_t> indices = {0, 1, 2, 3};
    col.getSortedIndices(indices, true);

    REQUIRE_THAT(indices, Equals(std::vector<size_t>{1, 3, 0, 2}));
}

TEST_CASE("IntColumn getSortedIndices descending", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(30);
    col.push_back(10);
    col.push_back(50);
    col.push_back(20);

    std::vector<size_t> indices = {0, 1, 2, 3};
    col.getSortedIndices(indices, false);

    REQUIRE_THAT(indices, Equals(std::vector<size_t>{2, 0, 3, 1}));
}

TEST_CASE("IntColumn reserve and clear", "[IntColumn]") {
    IntColumn col("numbers");

    REQUIRE_NOTHROW(col.reserve(1000));

    col.push_back(1);
    col.push_back(2);
    REQUIRE(col.size() == 2);

    col.clear();
    REQUIRE(col.size() == 0);
}

TEST_CASE("IntColumn data access", "[IntColumn]") {
    IntColumn col("numbers");
    col.push_back(1);
    col.push_back(2);
    col.push_back(3);

    const auto& data = col.data();

    REQUIRE(data.size() == 3);
    REQUIRE(data[0] == 1);
    REQUIRE(data[1] == 2);
    REQUIRE(data[2] == 3);
}

// =============================================================================
// DoubleColumn Tests
// =============================================================================

TEST_CASE("DoubleColumn push_back and at", "[DoubleColumn]") {
    DoubleColumn col("prices");

    col.push_back(10.5);
    col.push_back(20.25);
    col.push_back(30.125);

    REQUIRE(col.at(0) == 10.5);
    REQUIRE(col.at(1) == 20.25);
    REQUIRE(col.at(2) == 30.125);
}

TEST_CASE("DoubleColumn set", "[DoubleColumn]") {
    DoubleColumn col("prices");

    col.push_back(1.0);
    col.push_back(2.0);

    col.set(0, 100.5);
    col.set(1, 200.25);

    REQUIRE(col.at(0) == 100.5);
    REQUIRE(col.at(1) == 200.25);
}

TEST_CASE("DoubleColumn getName and getType", "[DoubleColumn]") {
    DoubleColumn col("my_prices");

    REQUIRE(col.getName() == "my_prices");
    REQUIRE(col.getType() == ColumnTypeOpt::DOUBLE);
}

TEST_CASE("DoubleColumn precision", "[DoubleColumn]") {
    DoubleColumn col("precise");

    double value = 3.141592653589793;
    col.push_back(value);

    REQUIRE(col.at(0) == value);
}

TEST_CASE("DoubleColumn filterEqual", "[DoubleColumn]") {
    DoubleColumn col("prices");
    col.push_back(10.5);
    col.push_back(20.5);
    col.push_back(10.5);
    col.push_back(30.5);

    auto result = col.filterEqual("10.5");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 2}));
}

TEST_CASE("DoubleColumn filterLessThan", "[DoubleColumn]") {
    DoubleColumn col("prices");
    col.push_back(10.0);
    col.push_back(20.0);
    col.push_back(30.0);
    col.push_back(40.0);

    auto result = col.filterLessThan("25.0");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 1}));
}

TEST_CASE("DoubleColumn getSortedIndices ascending", "[DoubleColumn]") {
    DoubleColumn col("prices");
    col.push_back(30.5);
    col.push_back(10.2);
    col.push_back(50.8);
    col.push_back(20.1);

    std::vector<size_t> indices = {0, 1, 2, 3};
    col.getSortedIndices(indices, true);

    REQUIRE_THAT(indices, Equals(std::vector<size_t>{1, 3, 0, 2}));
}

TEST_CASE("DoubleColumn clone", "[DoubleColumn]") {
    DoubleColumn col("prices");
    col.push_back(1.1);
    col.push_back(2.2);

    auto cloned = col.clone();

    REQUIRE(cloned->size() == 2);
    REQUIRE(cloned->getName() == "prices");

    col.set(0, 999.0);
    auto* doubleCloned = dynamic_cast<DoubleColumn*>(cloned.get());
    REQUIRE(doubleCloned->at(0) == 1.1);
}

// =============================================================================
// StringColumn Tests
// =============================================================================

TEST_CASE("StringColumn push_back and at", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Alice");
    col.push_back("Bob");
    col.push_back("Charlie");

    REQUIRE(col.at(0) == "Alice");
    REQUIRE(col.at(1) == "Bob");
    REQUIRE(col.at(2) == "Charlie");
}

TEST_CASE("StringColumn push_back with StringId", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    auto id = pool->intern("PreInterned");
    col.push_back(id);

    REQUIRE(col.at(0) == "PreInterned");
    REQUIRE(col.getId(0) == id);
}

TEST_CASE("StringColumn set", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Original");

    col.set(0, "Modified");

    REQUIRE(col.at(0) == "Modified");
}

TEST_CASE("StringColumn getName and getType", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("my_names", pool);

    REQUIRE(col.getName() == "my_names");
    REQUIRE(col.getType() == ColumnTypeOpt::STRING);
}

TEST_CASE("StringColumn getId", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("first");
    col.push_back("second");

    auto id0 = col.getId(0);
    auto id1 = col.getId(1);

    REQUIRE(pool->getString(id0) == "first");
    REQUIRE(pool->getString(id1) == "second");
}

TEST_CASE("StringColumn getStringPool", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("test");

    auto retrievedPool = col.getStringPool();
    REQUIRE(retrievedPool == pool);
    REQUIRE(retrievedPool->size() == 1);
}

TEST_CASE("StringColumn filterEqual", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Alice");
    col.push_back("Bob");
    col.push_back("Alice");
    col.push_back("Charlie");
    col.push_back("Alice");

    auto result = col.filterEqual("Alice");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 2, 4}));
}

TEST_CASE("StringColumn filterNotEqual", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Alice");
    col.push_back("Bob");
    col.push_back("Alice");

    auto result = col.filterNotEqual("Alice");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{1}));
}

TEST_CASE("StringColumn filterLessThan", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Charlie");
    col.push_back("Alice");
    col.push_back("Bob");
    col.push_back("David");

    auto result = col.filterLessThan("Charlie");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{1, 2}));
}

TEST_CASE("StringColumn filterContains", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("hello world");
    col.push_back("goodbye");
    col.push_back("hello there");
    col.push_back("hi");

    auto result = col.filterContains("hello");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 2}));
}

TEST_CASE("StringColumn filterContains - no match", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("apple");
    col.push_back("banana");

    auto result = col.filterContains("xyz");

    REQUIRE(result.empty());
}

TEST_CASE("StringColumn filterContains - position variants", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("text", pool);

    col.push_back("prefix_middle");  // at beginning
    col.push_back("in_prefix_here"); // in middle
    col.push_back("end_prefix");     // at end
    col.push_back("no_match");

    auto result = col.filterContains("prefix");

    REQUIRE_THAT(result, Equals(std::vector<size_t>{0, 1, 2}));
}

TEST_CASE("StringColumn getSortedIndices ascending", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Charlie");
    col.push_back("Alice");
    col.push_back("David");
    col.push_back("Bob");

    std::vector<size_t> indices = {0, 1, 2, 3};
    col.getSortedIndices(indices, true);

    REQUIRE_THAT(indices, Equals(std::vector<size_t>{1, 3, 0, 2}));
}

TEST_CASE("StringColumn getSortedIndices descending", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Charlie");
    col.push_back("Alice");
    col.push_back("David");
    col.push_back("Bob");

    std::vector<size_t> indices = {0, 1, 2, 3};
    col.getSortedIndices(indices, false);

    REQUIRE_THAT(indices, Equals(std::vector<size_t>{2, 0, 3, 1}));
}

TEST_CASE("StringColumn clone", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("Alice");
    col.push_back("Bob");

    auto cloned = col.clone();

    REQUIRE(cloned->size() == 2);
    REQUIRE(cloned->getName() == "names");

    col.set(0, "Modified");
    auto* stringCloned = dynamic_cast<StringColumn*>(cloned.get());
    REQUIRE(stringCloned->at(0) == "Alice");
}

TEST_CASE("StringColumn shared pool between columns", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();

    StringColumn col1("col1", pool);
    StringColumn col2("col2", pool);

    col1.push_back("shared_string");
    auto id1 = col1.getId(0);

    col2.push_back("shared_string");
    auto id2 = col2.getId(0);

    // Same string should have same ID (interned in same pool)
    REQUIRE(id1 == id2);
    REQUIRE(pool->size() == 1);
}

TEST_CASE("StringColumn filterByIndices", "[StringColumn]") {
    auto pool = std::make_shared<StringPool>();
    StringColumn col("names", pool);

    col.push_back("A");
    col.push_back("B");
    col.push_back("C");
    col.push_back("D");
    col.push_back("E");

    auto filtered = col.filterByIndices({1, 3});
    auto* stringFiltered = dynamic_cast<StringColumn*>(filtered.get());

    REQUIRE(stringFiltered->size() == 2);
    REQUIRE(stringFiltered->at(0) == "B");
    REQUIRE(stringFiltered->at(1) == "D");
}
