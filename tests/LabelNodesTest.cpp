#include <catch2/catch_test_macros.hpp>
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeExecutor.hpp"
#include "nodes/LabelRegistry.hpp"
#include "nodes/nodes/common/ScalarNodes.hpp"
#include "nodes/nodes/common/LabelNodes.hpp"

using namespace nodes;

class LabelNodesFixture {
public:
    LabelNodesFixture() {
        NodeRegistry::instance().clear();
        LabelRegistry::instance().clear();
        registerScalarNodes();
        registerLabelNodes();
    }

    ~LabelNodesFixture() {
        NodeRegistry::instance().clear();
        LabelRegistry::instance().clear();
    }
};

// =============================================================================
// Basic Label Tests
// =============================================================================

TEST_CASE("label_define_int stores value in registry", "[Labels]") {
    LabelNodesFixture fixture;

    NodeGraph graph;

    // Create: int_value(42) -> label_define_int("my_label")
    auto intNode = graph.addNode("int_value");
    graph.setProperty(intNode, "_value", Workload(int64_t(42), NodeType::Int));

    auto defineNode = graph.addNode("label_define_int");
    graph.setProperty(defineNode, "_label", Workload("my_label", NodeType::String));

    graph.connect(intNode, "value", defineNode, "value");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);

    // Check that label was stored
    REQUIRE(LabelRegistry::instance().hasLabel("my_label"));
    REQUIRE(LabelRegistry::instance().getLabel("my_label").getInt() == 42);

    // Check pass-through output
    REQUIRE(results[defineNode]["value"].getInt() == 42);
}

TEST_CASE("label_ref_int retrieves value from registry", "[Labels]") {
    LabelNodesFixture fixture;

    NodeGraph graph;

    // Create: int_value(42) -> label_define_int("my_label")
    //         label_ref_int("my_label") (should get 42)
    auto intNode = graph.addNode("int_value");
    graph.setProperty(intNode, "_value", Workload(int64_t(42), NodeType::Int));

    auto defineNode = graph.addNode("label_define_int");
    graph.setProperty(defineNode, "_label", Workload("my_label", NodeType::String));

    auto refNode = graph.addNode("label_ref_int");
    graph.setProperty(refNode, "_label", Workload("my_label", NodeType::String));

    graph.connect(intNode, "value", defineNode, "value");
    // No explicit connection between define and ref - should work via implicit dependency

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    INFO("Errors: ");
    for (const auto& err : exec.getErrors()) {
        INFO(err);
    }

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[refNode]["value"].getInt() == 42);
}

TEST_CASE("label_ref fails when label not defined", "[Labels]") {
    LabelNodesFixture fixture;

    NodeGraph graph;

    // Create: label_ref_int("nonexistent") - should fail
    auto refNode = graph.addNode("label_ref_int");
    graph.setProperty(refNode, "_label", Workload("nonexistent", NodeType::String));

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == true);
}

TEST_CASE("label_define_string and label_ref_string", "[Labels]") {
    LabelNodesFixture fixture;

    NodeGraph graph;

    auto strNode = graph.addNode("string_value");
    graph.setProperty(strNode, "_value", Workload("hello world", NodeType::String));

    auto defineNode = graph.addNode("label_define_string");
    graph.setProperty(defineNode, "_label", Workload("greeting", NodeType::String));

    auto refNode = graph.addNode("label_ref_string");
    graph.setProperty(refNode, "_label", Workload("greeting", NodeType::String));

    graph.connect(strNode, "value", defineNode, "value");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[refNode]["value"].getString() == "hello world");
}

TEST_CASE("multiple refs can use same label", "[Labels]") {
    LabelNodesFixture fixture;

    NodeGraph graph;

    auto intNode = graph.addNode("int_value");
    graph.setProperty(intNode, "_value", Workload(int64_t(100), NodeType::Int));

    auto defineNode = graph.addNode("label_define_int");
    graph.setProperty(defineNode, "_label", Workload("shared", NodeType::String));

    auto ref1 = graph.addNode("label_ref_int");
    graph.setProperty(ref1, "_label", Workload("shared", NodeType::String));

    auto ref2 = graph.addNode("label_ref_int");
    graph.setProperty(ref2, "_label", Workload("shared", NodeType::String));

    graph.connect(intNode, "value", defineNode, "value");

    NodeExecutor exec(NodeRegistry::instance());
    auto results = exec.execute(graph);

    REQUIRE(exec.hasErrors() == false);
    REQUIRE(results[ref1]["value"].getInt() == 100);
    REQUIRE(results[ref2]["value"].getInt() == 100);
}

TEST_CASE("labels are cleared between executions", "[Labels]") {
    LabelNodesFixture fixture;

    // First execution: define a label
    {
        NodeGraph graph;
        auto intNode = graph.addNode("int_value");
        graph.setProperty(intNode, "_value", Workload(int64_t(42), NodeType::Int));

        auto defineNode = graph.addNode("label_define_int");
        graph.setProperty(defineNode, "_label", Workload("temp_label", NodeType::String));

        graph.connect(intNode, "value", defineNode, "value");

        NodeExecutor exec(NodeRegistry::instance());
        exec.execute(graph);

        // Label should exist after first execution
        REQUIRE(LabelRegistry::instance().hasLabel("temp_label"));
    }

    // Second execution: try to use the label (should fail because cleared)
    {
        NodeGraph graph;
        auto refNode = graph.addNode("label_ref_int");
        graph.setProperty(refNode, "_label", Workload("temp_label", NodeType::String));

        NodeExecutor exec(NodeRegistry::instance());
        exec.execute(graph);

        // Should fail because labels are cleared at start of each execution
        REQUIRE(exec.hasErrors() == true);
    }
}
