#include <catch2/catch_test_macros.hpp>
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameSerializer.hpp"

using namespace dataframe;

// =============================================================================
// toString Tests
// =============================================================================

TEST_CASE("Serializer toString empty DataFrame", "[DataFrameSerializer]") {
    DataFrame df;
    // A DataFrame with columns but no rows shows headers
    df.addIntColumn("value");

    std::string result = df.toString();

    // Should show column headers even with no data
    REQUIRE(result.find("value") != std::string::npos);
}

TEST_CASE("Serializer toString no columns", "[DataFrameSerializer]") {
    DataFrame df;

    std::string result = df.toString();

    // A DataFrame with no columns shows "Empty"
    REQUIRE(result.find("Empty") != std::string::npos);
}

TEST_CASE("Serializer toString with data", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("name");

    df.addRow({"1", "Alice"});
    df.addRow({"2", "Bob"});

    std::string result = df.toString();

    // Should contain headers
    REQUIRE(result.find("id") != std::string::npos);
    REQUIRE(result.find("name") != std::string::npos);

    // Should contain data
    REQUIRE(result.find("Alice") != std::string::npos);
    REQUIRE(result.find("Bob") != std::string::npos);
}

TEST_CASE("Serializer toString with maxRows limit", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("value");

    for (int i = 1; i <= 20; ++i) {
        df.addRow({std::to_string(i)});
    }

    std::string result = df.toString(5);

    // Should contain "more rows" message
    REQUIRE(result.find("more rows") != std::string::npos);

    // Should have limited output
    REQUIRE(result.find("1") != std::string::npos);
    REQUIRE(result.find("5") != std::string::npos);
}

TEST_CASE("Serializer toString TSV format", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("a");
    df.addIntColumn("b");
    df.addIntColumn("c");

    df.addRow({"1", "2", "3"});

    std::string result = df.toString();

    // Headers should be tab-separated
    REQUIRE(result.find("a\tb\tc") != std::string::npos);
}

TEST_CASE("Serializer toString shows all columns", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("int_col");
    df.addDoubleColumn("double_col");
    df.addStringColumn("string_col");

    df.addRow({"42", "3.14", "hello"});

    std::string result = df.toString();

    REQUIRE(result.find("int_col") != std::string::npos);
    REQUIRE(result.find("double_col") != std::string::npos);
    REQUIRE(result.find("string_col") != std::string::npos);
    REQUIRE(result.find("42") != std::string::npos);
    REQUIRE(result.find("3.14") != std::string::npos);
    REQUIRE(result.find("hello") != std::string::npos);
}

// =============================================================================
// toJson Tests
// =============================================================================

TEST_CASE("Serializer toJson format columnar", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("name");

    df.addRow({"1", "Alice"});

    json result = df.toJson();

    REQUIRE(result.contains("columns"));
    REQUIRE(result.contains("data"));
}

TEST_CASE("Serializer toJson columns array", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("first");
    df.addDoubleColumn("second");
    df.addStringColumn("third");

    json result = df.toJson();

    REQUIRE(result["columns"].is_array());
    REQUIRE(result["columns"].size() == 3);
    REQUIRE(result["columns"][0] == "first");
    REQUIRE(result["columns"][1] == "second");
    REQUIRE(result["columns"][2] == "third");
}

TEST_CASE("Serializer toJson data array of arrays", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("name");

    df.addRow({"1", "Alice"});
    df.addRow({"2", "Bob"});

    json result = df.toJson();

    REQUIRE(result["data"].is_array());
    REQUIRE(result["data"].size() == 2);

    // Each row is an array
    REQUIRE(result["data"][0].is_array());
    REQUIRE(result["data"][1].is_array());
}

TEST_CASE("Serializer toJson types correct", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("int_col");
    df.addDoubleColumn("double_col");
    df.addStringColumn("string_col");

    df.addRow({"42", "3.14", "hello"});

    json result = df.toJson();

    // First row
    auto row = result["data"][0];

    REQUIRE(row[0].is_number_integer());
    REQUIRE(row[0] == 42);

    REQUIRE(row[1].is_number_float());
    REQUIRE(row[1] == 3.14);

    REQUIRE(row[2].is_string());
    REQUIRE(row[2] == "hello");
}

TEST_CASE("Serializer toJson empty DataFrame", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("value");

    json result = df.toJson();

    REQUIRE(result["columns"].size() == 1);
    REQUIRE(result["data"].empty());
}

TEST_CASE("Serializer toJson preserves column order", "[DataFrameSerializer]") {
    DataFrame df;
    df.addStringColumn("z_last");
    df.addIntColumn("a_first");
    df.addDoubleColumn("m_middle");

    json result = df.toJson();

    // Order should be insertion order, not alphabetical
    REQUIRE(result["columns"][0] == "z_last");
    REQUIRE(result["columns"][1] == "a_first");
    REQUIRE(result["columns"][2] == "m_middle");
}

TEST_CASE("Serializer toJson multiple rows", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("name");
    df.addDoubleColumn("score");

    df.addRow({"1", "Alice", "95.5"});
    df.addRow({"2", "Bob", "87.3"});
    df.addRow({"3", "Charlie", "92.1"});

    json result = df.toJson();

    REQUIRE(result["data"].size() == 3);

    REQUIRE(result["data"][0][0] == 1);
    REQUIRE(result["data"][0][1] == "Alice");
    REQUIRE(result["data"][0][2] == 95.5);

    REQUIRE(result["data"][1][0] == 2);
    REQUIRE(result["data"][1][1] == "Bob");
    REQUIRE(result["data"][1][2] == 87.3);

    REQUIRE(result["data"][2][0] == 3);
    REQUIRE(result["data"][2][1] == "Charlie");
    REQUIRE(result["data"][2][2] == 92.1);
}

TEST_CASE("Serializer toJson negative numbers", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("int_val");
    df.addDoubleColumn("double_val");

    df.addRow({"-42", "-3.14"});

    json result = df.toJson();

    REQUIRE(result["data"][0][0] == -42);
    REQUIRE(result["data"][0][1] == -3.14);
}

TEST_CASE("Serializer toJson special strings", "[DataFrameSerializer]") {
    DataFrame df;
    df.addStringColumn("text");

    df.addRow({"hello\tworld"});  // Tab
    df.addRow({"line1\nline2"});  // Newline
    df.addRow({""});              // Empty string

    json result = df.toJson();

    REQUIRE(result["data"][0][0] == "hello\tworld");
    REQUIRE(result["data"][1][0] == "line1\nline2");
    REQUIRE(result["data"][2][0] == "");
}

TEST_CASE("Serializer toJson large numbers", "[DataFrameSerializer]") {
    DataFrame df;
    df.addIntColumn("big_int");
    df.addDoubleColumn("big_double");

    df.addRow({"2147483647", "1.7976931348623157e+308"});

    json result = df.toJson();

    REQUIRE(result["data"][0][0] == 2147483647);
    // Double precision check
    REQUIRE(result["data"][0][1] > 1e300);
}
