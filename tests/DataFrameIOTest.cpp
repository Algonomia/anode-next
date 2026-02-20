#include <catch2/catch_test_macros.hpp>
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameIO.hpp"
#include <fstream>
#include <filesystem>

using namespace dataframe;

// Helper to create temporary CSV file
static std::string createTempCSV(const std::string& content) {
    std::string path = "/tmp/test_dataframe_" + std::to_string(std::rand()) + ".csv";
    std::ofstream file(path);
    file << content;
    file.close();
    return path;
}

// Helper to cleanup temp file
static void cleanupTempFile(const std::string& path) {
    std::filesystem::remove(path);
}

// =============================================================================
// readCSV Tests
// =============================================================================

TEST_CASE("CSV readCSV with header", "[DataFrameIO]") {
    std::string csv = "id,name,value\n1,Alice,100\n2,Bob,200\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    REQUIRE(df->columnCount() == 3);
    REQUIRE(df->rowCount() == 2);
    REQUIRE(df->hasColumn("id"));
    REQUIRE(df->hasColumn("name"));
    REQUIRE(df->hasColumn("value"));

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV without header", "[DataFrameIO]") {
    std::string csv = "1,Alice,100\n2,Bob,200\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path, ',', false);

    REQUIRE(df->columnCount() == 3);
    REQUIRE(df->rowCount() == 2);

    // Should have generated column names
    auto names = df->getColumnNames();
    REQUIRE(names[0] == "col0");
    REQUIRE(names[1] == "col1");
    REQUIRE(names[2] == "col2");

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV type detection int", "[DataFrameIO]") {
    std::string csv = "value\n1\n2\n3\n42\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    auto col = df->getColumn("value");
    REQUIRE(col->getType() == ColumnTypeOpt::INT);

    auto intCol = std::dynamic_pointer_cast<IntColumn>(col);
    REQUIRE(intCol->at(0) == 1);
    REQUIRE(intCol->at(3) == 42);

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV type detection double", "[DataFrameIO]") {
    std::string csv = "price\n10.5\n20.25\n30.125\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    auto col = df->getColumn("price");
    REQUIRE(col->getType() == ColumnTypeOpt::DOUBLE);

    auto doubleCol = std::dynamic_pointer_cast<DoubleColumn>(col);
    REQUIRE(doubleCol->at(0) == 10.5);
    REQUIRE(doubleCol->at(1) == 20.25);

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV type detection string", "[DataFrameIO]") {
    std::string csv = "name\nAlice\nBob\nCharlie\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    auto col = df->getColumn("name");
    REQUIRE(col->getType() == ColumnTypeOpt::STRING);

    auto stringCol = std::dynamic_pointer_cast<StringColumn>(col);
    REQUIRE(stringCol->at(0) == "Alice");
    REQUIRE(stringCol->at(2) == "Charlie");

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV custom delimiter semicolon", "[DataFrameIO]") {
    std::string csv = "id;name;value\n1;Alice;100\n2;Bob;200\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path, ';');

    REQUIRE(df->columnCount() == 3);
    REQUIRE(df->rowCount() == 2);
    REQUIRE(df->hasColumn("name"));

    auto nameCol = std::dynamic_pointer_cast<StringColumn>(df->getColumn("name"));
    REQUIRE(nameCol->at(0) == "Alice");

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV custom delimiter tab", "[DataFrameIO]") {
    std::string csv = "id\tname\tvalue\n1\tAlice\t100\n2\tBob\t200\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path, '\t');

    REQUIRE(df->columnCount() == 3);
    REQUIRE(df->rowCount() == 2);

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV quoted fields", "[DataFrameIO]") {
    std::string csv = "name,description\nAlice,\"Hello, World\"\nBob,\"Simple\"\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    auto descCol = std::dynamic_pointer_cast<StringColumn>(df->getColumn("description"));

    // Quoted field with comma should be parsed correctly
    REQUIRE(descCol->at(0) == "Hello, World");
    REQUIRE(descCol->at(1) == "Simple");

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV escaped quotes", "[DataFrameIO]") {
    std::string csv = "text\n\"He said \"\"Hello\"\"\"\n\"Normal\"\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    auto textCol = std::dynamic_pointer_cast<StringColumn>(df->getColumn("text"));

    // Escaped quotes ("") should become single quotes
    REQUIRE(textCol->at(0) == "He said \"Hello\"");

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV mixed types", "[DataFrameIO]") {
    std::string csv = "id,price,name,active\n1,10.5,Alice,true\n2,20.0,Bob,false\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    REQUIRE(df->getColumn("id")->getType() == ColumnTypeOpt::INT);
    REQUIRE(df->getColumn("price")->getType() == ColumnTypeOpt::DOUBLE);
    REQUIRE(df->getColumn("name")->getType() == ColumnTypeOpt::STRING);
    REQUIRE(df->getColumn("active")->getType() == ColumnTypeOpt::STRING);  // true/false as string

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV empty file with header only", "[DataFrameIO]") {
    // When CSV has only headers and no data, columns are not created
    // because type detection requires at least one data row
    std::string csv = "id,name,value\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    // No data rows means no columns created (types can't be detected)
    REQUIRE(df->columnCount() == 0);
    REQUIRE(df->rowCount() == 0);

    cleanupTempFile(path);
}

// =============================================================================
// writeCSV Tests
// =============================================================================

TEST_CASE("CSV writeCSV basic", "[DataFrameIO]") {
    DataFrame df;
    df.addIntColumn("id");
    df.addStringColumn("name");

    df.addRow({"1", "Alice"});
    df.addRow({"2", "Bob"});

    std::string path = "/tmp/test_write_" + std::to_string(std::rand()) + ".csv";

    DataFrameIO::writeCSV(df, path);

    // Verify file exists
    REQUIRE(std::filesystem::exists(path));

    // Read back and verify
    std::ifstream file(path);
    std::string line;

    std::getline(file, line);
    REQUIRE(line.find("id") != std::string::npos);
    REQUIRE(line.find("name") != std::string::npos);

    std::getline(file, line);
    REQUIRE(line.find("Alice") != std::string::npos);

    file.close();
    cleanupTempFile(path);
}

TEST_CASE("CSV writeCSV with header", "[DataFrameIO]") {
    DataFrame df;
    df.addIntColumn("col1");
    df.addIntColumn("col2");

    df.addRow({"1", "2"});

    std::string path = "/tmp/test_write_header_" + std::to_string(std::rand()) + ".csv";

    DataFrameIO::writeCSV(df, path, ',', true);

    std::ifstream file(path);
    std::string line;
    std::getline(file, line);

    REQUIRE(line == "col1,col2");

    file.close();
    cleanupTempFile(path);
}

TEST_CASE("CSV writeCSV without header", "[DataFrameIO]") {
    DataFrame df;
    df.addIntColumn("col1");
    df.addIntColumn("col2");

    df.addRow({"1", "2"});

    std::string path = "/tmp/test_write_noheader_" + std::to_string(std::rand()) + ".csv";

    DataFrameIO::writeCSV(df, path, ',', false);

    std::ifstream file(path);
    std::string line;
    std::getline(file, line);

    // First line should be data, not headers
    REQUIRE(line == "1,2");

    file.close();
    cleanupTempFile(path);
}

TEST_CASE("CSV writeCSV custom delimiter", "[DataFrameIO]") {
    DataFrame df;
    df.addIntColumn("a");
    df.addIntColumn("b");

    df.addRow({"1", "2"});

    std::string path = "/tmp/test_write_delim_" + std::to_string(std::rand()) + ".csv";

    DataFrameIO::writeCSV(df, path, ';', true);

    std::ifstream file(path);
    std::string line;
    std::getline(file, line);

    REQUIRE(line == "a;b");

    file.close();
    cleanupTempFile(path);
}

// =============================================================================
// Roundtrip Tests
// =============================================================================

TEST_CASE("CSV roundtrip read-write-read", "[DataFrameIO]") {
    // Create original DataFrame
    DataFrame original;
    original.addIntColumn("id");
    original.addDoubleColumn("price");
    original.addStringColumn("name");

    original.addRow({"1", "10.5", "Alice"});
    original.addRow({"2", "20.25", "Bob"});
    original.addRow({"3", "30.0", "Charlie"});

    // Write to CSV
    std::string path = "/tmp/test_roundtrip_" + std::to_string(std::rand()) + ".csv";
    DataFrameIO::writeCSV(original, path);

    // Read back
    auto loaded = DataFrameIO::readCSV(path);

    // Verify
    REQUIRE(loaded->rowCount() == 3);
    REQUIRE(loaded->columnCount() == 3);

    auto idCol = std::dynamic_pointer_cast<IntColumn>(loaded->getColumn("id"));
    auto priceCol = std::dynamic_pointer_cast<DoubleColumn>(loaded->getColumn("price"));
    auto nameCol = std::dynamic_pointer_cast<StringColumn>(loaded->getColumn("name"));

    REQUIRE(idCol->at(0) == 1);
    REQUIRE(idCol->at(1) == 2);
    REQUIRE(idCol->at(2) == 3);

    REQUIRE(priceCol->at(0) == 10.5);
    REQUIRE(priceCol->at(1) == 20.25);
    REQUIRE(priceCol->at(2) == 30.0);

    REQUIRE(nameCol->at(0) == "Alice");
    REQUIRE(nameCol->at(1) == "Bob");
    REQUIRE(nameCol->at(2) == "Charlie");

    cleanupTempFile(path);
}

TEST_CASE("CSV roundtrip preserves types", "[DataFrameIO]") {
    DataFrame original;
    original.addIntColumn("int_col");
    original.addDoubleColumn("double_col");
    original.addStringColumn("string_col");

    original.addRow({"42", "3.14159", "hello"});

    std::string path = "/tmp/test_roundtrip_types_" + std::to_string(std::rand()) + ".csv";
    DataFrameIO::writeCSV(original, path);

    auto loaded = DataFrameIO::readCSV(path);

    REQUIRE(loaded->getColumn("int_col")->getType() == ColumnTypeOpt::INT);
    REQUIRE(loaded->getColumn("double_col")->getType() == ColumnTypeOpt::DOUBLE);
    REQUIRE(loaded->getColumn("string_col")->getType() == ColumnTypeOpt::STRING);

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV file not found", "[DataFrameIO]") {
    REQUIRE_THROWS(DataFrameIO::readCSV("/nonexistent/path/file.csv"));
}

TEST_CASE("CSV readCSV empty rows ignored", "[DataFrameIO]") {
    std::string csv = "id,name\n1,Alice\n\n2,Bob\n\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    // Empty rows should be skipped
    REQUIRE(df->rowCount() == 2);

    cleanupTempFile(path);
}

TEST_CASE("CSV readCSV whitespace trimming", "[DataFrameIO]") {
    std::string csv = "id,name\n1 , Alice \n 2,Bob\n";
    std::string path = createTempCSV(csv);

    auto df = DataFrameIO::readCSV(path);

    auto nameCol = std::dynamic_pointer_cast<StringColumn>(df->getColumn("name"));

    // Whitespace should be trimmed
    REQUIRE(nameCol->at(0) == "Alice");
    REQUIRE(nameCol->at(1) == "Bob");

    cleanupTempFile(path);
}
