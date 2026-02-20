#include <catch2/catch_test_macros.hpp>
#include "nodes/NodeContext.hpp"
#include "nodes/NodeDefinition.hpp"
#include "dataframe/DataFrame.hpp"

using namespace nodes;
using namespace dataframe;

// =============================================================================
// NodeContext Basic Tests
// =============================================================================

TEST_CASE("NodeContext default state", "[NodeContext]") {
    NodeContext ctx;

    REQUIRE(ctx.hasError() == false);
    REQUIRE(ctx.getErrorMessage().empty());
    REQUIRE(ctx.getInputs().empty());
    REQUIRE(ctx.getOutputs().empty());
    REQUIRE(ctx.getActiveCsv() == nullptr);
}

TEST_CASE("NodeContext setInput/getInputWorkload", "[NodeContext]") {
    NodeContext ctx;

    ctx.setInput("a", Workload(int64_t(42), NodeType::Int));

    REQUIRE(ctx.hasInput("a") == true);
    REQUIRE(ctx.hasInput("b") == false);

    auto w = ctx.getInputWorkload("a");
    REQUIRE(w.getType() == NodeType::Int);
    REQUIRE(w.getInt() == 42);
}

TEST_CASE("NodeContext getInputWorkload missing returns null", "[NodeContext]") {
    NodeContext ctx;

    auto w = ctx.getInputWorkload("nonexistent");
    REQUIRE(w.isNull() == true);
}

TEST_CASE("NodeContext setOutput/getOutput", "[NodeContext]") {
    NodeContext ctx;

    ctx.setOutput("result", Workload(int64_t(100), NodeType::Int));

    auto w = ctx.getOutput("result");
    REQUIRE(w.getType() == NodeType::Int);
    REQUIRE(w.getInt() == 100);
}

TEST_CASE("NodeContext setOutput convenience - int64_t", "[NodeContext]") {
    NodeContext ctx;

    ctx.setOutput("x", int64_t(42));

    auto w = ctx.getOutput("x");
    REQUIRE(w.getType() == NodeType::Int);
    REQUIRE(w.getInt() == 42);
}

TEST_CASE("NodeContext setOutput convenience - double", "[NodeContext]") {
    NodeContext ctx;

    ctx.setOutput("x", 3.14);

    auto w = ctx.getOutput("x");
    REQUIRE(w.getType() == NodeType::Double);
    REQUIRE(w.getDouble() == 3.14);
}

TEST_CASE("NodeContext setOutput convenience - string", "[NodeContext]") {
    NodeContext ctx;

    ctx.setOutput("x", std::string("hello"));

    auto w = ctx.getOutput("x");
    REQUIRE(w.getType() == NodeType::String);
    REQUIRE(w.getString() == "hello");
}

TEST_CASE("NodeContext setOutput convenience - const char*", "[NodeContext]") {
    NodeContext ctx;

    ctx.setOutput("x", "world");

    auto w = ctx.getOutput("x");
    REQUIRE(w.getType() == NodeType::String);
    REQUIRE(w.getString() == "world");
}

TEST_CASE("NodeContext setOutput convenience - bool", "[NodeContext]") {
    NodeContext ctx;

    ctx.setOutput("x", true);

    auto w = ctx.getOutput("x");
    REQUIRE(w.getType() == NodeType::Bool);
    REQUIRE(w.getBool() == true);
}

TEST_CASE("NodeContext setOutput convenience - DataFrame", "[NodeContext]") {
    NodeContext ctx;

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("id");

    ctx.setOutput("csv", df);

    auto w = ctx.getOutput("csv");
    REQUIRE(w.getType() == NodeType::Csv);
    REQUIRE(w.getCsv() == df);
}

// =============================================================================
// NodeContext Error Handling
// =============================================================================

TEST_CASE("NodeContext setError", "[NodeContext]") {
    NodeContext ctx;

    REQUIRE(ctx.hasError() == false);

    ctx.setError("Something went wrong");

    REQUIRE(ctx.hasError() == true);
    REQUIRE(ctx.getErrorMessage() == "Something went wrong");
}

// =============================================================================
// NodeContext Active CSV
// =============================================================================

TEST_CASE("NodeContext setActiveCsv", "[NodeContext]") {
    NodeContext ctx;

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("x");

    ctx.setActiveCsv(df);

    REQUIRE(ctx.getActiveCsv() == df);
}

TEST_CASE("NodeContext auto-detects CSV from input", "[NodeContext]") {
    NodeContext ctx;

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("x");

    ctx.setInput("csv", Workload(df));

    // Should auto-set active CSV
    REQUIRE(ctx.getActiveCsv() == df);
}

// =============================================================================
// NodeContext Broadcasting with Active CSV
// =============================================================================

TEST_CASE("NodeContext getDoubleAtRow with scalar", "[NodeContext][Broadcasting]") {
    NodeContext ctx;

    ctx.setInput("val", Workload(3.14, NodeType::Double));

    // No CSV, should just return scalar
    REQUIRE(ctx.getDoubleAtRow("val", 0) == 3.14);
    REQUIRE(ctx.getDoubleAtRow("val", 1) == 3.14);
}

TEST_CASE("NodeContext getDoubleAtRow with field", "[NodeContext][Broadcasting]") {
    NodeContext ctx;

    // Create CSV
    auto df = std::make_shared<DataFrame>();
    df->addDoubleColumn("price");
    auto col = std::dynamic_pointer_cast<DoubleColumn>(df->getColumn("price"));
    col->push_back(1.5);
    col->push_back(2.5);
    col->push_back(3.5);

    ctx.setActiveCsv(df);
    ctx.setInput("val", Workload("price", NodeType::Field));

    REQUIRE(ctx.getDoubleAtRow("val", 0) == 1.5);
    REQUIRE(ctx.getDoubleAtRow("val", 1) == 2.5);
    REQUIRE(ctx.getDoubleAtRow("val", 2) == 3.5);
}

TEST_CASE("NodeContext getIntAtRow with field", "[NodeContext][Broadcasting]") {
    NodeContext ctx;

    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("count");
    df->addRow({"10"});
    df->addRow({"20"});
    df->addRow({"30"});

    ctx.setActiveCsv(df);
    ctx.setInput("val", Workload("count", NodeType::Field));

    REQUIRE(ctx.getIntAtRow("val", 0) == 10);
    REQUIRE(ctx.getIntAtRow("val", 1) == 20);
    REQUIRE(ctx.getIntAtRow("val", 2) == 30);
}

TEST_CASE("NodeContext getStringAtRow with field", "[NodeContext][Broadcasting]") {
    NodeContext ctx;

    auto df = std::make_shared<DataFrame>();
    df->addStringColumn("name");
    df->addRow({"Alice"});
    df->addRow({"Bob"});

    ctx.setActiveCsv(df);
    ctx.setInput("val", Workload("name", NodeType::Field));

    REQUIRE(ctx.getStringAtRow("val", 0) == "Alice");
    REQUIRE(ctx.getStringAtRow("val", 1) == "Bob");
}

// =============================================================================
// NodeDefinition Tests
// =============================================================================

TEST_CASE("NodeDefinition basic properties", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "add", "math",
        std::vector<InputDef>{
            InputDef("a", PortType(NodeType::Int)),
            InputDef("b", PortType(NodeType::Int))
        },
        std::vector<OutputDef>{
            OutputDef("result", PortType(NodeType::Int))
        },
        [](NodeContext&) {},
        false
    );

    REQUIRE(def->getName() == "add");
    REQUIRE(def->getCategory() == "math");
    REQUIRE(def->getInputs().size() == 2);
    REQUIRE(def->getOutputs().size() == 1);
    REQUIRE(def->isEntryPoint() == false);
}

TEST_CASE("NodeDefinition findInput", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "test", "cat",
        std::vector<InputDef>{
            InputDef("x", PortType(NodeType::Int)),
            InputDef("y", PortType(NodeType::Double))
        },
        std::vector<OutputDef>{},
        [](NodeContext&) {},
        false
    );

    auto inputX = def->findInput("x");
    REQUIRE(inputX != nullptr);
    REQUIRE(inputX->name == "x");
    REQUIRE(inputX->type.accepts(NodeType::Int) == true);

    auto inputY = def->findInput("y");
    REQUIRE(inputY != nullptr);
    REQUIRE(inputY->name == "y");

    auto inputZ = def->findInput("z");
    REQUIRE(inputZ == nullptr);
}

TEST_CASE("NodeDefinition findOutput", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "test", "cat",
        std::vector<InputDef>{},
        std::vector<OutputDef>{
            OutputDef("out", PortType(NodeType::Int))
        },
        [](NodeContext&) {},
        false
    );

    auto output = def->findOutput("out");
    REQUIRE(output != nullptr);
    REQUIRE(output->name == "out");

    auto missing = def->findOutput("missing");
    REQUIRE(missing == nullptr);
}

TEST_CASE("NodeDefinition entry point", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "const", "scalar",
        std::vector<InputDef>{},  // No inputs
        std::vector<OutputDef>{
            OutputDef("value", PortType(NodeType::Int))
        },
        [](NodeContext&) {},
        true  // Entry point
    );

    REQUIRE(def->isEntryPoint() == true);
}

TEST_CASE("NodeDefinition compile executes function", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "double_it", "math",
        std::vector<InputDef>{
            InputDef("in", PortType(NodeType::Int))
        },
        std::vector<OutputDef>{
            OutputDef("out", PortType(NodeType::Int))
        },
        [](NodeContext& ctx) {
            int64_t v = ctx.getInputWorkload("in").getInt();
            ctx.setOutput("out", v * 2);
        },
        false
    );

    NodeContext ctx;
    ctx.setInput("in", Workload(int64_t(5), NodeType::Int));

    def->compile(ctx);

    REQUIRE(ctx.hasError() == false);
    REQUIRE(ctx.getOutput("out").getInt() == 10);
}

TEST_CASE("NodeDefinition compile with error", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "fail", "test",
        std::vector<InputDef>{},
        std::vector<OutputDef>{},
        [](NodeContext& ctx) {
            ctx.setError("Intentional failure");
        },
        true
    );

    NodeContext ctx;
    def->compile(ctx);

    REQUIRE(ctx.hasError() == true);
    REQUIRE(ctx.getErrorMessage() == "Intentional failure");
}

TEST_CASE("NodeDefinition with multi-type input", "[NodeDefinition]") {
    auto def = std::make_shared<NodeDefinition>(
        "add", "math",
        std::vector<InputDef>{
            InputDef("a", PortType({NodeType::Int, NodeType::Double, NodeType::Field})),
            InputDef("b", PortType({NodeType::Int, NodeType::Double, NodeType::Field}))
        },
        std::vector<OutputDef>{
            OutputDef("result", PortType(NodeType::Double))
        },
        [](NodeContext& ctx) {
            auto a = ctx.getInputWorkload("a");
            auto b = ctx.getInputWorkload("b");
            ctx.setOutput("result", a.getDouble() + b.getDouble());
        },
        false
    );

    auto inputA = def->findInput("a");
    REQUIRE(inputA->type.isMultiType() == true);
    REQUIRE(inputA->type.accepts(NodeType::Int) == true);
    REQUIRE(inputA->type.accepts(NodeType::Double) == true);
    REQUIRE(inputA->type.accepts(NodeType::Field) == true);
    REQUIRE(inputA->type.accepts(NodeType::String) == false);

    // Test compile
    NodeContext ctx;
    ctx.setInput("a", Workload(int64_t(5), NodeType::Int));
    ctx.setInput("b", Workload(3.5, NodeType::Double));

    def->compile(ctx);

    REQUIRE(ctx.getOutput("result").getDouble() == 8.5);
}
