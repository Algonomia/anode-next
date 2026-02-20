#include <catch2/catch_test_macros.hpp>
#include "nodes/Types.hpp"
#include "dataframe/DataFrame.hpp"

using namespace nodes;
using namespace dataframe;

// =============================================================================
// NodeType Tests
// =============================================================================

TEST_CASE("NodeType to string conversion", "[NodeType]") {
    REQUIRE(nodeTypeToString(NodeType::Int) == "int");
    REQUIRE(nodeTypeToString(NodeType::Double) == "double");
    REQUIRE(nodeTypeToString(NodeType::String) == "string");
    REQUIRE(nodeTypeToString(NodeType::Bool) == "bool");
    REQUIRE(nodeTypeToString(NodeType::Null) == "null");
    REQUIRE(nodeTypeToString(NodeType::Field) == "field");
    REQUIRE(nodeTypeToString(NodeType::Csv) == "csv");
}

TEST_CASE("String to NodeType conversion", "[NodeType]") {
    REQUIRE(stringToNodeType("int") == NodeType::Int);
    REQUIRE(stringToNodeType("double") == NodeType::Double);
    REQUIRE(stringToNodeType("string") == NodeType::String);
    REQUIRE(stringToNodeType("bool") == NodeType::Bool);
    REQUIRE(stringToNodeType("null") == NodeType::Null);
    REQUIRE(stringToNodeType("field") == NodeType::Field);
    REQUIRE(stringToNodeType("csv") == NodeType::Csv);

    REQUIRE_THROWS_AS(stringToNodeType("unknown"), std::invalid_argument);
}

TEST_CASE("isScalarType", "[NodeType]") {
    REQUIRE(isScalarType(NodeType::Int) == true);
    REQUIRE(isScalarType(NodeType::Double) == true);
    REQUIRE(isScalarType(NodeType::String) == true);
    REQUIRE(isScalarType(NodeType::Bool) == true);

    REQUIRE(isScalarType(NodeType::Null) == false);
    REQUIRE(isScalarType(NodeType::Field) == false);
    REQUIRE(isScalarType(NodeType::Csv) == false);
}

// =============================================================================
// Workload Basic Tests
// =============================================================================

TEST_CASE("Workload default constructor creates Null", "[Workload]") {
    Workload w;
    REQUIRE(w.isNull() == true);
    REQUIRE(w.getType() == NodeType::Null);
}

TEST_CASE("Workload Int", "[Workload]") {
    Workload w(int64_t(42), NodeType::Int);

    REQUIRE(w.getType() == NodeType::Int);
    REQUIRE(w.getInt() == 42);
    REQUIRE(w.isScalar() == true);
    REQUIRE(w.isField() == false);
    REQUIRE(w.isCsv() == false);
}

TEST_CASE("Workload Double", "[Workload]") {
    Workload w(3.14, NodeType::Double);

    REQUIRE(w.getType() == NodeType::Double);
    REQUIRE(w.getDouble() == 3.14);
    REQUIRE(w.isScalar() == true);
}

TEST_CASE("Workload String", "[Workload]") {
    Workload w("hello", NodeType::String);

    REQUIRE(w.getType() == NodeType::String);
    REQUIRE(w.getString() == "hello");
    REQUIRE(w.isScalar() == true);
}

TEST_CASE("Workload Bool", "[Workload]") {
    Workload w(true);

    REQUIRE(w.getType() == NodeType::Bool);
    REQUIRE(w.getBool() == true);
    REQUIRE(w.isScalar() == true);
}

TEST_CASE("Workload Field", "[Workload]") {
    Workload w("price", NodeType::Field);

    REQUIRE(w.getType() == NodeType::Field);
    REQUIRE(w.getString() == "price");
    REQUIRE(w.isField() == true);
    REQUIRE(w.isScalar() == false);
}

TEST_CASE("Workload Csv", "[Workload]") {
    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("id");

    Workload w(df);

    REQUIRE(w.getType() == NodeType::Csv);
    REQUIRE(w.getCsv() == df);
    REQUIRE(w.isCsv() == true);
    REQUIRE(w.isScalar() == false);
}

// =============================================================================
// Workload Type Conversion Tests
// =============================================================================

TEST_CASE("Workload Int to Double conversion", "[Workload]") {
    Workload w(int64_t(42), NodeType::Int);

    REQUIRE(w.getDouble() == 42.0);
}

TEST_CASE("Workload Double to Int conversion", "[Workload]") {
    Workload w(3.7, NodeType::Double);

    REQUIRE(w.getInt() == 3);  // Truncated
}

TEST_CASE("Workload String to Int conversion", "[Workload]") {
    Workload w("123", NodeType::String);

    REQUIRE(w.getInt() == 123);
}

TEST_CASE("Workload String to Double conversion", "[Workload]") {
    Workload w("3.14", NodeType::String);

    REQUIRE(w.getDouble() == 3.14);
}

// =============================================================================
// Workload Error Cases
// =============================================================================

TEST_CASE("Workload wrong type throws", "[Workload]") {
    Workload wInt(int64_t(42), NodeType::Int);
    REQUIRE_THROWS(wInt.getBool());
    REQUIRE_THROWS(wInt.getCsv());

    Workload wBool(true);
    REQUIRE_THROWS(wBool.getInt());
    REQUIRE_THROWS(wBool.getString());

    auto df = std::make_shared<DataFrame>();
    Workload wCsv(df);
    REQUIRE_THROWS(wCsv.getInt());
    REQUIRE_THROWS(wCsv.getString());
}

// =============================================================================
// Workload Broadcasting Tests
// =============================================================================

TEST_CASE("Workload scalar broadcasting - Int", "[Workload][Broadcasting]") {
    Workload w(int64_t(100), NodeType::Int);

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("x");
    df->addRow({"1"});
    df->addRow({"2"});
    df->addRow({"3"});

    auto header = df->getColumnNames();

    // Scalar broadcasts: same value for all rows
    REQUIRE(w.getIntAtRow(0, header, df) == 100);
    REQUIRE(w.getIntAtRow(1, header, df) == 100);
    REQUIRE(w.getIntAtRow(2, header, df) == 100);
}

TEST_CASE("Workload scalar broadcasting - Double", "[Workload][Broadcasting]") {
    Workload w(3.14, NodeType::Double);

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("x");
    df->addRow({"1"});
    df->addRow({"2"});

    auto header = df->getColumnNames();

    REQUIRE(w.getDoubleAtRow(0, header, df) == 3.14);
    REQUIRE(w.getDoubleAtRow(1, header, df) == 3.14);
}

TEST_CASE("Workload Field lookup - Int column", "[Workload][Broadcasting]") {
    Workload w("price", NodeType::Field);

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("price");
    df->addRow({"10"});
    df->addRow({"20"});
    df->addRow({"30"});

    auto header = df->getColumnNames();

    REQUIRE(w.getIntAtRow(0, header, df) == 10);
    REQUIRE(w.getIntAtRow(1, header, df) == 20);
    REQUIRE(w.getIntAtRow(2, header, df) == 30);
}

TEST_CASE("Workload Field lookup - Double column", "[Workload][Broadcasting]") {
    Workload w("value", NodeType::Field);

    auto df = std::make_shared<DataFrame>();
    df->addDoubleColumn("value");

    auto col = std::dynamic_pointer_cast<DoubleColumn>(df->getColumn("value"));
    col->push_back(1.5);
    col->push_back(2.5);
    col->push_back(3.5);

    auto header = df->getColumnNames();

    REQUIRE(w.getDoubleAtRow(0, header, df) == 1.5);
    REQUIRE(w.getDoubleAtRow(1, header, df) == 2.5);
    REQUIRE(w.getDoubleAtRow(2, header, df) == 3.5);
}

TEST_CASE("Workload Field lookup - String column", "[Workload][Broadcasting]") {
    Workload w("name", NodeType::Field);

    auto df = std::make_shared<DataFrame>();
    df->addStringColumn("name");
    df->addRow({"Alice"});
    df->addRow({"Bob"});
    df->addRow({"Charlie"});

    auto header = df->getColumnNames();

    REQUIRE(w.getStringAtRow(0, header, df) == "Alice");
    REQUIRE(w.getStringAtRow(1, header, df) == "Bob");
    REQUIRE(w.getStringAtRow(2, header, df) == "Charlie");
}

TEST_CASE("Workload Field not found throws", "[Workload][Broadcasting]") {
    Workload w("nonexistent", NodeType::Field);

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("x");
    df->addRow({"1"});

    auto header = df->getColumnNames();

    REQUIRE_THROWS(w.getIntAtRow(0, header, df));
}

// =============================================================================
// PortType Tests
// =============================================================================

TEST_CASE("PortType single type", "[PortType]") {
    PortType pt(NodeType::Int);

    REQUIRE(pt.accepts(NodeType::Int) == true);
    REQUIRE(pt.accepts(NodeType::Double) == false);
    REQUIRE(pt.accepts(NodeType::String) == false);
    REQUIRE(pt.isMultiType() == false);
    REQUIRE(pt.getPrimaryType() == NodeType::Int);
}

TEST_CASE("PortType multi-type", "[PortType]") {
    PortType pt({NodeType::Int, NodeType::Double, NodeType::Field});

    REQUIRE(pt.accepts(NodeType::Int) == true);
    REQUIRE(pt.accepts(NodeType::Double) == true);
    REQUIRE(pt.accepts(NodeType::Field) == true);
    REQUIRE(pt.accepts(NodeType::String) == false);
    REQUIRE(pt.accepts(NodeType::Csv) == false);
    REQUIRE(pt.isMultiType() == true);
    REQUIRE(pt.getTypes().size() == 3);
}

TEST_CASE("PortType accepts Workload", "[PortType]") {
    PortType pt({NodeType::Int, NodeType::Double});

    Workload wInt(int64_t(42), NodeType::Int);
    Workload wDouble(3.14, NodeType::Double);
    Workload wString("hello", NodeType::String);

    REQUIRE(pt.accepts(wInt) == true);
    REQUIRE(pt.accepts(wDouble) == true);
    REQUIRE(pt.accepts(wString) == false);
}
