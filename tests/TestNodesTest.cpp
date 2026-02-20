#include <catch2/catch_test_macros.hpp>
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeExecutor.hpp"
#include "nodes/nodes/common/ScalarNodes.hpp"
#include "nodes/nodes/common/CsvNodes.hpp"
#include "nodes/nodes/common/MathNodes.hpp"
#include "dataframe/DataFrame.hpp"
#include "dataframe/Column.hpp"

using namespace nodes;
using namespace dataframe;

// Helper to get a fresh registry with test nodes
class TestNodesFixture {
public:
    TestNodesFixture() {
        // Clear and register nodes
        NodeRegistry::instance().clear();
        registerScalarNodes();
        registerCsvNodes();
        registerMathNodes();
    }

    ~TestNodesFixture() {
        NodeRegistry::instance().clear();
    }
};

// =============================================================================
// csv_source Tests
// =============================================================================

TEST_CASE("csv_source returns DataFrame", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("csv_source");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results.count(n) == 1);

    auto csv = results[n]["csv"].getCsv();
    REQUIRE(csv != nullptr);
    REQUIRE(csv->rowCount() == 4);
    REQUIRE(csv->hasColumn("id"));
    REQUIRE(csv->hasColumn("name"));
    REQUIRE(csv->hasColumn("price"));
}

TEST_CASE("csv_source data is correct", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("csv_source");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    auto csv = results[n]["csv"].getCsv();

    // Check id column
    auto idCol = std::dynamic_pointer_cast<IntColumn>(csv->getColumn("id"));
    REQUIRE(idCol->at(0) == 1);
    REQUIRE(idCol->at(1) == 2);
    REQUIRE(idCol->at(2) == 3);
    REQUIRE(idCol->at(3) == 4);

    // Check name column
    auto nameCol = std::dynamic_pointer_cast<StringColumn>(csv->getColumn("name"));
    REQUIRE(nameCol->at(0) == "Apple");
    REQUIRE(nameCol->at(1) == "Banana");
    REQUIRE(nameCol->at(2) == "Orange");
    REQUIRE(nameCol->at(3) == "Grape");

    // Check price column
    auto priceCol = std::dynamic_pointer_cast<DoubleColumn>(csv->getColumn("price"));
    REQUIRE(priceCol->at(0) == 1.50);
    REQUIRE(priceCol->at(1) == 0.75);
    REQUIRE(priceCol->at(2) == 2.00);
    REQUIRE(priceCol->at(3) == 3.50);
}

TEST_CASE("csv_source ignores _csv_data property (overrides stored separately)", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    // Create a custom DataFrame (simulating old-style _csv_data in properties)
    auto uploadedDf = std::make_shared<DataFrame>();
    uploadedDf->addIntColumn("id");
    uploadedDf->addStringColumn("question_id");
    uploadedDf->addIntColumn("task_id");
    uploadedDf->addRow({"12361619", "629", "42068"});

    NodeGraph graph;
    auto n = graph.addNode("csv_source");
    graph.setProperty(n, "_csv_data", Workload(uploadedDf));

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // _csv_data in properties is now ignored — should get fallback test data
    auto csv = results[n]["csv"].getCsv();
    REQUIRE(csv != nullptr);
    REQUIRE(csv->rowCount() == 4);  // default Apple/Banana/Orange/Grape
    REQUIRE(csv->hasColumn("id"));
    REQUIRE(csv->hasColumn("name"));
    REQUIRE(csv->hasColumn("price"));
}

TEST_CASE("csv_source connected input wins over _csv_data property", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    // Create uploaded CSV
    auto uploadedDf = std::make_shared<DataFrame>();
    uploadedDf->addIntColumn("val");
    uploadedDf->addRow({"99"});

    // Create a second csv_source that provides a connected input
    NodeGraph graph;
    auto upstream = graph.addNode("csv_source");  // produces default Apple/Banana/... data
    auto target = graph.addNode("csv_source");
    graph.setProperty(target, "_csv_data", Workload(uploadedDf));

    // Wire upstream csv into target's optional csv input
    graph.connect(upstream, "csv", target, "csv");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // Connected input should win (4 rows from upstream), _csv_data is ignored
    auto csv = results[target]["csv"].getCsv();
    REQUIRE(csv != nullptr);
    REQUIRE(csv->rowCount() == 4);
    REQUIRE(csv->hasColumn("name"));
}

TEST_CASE("csv_source CsvOverrides injects DataFrame via identifier", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    // Create override DataFrame
    auto overrideDf = std::make_shared<DataFrame>();
    overrideDf->addIntColumn("x");
    overrideDf->addStringColumn("label");
    overrideDf->addRow({"42", "hello"});
    overrideDf->addRow({"99", "world"});

    NodeGraph graph;
    auto n = graph.addNode("csv_source");
    graph.setProperty(n, "_identifier", Workload(std::string("my_csv"), NodeType::String));

    // Build CsvOverrides map
    CsvOverrides overrides;
    overrides["my_csv"] = overrideDf;

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph, overrides);

    REQUIRE(exec.hasErrors() == false);

    auto csv = results[n]["csv"].getCsv();
    REQUIRE(csv != nullptr);
    REQUIRE(csv->rowCount() == 2);
    REQUIRE(csv->hasColumn("x"));
    REQUIRE(csv->hasColumn("label"));

    auto xCol = std::dynamic_pointer_cast<IntColumn>(csv->getColumn("x"));
    REQUIRE(xCol->at(0) == 42);
    REQUIRE(xCol->at(1) == 99);
}

TEST_CASE("csv_source uses connected input when available", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto upstream = graph.addNode("csv_source");  // default test data (4 rows)
    auto target = graph.addNode("csv_source");

    graph.connect(upstream, "csv", target, "csv");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // Should use the connected input (4 rows from upstream)
    auto csv = results[target]["csv"].getCsv();
    REQUIRE(csv != nullptr);
    REQUIRE(csv->rowCount() == 4);
    REQUIRE(csv->hasColumn("name"));
}

TEST_CASE("csv_source falls back to test data when no input", "[TestNodes][csv_source]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("csv_source");
    // No connected input → should get default Apple/Banana/Orange/Grape

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    auto csv = results[n]["csv"].getCsv();
    REQUIRE(csv != nullptr);
    REQUIRE(csv->rowCount() == 4);
}

// =============================================================================
// int_value Tests
// =============================================================================

TEST_CASE("int_value returns default 0", "[TestNodes][int_value]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("int_value");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[n]["value"].getInt() == 0);
}

TEST_CASE("int_value returns configured value", "[TestNodes][int_value]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("int_value");
    graph.setProperty(n, "_value", Workload(int64_t(42), Type::Int));

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[n]["value"].getInt() == 42);
}

TEST_CASE("int_value with negative value", "[TestNodes][int_value]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("int_value");
    graph.setProperty(n, "_value", Workload(int64_t(-100), Type::Int));

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(results[n]["value"].getInt() == -100);
}

// =============================================================================
// field Tests
// =============================================================================

TEST_CASE("field selects column", "[TestNodes][field]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");

    graph.connect(csvNode, "csv", fieldNode, "csv");
    graph.setProperty(fieldNode, "_column", Workload("name", Type::String));

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // Check field output
    auto fieldOutput = results[fieldNode]["field"];
    REQUIRE(fieldOutput.getType() == Type::Field);
    REQUIRE(fieldOutput.getString() == "name");

    // Check CSV passthrough
    auto csvOutput = results[fieldNode]["csv"].getCsv();
    REQUIRE(csvOutput != nullptr);
    REQUIRE(csvOutput->rowCount() == 4);
}

TEST_CASE("field error on missing column", "[TestNodes][field]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");

    graph.connect(csvNode, "csv", fieldNode, "csv");
    graph.setProperty(fieldNode, "_column", Workload("nonexistent", Type::String));

    NodeExecutor exec(NodeRegistry::instance());
    exec.execute(graph);

    REQUIRE(exec.hasErrors() == true);

    auto* result = exec.getResult(fieldNode);
    REQUIRE(result->hasError == true);
    REQUIRE(result->errorMessage.find("not found") != std::string::npos);
}

TEST_CASE("field error without column property", "[TestNodes][field]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");

    graph.connect(csvNode, "csv", fieldNode, "csv");
    // No _column property set

    NodeExecutor exec(NodeRegistry::instance());
    exec.execute(graph);

    REQUIRE(exec.hasErrors() == true);
}

// =============================================================================
// add Tests - Scalar Mode
// =============================================================================

TEST_CASE("add with two int scalars", "[TestNodes][add][scalar]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto n1 = graph.addNode("int_value");
    auto n2 = graph.addNode("int_value");
    auto addNode = graph.addNode("add");

    graph.setProperty(n1, "_value", Workload(int64_t(5), Type::Int));
    graph.setProperty(n2, "_value", Workload(int64_t(3), Type::Int));

    graph.connect(n1, "value", addNode, "src");
    graph.connect(n2, "value", addNode, "operand");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[addNode]["result"].getDouble() == 8.0);
}

TEST_CASE("add with int and double scalars", "[TestNodes][add][scalar]") {
    TestNodesFixture fixture;

    // Register a double_value node for this test
    NodeBuilder("double_value", "scalar")
        .output("value", Type::Double)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            ctx.setOutput("value", prop.isNull() ? 0.0 : prop.getDouble());
        })
        .buildAndRegister();

    NodeGraph graph;
    auto n1 = graph.addNode("int_value");
    auto n2 = graph.addNode("double_value");
    auto addNode = graph.addNode("add");

    graph.setProperty(n1, "_value", Workload(int64_t(10), Type::Int));
    graph.setProperty(n2, "_value", Workload(2.5, Type::Double));

    graph.connect(n1, "value", addNode, "src");
    graph.connect(n2, "value", addNode, "operand");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[addNode]["result"].getDouble() == 12.5);
}

// =============================================================================
// add Tests - Vector Mode (Broadcasting)
// =============================================================================

TEST_CASE("add field + scalar (broadcasting)", "[TestNodes][add][broadcasting]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");
    auto intNode = graph.addNode("int_value");
    auto addNode = graph.addNode("add");

    // field("price") + int(10)
    graph.setProperty(fieldNode, "_column", Workload("price", Type::String));
    graph.setProperty(intNode, "_value", Workload(int64_t(10), Type::Int));

    graph.connect(csvNode, "csv", fieldNode, "csv");
    graph.connect(fieldNode, "field", addNode, "src");
    graph.connect(fieldNode, "csv", addNode, "csv");
    graph.connect(intNode, "value", addNode, "operand");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // Check result CSV - src is field("price"), so "price" column is overwritten
    auto resultCsv = results[addNode]["csv"].getCsv();
    REQUIRE(resultCsv != nullptr);
    REQUIRE(resultCsv->hasColumn("price"));

    auto resultCol = std::dynamic_pointer_cast<DoubleColumn>(resultCsv->getColumn("price"));
    // prices: 1.50, 0.75, 2.00, 3.50 + 10 = 11.50, 10.75, 12.00, 13.50
    REQUIRE(resultCol->at(0) == 11.50);
    REQUIRE(resultCol->at(1) == 10.75);
    REQUIRE(resultCol->at(2) == 12.00);
    REQUIRE(resultCol->at(3) == 13.50);

    // Check scalar result (first row)
    REQUIRE(results[addNode]["result"].getDouble() == 11.50);
}

TEST_CASE("add scalar + field (broadcasting)", "[TestNodes][add][broadcasting]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");
    auto intNode = graph.addNode("int_value");
    auto addNode = graph.addNode("add");

    // int(100) + field("id")
    graph.setProperty(fieldNode, "_column", Workload("id", Type::String));
    graph.setProperty(intNode, "_value", Workload(int64_t(100), Type::Int));

    graph.connect(csvNode, "csv", fieldNode, "csv");
    graph.connect(intNode, "value", addNode, "src");
    graph.connect(fieldNode, "field", addNode, "operand");
    graph.connect(fieldNode, "csv", addNode, "csv");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    auto resultCsv = results[addNode]["csv"].getCsv();
    auto resultCol = std::dynamic_pointer_cast<DoubleColumn>(resultCsv->getColumn("_add_result"));

    // ids: 1, 2, 3, 4 + 100 = 101, 102, 103, 104
    REQUIRE(resultCol->at(0) == 101.0);
    REQUIRE(resultCol->at(1) == 102.0);
    REQUIRE(resultCol->at(2) == 103.0);
    REQUIRE(resultCol->at(3) == 104.0);
}

TEST_CASE("add field + field", "[TestNodes][add][broadcasting]") {
    TestNodesFixture fixture;

    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode1 = graph.addNode("field");
    auto fieldNode2 = graph.addNode("field");
    auto addNode = graph.addNode("add");

    // field("id") + field("price")
    graph.setProperty(fieldNode1, "_column", Workload("id", Type::String));
    graph.setProperty(fieldNode2, "_column", Workload("price", Type::String));

    graph.connect(csvNode, "csv", fieldNode1, "csv");
    graph.connect(csvNode, "csv", fieldNode2, "csv");
    graph.connect(fieldNode1, "field", addNode, "src");
    graph.connect(fieldNode2, "field", addNode, "operand");
    graph.connect(fieldNode1, "csv", addNode, "csv");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // src is field("id"), so "id" column is overwritten with double results
    auto resultCsv = results[addNode]["csv"].getCsv();
    auto resultCol = std::dynamic_pointer_cast<DoubleColumn>(resultCsv->getColumn("id"));

    // id + price: 1+1.50, 2+0.75, 3+2.00, 4+3.50 = 2.50, 2.75, 5.00, 7.50
    REQUIRE(resultCol->at(0) == 2.50);
    REQUIRE(resultCol->at(1) == 2.75);
    REQUIRE(resultCol->at(2) == 5.00);
    REQUIRE(resultCol->at(3) == 7.50);
}

// =============================================================================
// Integration Test: Complete Pipeline
// =============================================================================

TEST_CASE("Complete pipeline: csv -> field -> add -> result", "[TestNodes][Integration]") {
    TestNodesFixture fixture;

    // Pipeline: csv_source -> field(price) + int(5) -> result
    NodeGraph graph;

    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");
    auto intNode = graph.addNode("int_value");
    auto addNode = graph.addNode("add");

    graph.setProperty(fieldNode, "_column", Workload("price", Type::String));
    graph.setProperty(intNode, "_value", Workload(int64_t(5), Type::Int));

    graph.connect(csvNode, "csv", fieldNode, "csv");
    graph.connect(fieldNode, "field", addNode, "src");
    graph.connect(fieldNode, "csv", addNode, "csv");
    graph.connect(intNode, "value", addNode, "operand");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // Verify complete pipeline - src is field("price"), so "price" is overwritten (3 cols total)
    auto finalCsv = results[addNode]["csv"].getCsv();
    REQUIRE(finalCsv->columnCount() == 3);  // id, name, price (overwritten)
    REQUIRE(finalCsv->rowCount() == 4);

    auto resultCol = std::dynamic_pointer_cast<DoubleColumn>(finalCsv->getColumn("price"));
    REQUIRE(resultCol->at(0) == 6.50);   // 1.50 + 5
    REQUIRE(resultCol->at(1) == 5.75);   // 0.75 + 5
    REQUIRE(resultCol->at(2) == 7.00);   // 2.00 + 5
    REQUIRE(resultCol->at(3) == 8.50);   // 3.50 + 5
}

TEST_CASE("Chained adds: field + 10 + 5", "[TestNodes][Integration]") {
    TestNodesFixture fixture;

    NodeGraph graph;

    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");
    auto int1 = graph.addNode("int_value");
    auto int2 = graph.addNode("int_value");
    auto add1 = graph.addNode("add");
    auto add2 = graph.addNode("add");

    // price + 10 -> result + 5
    graph.setProperty(fieldNode, "_column", Workload("price", Type::String));
    graph.setProperty(int1, "_value", Workload(int64_t(10), Type::Int));
    graph.setProperty(int2, "_value", Workload(int64_t(5), Type::Int));

    graph.connect(csvNode, "csv", fieldNode, "csv");

    // First add: field + 10
    graph.connect(fieldNode, "field", add1, "src");
    graph.connect(fieldNode, "csv", add1, "csv");
    graph.connect(int1, "value", add1, "operand");

    // Second add: scalar result + 5
    graph.connect(add1, "result", add2, "src");
    graph.connect(int2, "value", add2, "operand");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // First add: 1.50 + 10 = 11.50 (first row scalar result)
    // Second add: 11.50 + 5 = 16.50
    REQUIRE(results[add2]["result"].getDouble() == 16.50);
}
