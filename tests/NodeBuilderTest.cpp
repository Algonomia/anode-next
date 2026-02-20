#include <catch2/catch_test_macros.hpp>
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "dataframe/DataFrame.hpp"

using namespace nodes;
using namespace dataframe;

// =============================================================================
// NodeBuilder Basic Tests
// =============================================================================

TEST_CASE("NodeBuilder minimal node", "[NodeBuilder]") {
    auto def = NodeBuilder("test", "category")
        .output("out", NodeType::Int)
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("out", int64_t(42));
        })
        .build();

    REQUIRE(def->getName() == "test");
    REQUIRE(def->getCategory() == "category");
    REQUIRE(def->getInputs().empty());
    REQUIRE(def->getOutputs().size() == 1);
    REQUIRE(def->isEntryPoint() == false);
}

TEST_CASE("NodeBuilder with single input", "[NodeBuilder]") {
    auto def = NodeBuilder("double_it", "math")
        .input("in", NodeType::Int)
        .output("out", NodeType::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t v = ctx.getInputWorkload("in").getInt();
            ctx.setOutput("out", v * 2);
        })
        .build();

    REQUIRE(def->getInputs().size() == 1);
    REQUIRE(def->getInputs()[0].name == "in");
    REQUIRE(def->getInputs()[0].type.accepts(NodeType::Int));
    REQUIRE(def->getInputs()[0].required == true);
}

TEST_CASE("NodeBuilder with multiple inputs", "[NodeBuilder]") {
    auto def = NodeBuilder("add", "math")
        .input("a", NodeType::Int)
        .input("b", NodeType::Int)
        .output("result", NodeType::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t a = ctx.getInputWorkload("a").getInt();
            int64_t b = ctx.getInputWorkload("b").getInt();
            ctx.setOutput("result", a + b);
        })
        .build();

    REQUIRE(def->getInputs().size() == 2);
    REQUIRE(def->getInputs()[0].name == "a");
    REQUIRE(def->getInputs()[1].name == "b");
}

TEST_CASE("NodeBuilder with multi-type input", "[NodeBuilder]") {
    auto def = NodeBuilder("flexible", "test")
        .input("val", {NodeType::Int, NodeType::Double, NodeType::Field})
        .output("out", NodeType::Double)
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("out", ctx.getInputWorkload("val").getDouble());
        })
        .build();

    auto input = def->findInput("val");
    REQUIRE(input != nullptr);
    REQUIRE(input->type.isMultiType() == true);
    REQUIRE(input->type.accepts(NodeType::Int) == true);
    REQUIRE(input->type.accepts(NodeType::Double) == true);
    REQUIRE(input->type.accepts(NodeType::Field) == true);
    REQUIRE(input->type.accepts(NodeType::String) == false);
}

TEST_CASE("NodeBuilder with optional input", "[NodeBuilder]") {
    auto def = NodeBuilder("with_optional", "test")
        .input("required", NodeType::Int)
        .inputOptional("optional", NodeType::String)
        .output("out", NodeType::Int)
        .onCompile([](NodeContext&) {})
        .build();

    REQUIRE(def->getInputs().size() == 2);

    auto required = def->findInput("required");
    REQUIRE(required->required == true);

    auto optional = def->findInput("optional");
    REQUIRE(optional->required == false);
}

TEST_CASE("NodeBuilder with optional multi-type input", "[NodeBuilder]") {
    auto def = NodeBuilder("test", "test")
        .inputOptional("opt", {NodeType::Int, NodeType::Double})
        .output("out", NodeType::Int)
        .onCompile([](NodeContext&) {})
        .build();

    auto input = def->findInput("opt");
    REQUIRE(input->required == false);
    REQUIRE(input->type.isMultiType() == true);
}

TEST_CASE("NodeBuilder with multiple outputs", "[NodeBuilder]") {
    auto def = NodeBuilder("multi_out", "test")
        .input("in", NodeType::Csv)
        .output("csv", NodeType::Csv)
        .output("count", NodeType::Int)
        .onCompile([](NodeContext&) {})
        .build();

    REQUIRE(def->getOutputs().size() == 2);
    REQUIRE(def->findOutput("csv") != nullptr);
    REQUIRE(def->findOutput("count") != nullptr);
}

TEST_CASE("NodeBuilder with multi-type output", "[NodeBuilder]") {
    auto def = NodeBuilder("test", "test")
        .output("result", {NodeType::Int, NodeType::Double})
        .onCompile([](NodeContext&) {})
        .build();

    auto output = def->findOutput("result");
    REQUIRE(output->type.isMultiType() == true);
}

TEST_CASE("NodeBuilder entry point", "[NodeBuilder]") {
    auto def = NodeBuilder("const", "scalar")
        .output("value", NodeType::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("value", int64_t(100));
        })
        .build();

    REQUIRE(def->isEntryPoint() == true);
    REQUIRE(def->getInputs().empty());
}

TEST_CASE("NodeBuilder compile works", "[NodeBuilder]") {
    auto def = NodeBuilder("triple", "math")
        .input("in", NodeType::Int)
        .output("out", NodeType::Int)
        .onCompile([](NodeContext& ctx) {
            int64_t v = ctx.getInputWorkload("in").getInt();
            ctx.setOutput("out", v * 3);
        })
        .build();

    NodeContext ctx;
    ctx.setInput("in", Workload(int64_t(7), NodeType::Int));

    def->compile(ctx);

    REQUIRE(ctx.getOutput("out").getInt() == 21);
}

// =============================================================================
// NodeBuilder using Type alias
// =============================================================================

TEST_CASE("NodeBuilder with Type alias", "[NodeBuilder]") {
    auto def = NodeBuilder("add", "math")
        .input("a", Type::Int)
        .input("b", Type::Double)
        .output("result", Type::Double)
        .onCompile([](NodeContext& ctx) {
            double a = ctx.getInputWorkload("a").getDouble();
            double b = ctx.getInputWorkload("b").getDouble();
            ctx.setOutput("result", a + b);
        })
        .build();

    REQUIRE(def->findInput("a")->type.accepts(Type::Int));
    REQUIRE(def->findInput("b")->type.accepts(Type::Double));
}

// =============================================================================
// NodeRegistry Basic Tests
// =============================================================================

TEST_CASE("NodeRegistry empty", "[NodeRegistry]") {
    NodeRegistry reg;

    REQUIRE(reg.size() == 0);
    REQUIRE(reg.hasNode("anything") == false);
    REQUIRE(reg.getNode("anything") == nullptr);
    REQUIRE(reg.getNodeNames().empty());
    REQUIRE(reg.getCategories().empty());
}

TEST_CASE("NodeRegistry register and get", "[NodeRegistry]") {
    NodeRegistry reg;

    auto def = NodeBuilder("mynode", "mycat")
        .output("x", NodeType::Int)
        .onCompile([](NodeContext&) {})
        .build();

    reg.registerNode(def);

    REQUIRE(reg.size() == 1);
    REQUIRE(reg.hasNode("mynode") == true);
    REQUIRE(reg.getNode("mynode") == def);
    REQUIRE(reg.getNode("mynode")->getName() == "mynode");
}

TEST_CASE("NodeRegistry unregister", "[NodeRegistry]") {
    NodeRegistry reg;

    auto def = NodeBuilder("temp", "test")
        .output("x", NodeType::Int)
        .onCompile([](NodeContext&) {})
        .build();

    reg.registerNode(def);
    REQUIRE(reg.hasNode("temp") == true);

    reg.unregisterNode("temp");
    REQUIRE(reg.hasNode("temp") == false);
}

TEST_CASE("NodeRegistry getNodeNames", "[NodeRegistry]") {
    NodeRegistry reg;

    reg.registerNode(NodeBuilder("c", "cat").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("a", "cat").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("b", "cat").output("x", Type::Int).onCompile([](NodeContext&) {}).build());

    auto names = reg.getNodeNames();

    REQUIRE(names.size() == 3);
    // Should be sorted
    REQUIRE(names[0] == "a");
    REQUIRE(names[1] == "b");
    REQUIRE(names[2] == "c");
}

TEST_CASE("NodeRegistry getNodeNamesInCategory", "[NodeRegistry]") {
    NodeRegistry reg;

    reg.registerNode(NodeBuilder("add", "math").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("sub", "math").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("int", "scalar").output("x", Type::Int).onCompile([](NodeContext&) {}).build());

    auto mathNodes = reg.getNodeNamesInCategory("math");
    REQUIRE(mathNodes.size() == 2);
    REQUIRE(mathNodes[0] == "add");
    REQUIRE(mathNodes[1] == "sub");

    auto scalarNodes = reg.getNodeNamesInCategory("scalar");
    REQUIRE(scalarNodes.size() == 1);
    REQUIRE(scalarNodes[0] == "int");

    auto unknownNodes = reg.getNodeNamesInCategory("unknown");
    REQUIRE(unknownNodes.empty());
}

TEST_CASE("NodeRegistry getCategories", "[NodeRegistry]") {
    NodeRegistry reg;

    reg.registerNode(NodeBuilder("a", "math").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("b", "scalar").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("c", "math").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("d", "csv").output("x", Type::Int).onCompile([](NodeContext&) {}).build());

    auto categories = reg.getCategories();

    REQUIRE(categories.size() == 3);
    // Sorted (from set)
    REQUIRE(std::find(categories.begin(), categories.end(), "math") != categories.end());
    REQUIRE(std::find(categories.begin(), categories.end(), "scalar") != categories.end());
    REQUIRE(std::find(categories.begin(), categories.end(), "csv") != categories.end());
}

TEST_CASE("NodeRegistry clear", "[NodeRegistry]") {
    NodeRegistry reg;

    reg.registerNode(NodeBuilder("a", "cat").output("x", Type::Int).onCompile([](NodeContext&) {}).build());
    reg.registerNode(NodeBuilder("b", "cat").output("x", Type::Int).onCompile([](NodeContext&) {}).build());

    REQUIRE(reg.size() == 2);

    reg.clear();

    REQUIRE(reg.size() == 0);
    REQUIRE(reg.hasNode("a") == false);
}

TEST_CASE("NodeRegistry overwrite existing", "[NodeRegistry]") {
    NodeRegistry reg;

    auto def1 = NodeBuilder("node", "cat1")
        .output("x", Type::Int)
        .onCompile([](NodeContext&) {})
        .build();

    auto def2 = NodeBuilder("node", "cat2")
        .output("y", Type::Double)
        .onCompile([](NodeContext&) {})
        .build();

    reg.registerNode(def1);
    REQUIRE(reg.getNode("node")->getCategory() == "cat1");

    reg.registerNode(def2);
    REQUIRE(reg.getNode("node")->getCategory() == "cat2");
    REQUIRE(reg.size() == 1);
}

// =============================================================================
// NodeBuilder buildAndRegister Tests
// =============================================================================

TEST_CASE("NodeBuilder buildAndRegister with custom registry", "[NodeBuilder][NodeRegistry]") {
    NodeRegistry reg;

    auto def = NodeBuilder("registered", "test")
        .output("x", Type::Int)
        .onCompile([](NodeContext& ctx) {
            ctx.setOutput("x", int64_t(123));
        })
        .buildAndRegister(reg);

    REQUIRE(reg.hasNode("registered") == true);
    REQUIRE(reg.getNode("registered") == def);

    // Verify it works
    NodeContext ctx;
    def->compile(ctx);
    REQUIRE(ctx.getOutput("x").getInt() == 123);
}

TEST_CASE("NodeBuilder buildAndRegister global registry", "[NodeBuilder][NodeRegistry]") {
    // Clear global registry first
    NodeRegistry::instance().clear();

    auto def = NodeBuilder("global_node", "test")
        .output("x", Type::Int)
        .onCompile([](NodeContext&) {})
        .buildAndRegister();

    REQUIRE(NodeRegistry::instance().hasNode("global_node") == true);
    REQUIRE(NodeRegistry::instance().getNode("global_node") == def);

    // Cleanup
    NodeRegistry::instance().clear();
}

// =============================================================================
// Integration Test: Complete Node with Broadcasting
// =============================================================================

TEST_CASE("NodeBuilder complete add node with broadcasting", "[NodeBuilder][Integration]") {
    auto def = NodeBuilder("add", "math")
        .input("a", {Type::Int, Type::Double, Type::Field})
        .input("b", {Type::Int, Type::Double, Type::Field})
        .inputOptional("csv", Type::Csv)
        .output("result", Type::Double)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto a = ctx.getInputWorkload("a");
            auto b = ctx.getInputWorkload("b");

            bool hasField = (a.getType() == Type::Field || b.getType() == Type::Field);

            if (!hasField) {
                // Pure scalar
                ctx.setOutput("result", a.getDouble() + b.getDouble());
            } else {
                // Vector mode
                auto csv = ctx.getActiveCsv();
                if (!csv) {
                    ctx.setError("Field requires CSV");
                    return;
                }

                auto header = csv->getColumnNames();
                size_t rows = csv->rowCount();

                auto resultCol = std::make_shared<DoubleColumn>("_add_result");
                for (size_t i = 0; i < rows; ++i) {
                    double va = a.getDoubleAtRow(i, header, csv);
                    double vb = b.getDoubleAtRow(i, header, csv);
                    resultCol->push_back(va + vb);
                }

                // Clone CSV
                auto resultCsv = std::make_shared<DataFrame>();
                resultCsv->setStringPool(csv->getStringPool());
                for (const auto& colName : header) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
                resultCsv->addColumn(resultCol);

                ctx.setOutput("csv", resultCsv);
                if (rows > 0) {
                    ctx.setOutput("result", resultCol->at(0));
                }
            }
        })
        .build();

    // Test scalar mode
    SECTION("Scalar + Scalar") {
        NodeContext ctx;
        ctx.setInput("a", Workload(int64_t(5), Type::Int));
        ctx.setInput("b", Workload(3.5, Type::Double));

        def->compile(ctx);

        REQUIRE(ctx.hasError() == false);
        REQUIRE(ctx.getOutput("result").getDouble() == 8.5);
    }

    // Test vector mode
    SECTION("Field + Scalar (broadcasting)") {
        auto csv = std::make_shared<DataFrame>();
        csv->addDoubleColumn("price");
        auto col = std::dynamic_pointer_cast<DoubleColumn>(csv->getColumn("price"));
        col->push_back(10.0);
        col->push_back(20.0);
        col->push_back(30.0);

        NodeContext ctx;
        ctx.setActiveCsv(csv);
        ctx.setInput("a", Workload("price", Type::Field));
        ctx.setInput("b", Workload(5.0, Type::Double));

        def->compile(ctx);

        REQUIRE(ctx.hasError() == false);

        auto resultCsv = ctx.getOutput("csv").getCsv();
        REQUIRE(resultCsv->hasColumn("_add_result"));

        auto resultCol = std::dynamic_pointer_cast<DoubleColumn>(resultCsv->getColumn("_add_result"));
        REQUIRE(resultCol->at(0) == 15.0);
        REQUIRE(resultCol->at(1) == 25.0);
        REQUIRE(resultCol->at(2) == 35.0);
    }
}
