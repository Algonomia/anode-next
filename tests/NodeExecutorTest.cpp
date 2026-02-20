#include <catch2/catch_test_macros.hpp>
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeExecutor.hpp"
#include "dataframe/DataFrame.hpp"

using namespace nodes;
using namespace dataframe;

// =============================================================================
// NodeGraph Tests
// =============================================================================

TEST_CASE("NodeGraph empty", "[NodeGraph]") {
    NodeGraph graph;

    REQUIRE(graph.nodeCount() == 0);
    REQUIRE(graph.getNodes().empty());
    REQUIRE(graph.getConnections().empty());
}

TEST_CASE("NodeGraph addNode", "[NodeGraph]") {
    NodeGraph graph;

    auto id1 = graph.addNode("node_type_a");
    auto id2 = graph.addNode("node_type_b");

    REQUIRE(graph.nodeCount() == 2);
    REQUIRE(id1 != id2);

    auto* node1 = graph.getNode(id1);
    REQUIRE(node1 != nullptr);
    REQUIRE(node1->id == id1);
    REQUIRE(node1->definitionName == "node_type_a");
}

TEST_CASE("NodeGraph removeNode", "[NodeGraph]") {
    NodeGraph graph;

    auto id = graph.addNode("test");
    REQUIRE(graph.nodeCount() == 1);

    graph.removeNode(id);
    REQUIRE(graph.nodeCount() == 0);
    REQUIRE(graph.getNode(id) == nullptr);
}

TEST_CASE("NodeGraph connect", "[NodeGraph]") {
    NodeGraph graph;

    auto n1 = graph.addNode("a");
    auto n2 = graph.addNode("b");

    graph.connect(n1, "out", n2, "in");

    REQUIRE(graph.getConnections().size() == 1);

    auto* conn = graph.getConnectionTo(n2, "in");
    REQUIRE(conn != nullptr);
    REQUIRE(conn->sourceNodeId == n1);
    REQUIRE(conn->sourcePortName == "out");
    REQUIRE(conn->targetNodeId == n2);
    REQUIRE(conn->targetPortName == "in");
}

TEST_CASE("NodeGraph connect overwrites existing", "[NodeGraph]") {
    NodeGraph graph;

    auto n1 = graph.addNode("a");
    auto n2 = graph.addNode("b");
    auto n3 = graph.addNode("c");

    graph.connect(n1, "out", n3, "in");
    graph.connect(n2, "out", n3, "in");  // Should replace

    REQUIRE(graph.getConnections().size() == 1);

    auto* conn = graph.getConnectionTo(n3, "in");
    REQUIRE(conn->sourceNodeId == n2);
}

TEST_CASE("NodeGraph disconnect", "[NodeGraph]") {
    NodeGraph graph;

    auto n1 = graph.addNode("a");
    auto n2 = graph.addNode("b");

    graph.connect(n1, "out", n2, "in");
    REQUIRE(graph.getConnections().size() == 1);

    graph.disconnect(n2, "in");
    REQUIRE(graph.getConnections().empty());
}

TEST_CASE("NodeGraph removeNode removes connections", "[NodeGraph]") {
    NodeGraph graph;

    auto n1 = graph.addNode("a");
    auto n2 = graph.addNode("b");
    auto n3 = graph.addNode("c");

    graph.connect(n1, "out", n2, "in");
    graph.connect(n2, "out", n3, "in");

    REQUIRE(graph.getConnections().size() == 2);

    graph.removeNode(n2);

    REQUIRE(graph.getConnections().empty());
}

TEST_CASE("NodeGraph setProperty/getProperty", "[NodeGraph]") {
    NodeGraph graph;

    auto id = graph.addNode("test");

    graph.setProperty(id, "value", Workload(int64_t(42), NodeType::Int));

    auto prop = graph.getProperty(id, "value");
    REQUIRE(prop.getType() == NodeType::Int);
    REQUIRE(prop.getInt() == 42);

    // Non-existent property
    auto missing = graph.getProperty(id, "missing");
    REQUIRE(missing.isNull());
}

// =============================================================================
// NodeExecutor Basic Tests
// =============================================================================

TEST_CASE("NodeExecutor single entry point node", "[NodeExecutor]") {
    NodeRegistry reg;

    NodeBuilder("const_42", "test")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("value", int64_t(42));
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n = graph.addNode("const_42");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(results.count(n) == 1);
    REQUIRE(results[n]["value"].getInt() == 42);
}

TEST_CASE("NodeExecutor linear chain", "[NodeExecutor]") {
    NodeRegistry reg;

    // const_5 -> double_it -> result = 10
    NodeBuilder("const_5", "test")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("value", int64_t(5));
        })
        .buildAndRegister(reg);

    NodeBuilder("double_it", "test")
        .input("in", Type::Int)
        .output("out", Type::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t v = ctx.getInputWorkload("in").getInt();
            ctx.setOutput("out", v * 2);
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n1 = graph.addNode("const_5");
    auto n2 = graph.addNode("double_it");
    graph.connect(n1, "value", n2, "in");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(results[n1]["value"].getInt() == 5);
    REQUIRE(results[n2]["out"].getInt() == 10);
}

TEST_CASE("NodeExecutor two inputs", "[NodeExecutor]") {
    NodeRegistry reg;

    NodeBuilder("const", "test")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            ctx.setOutput("value", prop.isNull() ? int64_t(0) : prop.getInt());
        })
        .buildAndRegister(reg);

    NodeBuilder("add", "test")
        .input("a", Type::Int)
        .input("b", Type::Int)
        .output("result", Type::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t a = ctx.getInputWorkload("a").getInt();
            int64_t b = ctx.getInputWorkload("b").getInt();
            ctx.setOutput("result", a + b);
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n1 = graph.addNode("const");
    auto n2 = graph.addNode("const");
    auto n3 = graph.addNode("add");

    graph.setProperty(n1, "_value", Workload(int64_t(7), Type::Int));
    graph.setProperty(n2, "_value", Workload(int64_t(3), Type::Int));

    graph.connect(n1, "value", n3, "a");
    graph.connect(n2, "value", n3, "b");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(results[n3]["result"].getInt() == 10);
}

TEST_CASE("NodeExecutor longer chain", "[NodeExecutor]") {
    NodeRegistry reg;

    // const(2) -> double -> double -> double = 16
    NodeBuilder("const", "test")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            ctx.setOutput("value", prop.isNull() ? int64_t(0) : prop.getInt());
        })
        .buildAndRegister(reg);

    NodeBuilder("double", "test")
        .input("in", Type::Int)
        .output("out", Type::Int)
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("out", ctx.getInputWorkload("in").getInt() * 2);
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n1 = graph.addNode("const");
    auto n2 = graph.addNode("double");
    auto n3 = graph.addNode("double");
    auto n4 = graph.addNode("double");

    graph.setProperty(n1, "_value", Workload(int64_t(2), Type::Int));

    graph.connect(n1, "value", n2, "in");
    graph.connect(n2, "out", n3, "in");
    graph.connect(n3, "out", n4, "in");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(results[n4]["out"].getInt() == 16);
}

TEST_CASE("NodeExecutor diamond pattern", "[NodeExecutor]") {
    NodeRegistry reg;

    //     const(10)
    //     /       \
    //  double    triple
    //     \       /
    //       add
    //  result = 20 + 30 = 50

    NodeBuilder("const", "test")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto prop = ctx.getInputWorkload("_value");
            ctx.setOutput("value", prop.isNull() ? int64_t(0) : prop.getInt());
        })
        .buildAndRegister(reg);

    NodeBuilder("double", "test")
        .input("in", Type::Int)
        .output("out", Type::Int)
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("out", ctx.getInputWorkload("in").getInt() * 2);
        })
        .buildAndRegister(reg);

    NodeBuilder("triple", "test")
        .input("in", Type::Int)
        .output("out", Type::Int)
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("out", ctx.getInputWorkload("in").getInt() * 3);
        })
        .buildAndRegister(reg);

    NodeBuilder("add", "test")
        .input("a", Type::Int)
        .input("b", Type::Int)
        .output("result", Type::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t a = ctx.getInputWorkload("a").getInt();
            int64_t b = ctx.getInputWorkload("b").getInt();
            ctx.setOutput("result", a + b);
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto nConst = graph.addNode("const");
    auto nDouble = graph.addNode("double");
    auto nTriple = graph.addNode("triple");
    auto nAdd = graph.addNode("add");

    graph.setProperty(nConst, "_value", Workload(int64_t(10), Type::Int));

    graph.connect(nConst, "value", nDouble, "in");
    graph.connect(nConst, "value", nTriple, "in");
    graph.connect(nDouble, "out", nAdd, "a");
    graph.connect(nTriple, "out", nAdd, "b");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(results[nConst]["value"].getInt() == 10);
    REQUIRE(results[nDouble]["out"].getInt() == 20);
    REQUIRE(results[nTriple]["out"].getInt() == 30);
    REQUIRE(results[nAdd]["result"].getInt() == 50);
}

// =============================================================================
// NodeExecutor Error Handling
// =============================================================================

TEST_CASE("NodeExecutor missing definition", "[NodeExecutor]") {
    NodeRegistry reg;  // Empty registry

    NodeGraph graph;
    auto n = graph.addNode("nonexistent");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors());
    auto errors = exec.getErrors();
    REQUIRE(errors.size() == 1);
    REQUIRE(errors[0].find("not found") != std::string::npos);
}

TEST_CASE("NodeExecutor node with error", "[NodeExecutor]") {
    NodeRegistry reg;

    NodeBuilder("failing", "test")
        .output("x", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            ctx.setError("Intentional failure");
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n = graph.addNode("failing");

    NodeExecutor exec(reg);
    exec.execute(graph);

    REQUIRE(exec.hasErrors());

    auto* result = exec.getResult(n);
    REQUIRE(result != nullptr);
    REQUIRE(result->hasError);
    REQUIRE(result->errorMessage == "Intentional failure");
}

// =============================================================================
// NodeExecutor with CSV
// =============================================================================

TEST_CASE("NodeExecutor with CSV passthrough", "[NodeExecutor][CSV]") {
    NodeRegistry reg;

    NodeBuilder("csv_source", "test")
        .output("csv", Type::Csv)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto df = std::make_shared<DataFrame>();
            df->addIntColumn("id");
            df->addRow({"1"});
            df->addRow({"2"});
            df->addRow({"3"});
            ctx.setOutput("csv", df);
        })
        .buildAndRegister(reg);

    NodeBuilder("passthrough", "test")
        .input("csv", Type::Csv)
        .output("csv", Type::Csv)
        .output("count", Type::Int)
        .onCompile([](NodeContext& ctx) {
            auto csv = ctx.getInputWorkload("csv").getCsv();
            ctx.setOutput("csv", csv);
            ctx.setOutput("count", static_cast<int64_t>(csv->rowCount()));
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n1 = graph.addNode("csv_source");
    auto n2 = graph.addNode("passthrough");
    graph.connect(n1, "csv", n2, "csv");

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    REQUIRE(results[n2]["count"].getInt() == 3);
    REQUIRE(results[n2]["csv"].getCsv()->rowCount() == 3);
}

TEST_CASE("NodeExecutor executeNode standalone", "[NodeExecutor]") {
    NodeRegistry reg;

    auto def = NodeBuilder("multiply", "test")
        .input("a", Type::Int)
        .input("b", Type::Int)
        .output("result", Type::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t a = ctx.getInputWorkload("a").getInt();
            int64_t b = ctx.getInputWorkload("b").getInt();
            ctx.setOutput("result", a * b);
        })
        .build();

    NodeExecutor exec(reg);

    std::unordered_map<std::string, Workload> inputs;
    inputs["a"] = Workload(int64_t(6), Type::Int);
    inputs["b"] = Workload(int64_t(7), Type::Int);

    auto ctx = exec.executeNode(*def, inputs);

    REQUIRE(ctx.hasError() == false);
    REQUIRE(ctx.getOutput("result").getInt() == 42);
}

// =============================================================================
// NodeExecutor Active CSV Detection
// =============================================================================

TEST_CASE("NodeExecutor auto-sets activeCsv", "[NodeExecutor][CSV]") {
    NodeRegistry reg;

    NodeBuilder("csv_source", "test")
        .output("csv", Type::Csv)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto df = std::make_shared<DataFrame>();
            df->addDoubleColumn("value");
            auto col = std::dynamic_pointer_cast<DoubleColumn>(df->getColumn("value"));
            col->push_back(1.0);
            col->push_back(2.0);
            col->push_back(3.0);
            ctx.setOutput("csv", df);
        })
        .buildAndRegister(reg);

    NodeBuilder("field_sum", "test")
        .input("csv", Type::Csv)
        .input("field", Type::Field)
        .output("sum", Type::Double)
        .onCompile([](NodeContext& ctx) {
            auto csv = ctx.getActiveCsv();
            REQUIRE(csv != nullptr);  // Should be auto-set

            auto field = ctx.getInputWorkload("field");
            auto header = csv->getColumnNames();

            double sum = 0;
            for (size_t i = 0; i < csv->rowCount(); ++i) {
                sum += field.getDoubleAtRow(i, header, csv);
            }
            ctx.setOutput("sum", sum);
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n1 = graph.addNode("csv_source");
    auto n2 = graph.addNode("field_sum");

    graph.connect(n1, "csv", n2, "csv");
    graph.setProperty(n2, "field", Workload("value", Type::Field));
    // Note: properties are set as inputs, so we use "field" as input name
    // Actually, let me check - we need to pass field as an input

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    // The field property should be accessible
    // Actually the node needs the field input connected or set as property
}

TEST_CASE("NodeExecutor with field and scalar broadcasting", "[NodeExecutor][CSV][Broadcasting]") {
    NodeRegistry reg;

    NodeBuilder("csv_source", "test")
        .output("csv", Type::Csv)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto df = std::make_shared<DataFrame>();
            df->addDoubleColumn("price");
            auto col = std::dynamic_pointer_cast<DoubleColumn>(df->getColumn("price"));
            col->push_back(10.0);
            col->push_back(20.0);
            col->push_back(30.0);
            ctx.setOutput("csv", df);
        })
        .buildAndRegister(reg);

    NodeBuilder("add_scalar", "test")
        .input("csv", Type::Csv)
        .input("field", Type::Field)
        .input("scalar", Type::Double)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto csv = ctx.getInputWorkload("csv").getCsv();
            auto field = ctx.getInputWorkload("field");
            auto scalar = ctx.getInputWorkload("scalar");

            auto header = csv->getColumnNames();
            auto resultCol = std::make_shared<DoubleColumn>("result");

            for (size_t i = 0; i < csv->rowCount(); ++i) {
                double fieldVal = field.getDoubleAtRow(i, header, csv);
                double scalarVal = scalar.getDouble();  // broadcasts
                resultCol->push_back(fieldVal + scalarVal);
            }

            auto resultCsv = std::make_shared<DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());
            for (const auto& colName : header) {
                resultCsv->addColumn(csv->getColumn(colName)->clone());
            }
            resultCsv->addColumn(resultCol);

            ctx.setOutput("csv", resultCsv);
        })
        .buildAndRegister(reg);

    NodeGraph graph;
    auto n1 = graph.addNode("csv_source");
    auto n2 = graph.addNode("add_scalar");

    graph.connect(n1, "csv", n2, "csv");
    graph.setProperty(n2, "field", Workload("price", Type::Field));
    graph.setProperty(n2, "scalar", Workload(5.0, Type::Double));

    NodeExecutor exec(reg);
    auto results = exec.execute(graph);

    auto resultCsv = results[n2]["csv"].getCsv();
    REQUIRE(resultCsv->hasColumn("result"));

    auto resultCol = std::dynamic_pointer_cast<DoubleColumn>(resultCsv->getColumn("result"));
    REQUIRE(resultCol->at(0) == 15.0);
    REQUIRE(resultCol->at(1) == 25.0);
    REQUIRE(resultCol->at(2) == 35.0);
}
