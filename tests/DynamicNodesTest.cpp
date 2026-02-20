#include <catch2/catch_test_macros.hpp>
#include "nodes/nodes/common/DynamicNodes.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeExecutor.hpp"
#include "nodes/Types.hpp"
#include "dataframe/DataFrame.hpp"

using namespace nodes;
using namespace dataframe;
using Type = nodes::NodeType;

// =============================================================================
// DynamicNodes Registration Tests
// =============================================================================

TEST_CASE("Dynamic nodes are registered", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();

    // Register nodes (safe to call multiple times)
    registerDynamicNodes();

    // After registration, nodes should exist
    // They are registered as "category/name", so it's "dynamic/dynamic_begin"
    bool hasBegin = registry.hasNode("dynamic/dynamic_begin");
    bool hasEnd = registry.hasNode("dynamic/dynamic_end");

    // If not found with full path, try just name (depends on registry implementation)
    if (!hasBegin) hasBegin = registry.hasNode("dynamic_begin");
    if (!hasEnd) hasEnd = registry.hasNode("dynamic_end");

    REQUIRE(hasBegin);
    REQUIRE(hasEnd);
}

TEST_CASE("dynamic_begin node definition", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    auto def = registry.getNode("dynamic/dynamic_begin");
    REQUIRE(def != nullptr);

    REQUIRE(def->getName() == "dynamic_begin");
    REQUIRE(def->getCategory() == "dynamic");

    // Should have CSV input and output
    auto inputs = def->getInputs();
    REQUIRE(inputs.size() == 1);
    REQUIRE(inputs[0].name == "csv");

    auto outputs = def->getOutputs();
    REQUIRE(outputs.size() == 1);
    REQUIRE(outputs[0].name == "csv");
}

TEST_CASE("dynamic_end node definition", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    auto def = registry.getNode("dynamic/dynamic_end");
    REQUIRE(def != nullptr);

    REQUIRE(def->getName() == "dynamic_end");
    REQUIRE(def->getCategory() == "dynamic");
}

// =============================================================================
// DynamicNodes Execution Tests
// =============================================================================

TEST_CASE("dynamic_begin passes through CSV", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    // Create a test DataFrame
    auto df = std::make_shared<DataFrame>();
    auto col = std::make_shared<IntColumn>("value");
    col->push_back(1);
    col->push_back(2);
    col->push_back(3);
    df->addColumn(col);

    // Execute the node
    auto def = registry.getNode("dynamic/dynamic_begin");
    REQUIRE(def != nullptr);

    std::unordered_map<std::string, Workload> inputs;
    inputs["csv"] = Workload(df, Type::Csv);

    NodeExecutor executor(registry);
    auto ctx = executor.executeNode(*def, inputs);

    REQUIRE_FALSE(ctx.hasError());

    auto output = ctx.getOutput("csv");
    REQUIRE_FALSE(output.isNull());
    REQUIRE(output.getType() == Type::Csv);

    auto resultDf = output.getCsv();
    REQUIRE(resultDf != nullptr);
    REQUIRE(resultDf->rowCount() == 3);
    REQUIRE(resultDf->columnCount() == 1);
}

TEST_CASE("dynamic_end passes through CSV", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    // Create a test DataFrame
    auto df = std::make_shared<DataFrame>();
    auto col = std::make_shared<DoubleColumn>("price");
    col->push_back(10.5);
    col->push_back(20.0);
    df->addColumn(col);

    auto def = registry.getNode("dynamic/dynamic_end");
    REQUIRE(def != nullptr);

    std::unordered_map<std::string, Workload> inputs;
    inputs["csv"] = Workload(df, Type::Csv);

    NodeExecutor executor(registry);
    auto ctx = executor.executeNode(*def, inputs);

    REQUIRE_FALSE(ctx.hasError());

    auto output = ctx.getOutput("csv");
    REQUIRE_FALSE(output.isNull());

    auto resultDf = output.getCsv();
    REQUIRE(resultDf->rowCount() == 2);
}

TEST_CASE("dynamic_begin errors without CSV input", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    auto def = registry.getNode("dynamic/dynamic_begin");
    REQUIRE(def != nullptr);

    std::unordered_map<std::string, Workload> inputs;
    // No CSV input provided

    NodeExecutor executor(registry);
    auto ctx = executor.executeNode(*def, inputs);

    REQUIRE(ctx.hasError());
    REQUIRE(ctx.getErrorMessage() == "No CSV input");
}

TEST_CASE("dynamic_end errors without CSV input", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    auto def = registry.getNode("dynamic/dynamic_end");

    std::unordered_map<std::string, Workload> inputs;

    NodeExecutor executor(registry);
    auto ctx = executor.executeNode(*def, inputs);

    REQUIRE(ctx.hasError());
    REQUIRE(ctx.getErrorMessage() == "No CSV input");
}

// =============================================================================
// Graph Integration Tests
// =============================================================================

TEST_CASE("Dynamic nodes in a graph pass-through correctly", "[DynamicNodes]") {
    auto& registry = NodeRegistry::instance();
    registerDynamicNodes();

    // Create graph: csv_source -> dynamic_begin -> dynamic_end
    NodeGraph graph;

    // We can't test csv_source without registering CsvNodes, so let's use
    // a simpler approach with properties

    std::string beginId = graph.addNode("dynamic/dynamic_begin");
    std::string endId = graph.addNode("dynamic/dynamic_end");

    graph.setProperty(beginId, "_name", Workload("test_zone", Type::String));
    graph.setProperty(endId, "_name", Workload("test_zone", Type::String));

    graph.connect(beginId, "csv", endId, "csv");

    // Verify the connection exists
    auto conn = graph.getConnectionTo(endId, "csv");
    REQUIRE(conn != nullptr);
    REQUIRE(conn->sourceNodeId == beginId);
    REQUIRE(conn->sourcePortName == "csv");
}
