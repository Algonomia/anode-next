#include <catch2/catch_test_macros.hpp>
#include "nodes/NodeGraphSerializer.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/nodes/common/ScalarNodes.hpp"
#include "nodes/nodes/common/CsvNodes.hpp"
#include "nodes/nodes/common/MathNodes.hpp"

using namespace nodes;

// Helper to get a fresh registry with test nodes
class SerializerTestFixture {
public:
    SerializerTestFixture() {
        NodeRegistry::instance().clear();
        registerScalarNodes();
        registerCsvNodes();
        registerMathNodes();
    }

    ~SerializerTestFixture() {
        NodeRegistry::instance().clear();
    }
};

// =============================================================================
// Basic Serialization Tests
// =============================================================================

TEST_CASE("Serialize empty graph", "[NodeGraphSerializer]") {
    NodeGraph graph;

    auto j = NodeGraphSerializer::toJson(graph);

    REQUIRE(j.contains("nodes"));
    REQUIRE(j.contains("connections"));
    REQUIRE(j["nodes"].is_array());
    REQUIRE(j["connections"].is_array());
    REQUIRE(j["nodes"].size() == 0);
    REQUIRE(j["connections"].size() == 0);
}

TEST_CASE("Serialize graph with single node", "[NodeGraphSerializer]") {
    SerializerTestFixture fixture;

    NodeGraph graph;
    auto n = graph.addNode("int_value");
    graph.setProperty(n, "_value", Workload(int64_t(42), NodeType::Int));

    auto j = NodeGraphSerializer::toJson(graph);

    REQUIRE(j["nodes"].size() == 1);
    REQUIRE(j["nodes"][0]["id"] == n);
    REQUIRE(j["nodes"][0]["type"] == "int_value");
    REQUIRE(j["nodes"][0]["properties"]["_value"]["value"] == 42);
    REQUIRE(j["nodes"][0]["properties"]["_value"]["type"] == "int");
}

TEST_CASE("Serialize graph with connections", "[NodeGraphSerializer]") {
    SerializerTestFixture fixture;

    NodeGraph graph;
    auto n1 = graph.addNode("int_value");
    auto n2 = graph.addNode("add");

    graph.setProperty(n1, "_value", Workload(int64_t(10), NodeType::Int));
    graph.connect(n1, "value", n2, "a");

    auto j = NodeGraphSerializer::toJson(graph);

    REQUIRE(j["nodes"].size() == 2);
    REQUIRE(j["connections"].size() == 1);
    REQUIRE(j["connections"][0]["from"] == n1);
    REQUIRE(j["connections"][0]["fromPort"] == "value");
    REQUIRE(j["connections"][0]["to"] == n2);
    REQUIRE(j["connections"][0]["toPort"] == "a");
}

// =============================================================================
// Property Type Serialization Tests
// =============================================================================

TEST_CASE("Serialize different property types", "[NodeGraphSerializer]") {
    NodeGraph graph;
    auto n = graph.addNode("test");

    graph.setProperty(n, "int_prop", Workload(int64_t(123), NodeType::Int));
    graph.setProperty(n, "double_prop", Workload(3.14, NodeType::Double));
    graph.setProperty(n, "string_prop", Workload("hello", NodeType::String));
    graph.setProperty(n, "bool_prop", Workload(true));
    graph.setProperty(n, "field_prop", Workload("column_name", NodeType::Field));

    auto j = NodeGraphSerializer::toJson(graph);

    auto& props = j["nodes"][0]["properties"];
    REQUIRE(props["int_prop"]["type"] == "int");
    REQUIRE(props["int_prop"]["value"] == 123);
    REQUIRE(props["double_prop"]["type"] == "double");
    REQUIRE(props["double_prop"]["value"] == 3.14);
    REQUIRE(props["string_prop"]["type"] == "string");
    REQUIRE(props["string_prop"]["value"] == "hello");
    REQUIRE(props["bool_prop"]["type"] == "bool");
    REQUIRE(props["bool_prop"]["value"] == true);
    REQUIRE(props["field_prop"]["type"] == "field");
    REQUIRE(props["field_prop"]["value"] == "column_name");
}

TEST_CASE("Serialize null workload", "[NodeGraphSerializer]") {
    NodeGraph graph;
    auto n = graph.addNode("test");
    graph.setProperty(n, "null_prop", Workload());

    auto j = NodeGraphSerializer::toJson(graph);

    REQUIRE(j["nodes"][0]["properties"]["null_prop"]["type"] == "null");
    REQUIRE(j["nodes"][0]["properties"]["null_prop"]["value"].is_null());
}

// =============================================================================
// Deserialization Tests
// =============================================================================

TEST_CASE("Deserialize empty graph", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", json::array()},
        {"connections", json::array()}
    };

    auto graph = NodeGraphSerializer::fromJson(j);

    REQUIRE(graph.nodeCount() == 0);
    REQUIRE(graph.getConnections().empty());
}

TEST_CASE("Deserialize graph with single node", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {
                {"id", "node_5"},
                {"type", "int_value"},
                {"properties", {
                    {"_value", {{"value", 42}, {"type", "int"}}}
                }}
            }
        }},
        {"connections", json::array()}
    };

    auto graph = NodeGraphSerializer::fromJson(j);

    REQUIRE(graph.nodeCount() == 1);
    auto* node = graph.getNode("node_5");
    REQUIRE(node != nullptr);
    REQUIRE(node->definitionName == "int_value");

    auto prop = graph.getProperty("node_5", "_value");
    REQUIRE(prop.getType() == NodeType::Int);
    REQUIRE(prop.getInt() == 42);
}

TEST_CASE("Deserialize graph with connections", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "node_1"}, {"type", "int_value"}, {"properties", json::object()}},
            {{"id", "node_2"}, {"type", "add"}, {"properties", json::object()}}
        }},
        {"connections", {
            {{"from", "node_1"}, {"fromPort", "value"}, {"to", "node_2"}, {"toPort", "a"}}
        }}
    };

    auto graph = NodeGraphSerializer::fromJson(j);

    REQUIRE(graph.nodeCount() == 2);
    REQUIRE(graph.getConnections().size() == 1);

    auto* conn = graph.getConnectionTo("node_2", "a");
    REQUIRE(conn != nullptr);
    REQUIRE(conn->sourceNodeId == "node_1");
    REQUIRE(conn->sourcePortName == "value");
}

TEST_CASE("Deserialize updates nextId correctly", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "node_10"}, {"type", "test"}, {"properties", json::object()}},
            {{"id", "node_5"}, {"type", "test"}, {"properties", json::object()}}
        }},
        {"connections", json::array()}
    };

    auto graph = NodeGraphSerializer::fromJson(j);

    // Adding a new node should get node_11 (max was 10)
    auto newId = graph.addNode("test");
    REQUIRE(newId == "node_11");
}

// =============================================================================
// Round-trip Tests
// =============================================================================

TEST_CASE("Round-trip: serialize then deserialize", "[NodeGraphSerializer]") {
    SerializerTestFixture fixture;

    // Build original graph
    NodeGraph original;
    auto n1 = original.addNode("int_value");
    auto n2 = original.addNode("int_value");
    auto n3 = original.addNode("add");

    original.setProperty(n1, "_value", Workload(int64_t(10), NodeType::Int));
    original.setProperty(n2, "_value", Workload(int64_t(20), NodeType::Int));

    original.connect(n1, "value", n3, "a");
    original.connect(n2, "value", n3, "b");

    // Serialize
    std::string jsonStr = NodeGraphSerializer::toString(original);

    // Deserialize
    auto restored = NodeGraphSerializer::fromString(jsonStr);

    // Verify structure
    REQUIRE(restored.nodeCount() == original.nodeCount());
    REQUIRE(restored.getConnections().size() == original.getConnections().size());

    // Verify node properties
    REQUIRE(restored.getProperty(n1, "_value").getInt() == 10);
    REQUIRE(restored.getProperty(n2, "_value").getInt() == 20);

    // Verify connections
    auto* conn1 = restored.getConnectionTo(n3, "a");
    auto* conn2 = restored.getConnectionTo(n3, "b");
    REQUIRE(conn1 != nullptr);
    REQUIRE(conn2 != nullptr);
    REQUIRE(conn1->sourceNodeId == n1);
    REQUIRE(conn2->sourceNodeId == n2);
}

TEST_CASE("Round-trip with all property types", "[NodeGraphSerializer]") {
    NodeGraph original;
    auto n = original.addNode("test");

    original.setProperty(n, "int_val", Workload(int64_t(-999), NodeType::Int));
    original.setProperty(n, "double_val", Workload(2.718281828, NodeType::Double));
    original.setProperty(n, "string_val", Workload("test string", NodeType::String));
    original.setProperty(n, "bool_val", Workload(false));
    original.setProperty(n, "field_val", Workload("my_column", NodeType::Field));

    // Round-trip
    auto restored = NodeGraphSerializer::fromString(NodeGraphSerializer::toString(original));

    REQUIRE(restored.getProperty(n, "int_val").getInt() == -999);
    REQUIRE(restored.getProperty(n, "double_val").getDouble() == 2.718281828);
    REQUIRE(restored.getProperty(n, "string_val").getString() == "test string");
    REQUIRE(restored.getProperty(n, "bool_val").getBool() == false);
    REQUIRE(restored.getProperty(n, "field_val").getString() == "my_column");
    REQUIRE(restored.getProperty(n, "field_val").getType() == NodeType::Field);
}

// =============================================================================
// Complex Graph Tests
// =============================================================================

TEST_CASE("Serialize complete pipeline", "[NodeGraphSerializer]") {
    SerializerTestFixture fixture;

    // csv_source -> field(price) -> add(+10) -> result
    NodeGraph graph;
    auto csvNode = graph.addNode("csv_source");
    auto fieldNode = graph.addNode("field");
    auto intNode = graph.addNode("int_value");
    auto addNode = graph.addNode("add");

    graph.setProperty(fieldNode, "_column", Workload("price", NodeType::String));
    graph.setProperty(intNode, "_value", Workload(int64_t(10), NodeType::Int));

    graph.connect(csvNode, "csv", fieldNode, "csv");
    graph.connect(fieldNode, "field", addNode, "a");
    graph.connect(fieldNode, "csv", addNode, "csv");
    graph.connect(intNode, "value", addNode, "b");

    // Serialize
    auto j = NodeGraphSerializer::toJson(graph);

    REQUIRE(j["nodes"].size() == 4);
    REQUIRE(j["connections"].size() == 4);

    // Round-trip and verify
    auto restored = NodeGraphSerializer::fromJson(j);
    REQUIRE(restored.nodeCount() == 4);
    REQUIRE(restored.getConnections().size() == 4);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("Deserialize invalid JSON throws", "[NodeGraphSerializer]") {
    REQUIRE_THROWS(NodeGraphSerializer::fromString("not valid json"));
}

TEST_CASE("Deserialize node without id throws", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"type", "test"}, {"properties", json::object()}}
        }},
        {"connections", json::array()}
    };

    REQUIRE_THROWS_AS(NodeGraphSerializer::fromJson(j), std::runtime_error);
}

TEST_CASE("Deserialize node without type throws", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "node_1"}, {"properties", json::object()}}
        }},
        {"connections", json::array()}
    };

    REQUIRE_THROWS_AS(NodeGraphSerializer::fromJson(j), std::runtime_error);
}

TEST_CASE("Deserialize connection without required fields throws", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "node_1"}, {"type", "test"}, {"properties", json::object()}}
        }},
        {"connections", {
            {{"from", "node_1"}}  // Missing other fields
        }}
    };

    REQUIRE_THROWS_AS(NodeGraphSerializer::fromJson(j), std::runtime_error);
}

// =============================================================================
// String Serialization Tests
// =============================================================================

TEST_CASE("toString produces valid JSON", "[NodeGraphSerializer]") {
    NodeGraph graph;
    auto n = graph.addNode("test");
    graph.setProperty(n, "_value", Workload(int64_t(42), NodeType::Int));

    std::string str = NodeGraphSerializer::toString(graph);

    // Should be valid JSON
    json parsed;
    REQUIRE_NOTHROW(parsed = json::parse(str));
    REQUIRE(parsed.contains("nodes"));

    // Should be parseable back
    auto restored = NodeGraphSerializer::fromString(str);
    REQUIRE(restored.nodeCount() == 1);
}

TEST_CASE("toString with indent formatting", "[NodeGraphSerializer]") {
    NodeGraph graph;
    graph.addNode("test");

    std::string compact = NodeGraphSerializer::toString(graph, -1);
    std::string pretty = NodeGraphSerializer::toString(graph, 2);

    // Pretty should have newlines, compact should not
    REQUIRE(compact.find('\n') == std::string::npos);
    REQUIRE(pretty.find('\n') != std::string::npos);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Deserialize with arbitrary node IDs", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "custom_id_abc"}, {"type", "test"}, {"properties", json::object()}},
            {{"id", "another-id"}, {"type", "test"}, {"properties", json::object()}}
        }},
        {"connections", json::array()}
    };

    auto graph = NodeGraphSerializer::fromJson(j);

    REQUIRE(graph.getNode("custom_id_abc") != nullptr);
    REQUIRE(graph.getNode("another-id") != nullptr);
}

TEST_CASE("Deserialize empty properties object", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "node_1"}, {"type", "test"}, {"properties", json::object()}}
        }},
        {"connections", json::array()}
    };

    auto graph = NodeGraphSerializer::fromJson(j);

    auto* node = graph.getNode("node_1");
    REQUIRE(node != nullptr);
    REQUIRE(node->properties.empty());
}

TEST_CASE("Round-trip CSV property", "[NodeGraphSerializer]") {
    // Create a DataFrame
    auto df = std::make_shared<dataframe::DataFrame>();
    df->addIntColumn("id");
    df->addStringColumn("name");
    df->addDoubleColumn("price");
    df->addRow({"1", "Apple", "1.50"});
    df->addRow({"2", "Banana", "0.75"});

    // Create graph with CSV property
    NodeGraph original;
    auto n = original.addNode("data/csv_source");
    original.setProperty(n, "_identifier", Workload("my_csv", NodeType::String));
    original.setProperty(n, "_csv_data", Workload(df));

    // Serialize to JSON
    auto j = NodeGraphSerializer::toJson(original);

    // Verify JSON structure matches what frontend expects
    auto& props = j["nodes"][0]["properties"];
    REQUIRE(props.contains("_csv_data"));
    REQUIRE(props["_csv_data"]["type"] == "csv");
    REQUIRE(props["_csv_data"].contains("value"));
    REQUIRE(props["_csv_data"]["value"].contains("columns"));
    REQUIRE(props["_csv_data"]["value"].contains("data"));
    REQUIRE(props["_csv_data"]["value"]["columns"].size() == 3);
    REQUIRE(props["_csv_data"]["value"]["data"].size() == 2);

    // Round-trip: serialize → string → deserialize
    std::string jsonStr = NodeGraphSerializer::toString(original);
    auto restored = NodeGraphSerializer::fromString(jsonStr);

    // Verify CSV property survived
    auto csvProp = restored.getProperty(n, "_csv_data");
    REQUIRE(csvProp.getType() == NodeType::Csv);
    REQUIRE(csvProp.isCsv());
    auto restoredDf = csvProp.getCsv();
    REQUIRE(restoredDf != nullptr);
    REQUIRE(restoredDf->rowCount() == 2);
    REQUIRE(restoredDf->getColumnNames().size() == 3);

    // Verify second round-trip produces same JSON
    NodeGraph restored2;
    auto n2 = restored2.addNode("data/csv_source");
    restored2.setProperty(n2, "_csv_data", csvProp);
    auto j2 = NodeGraphSerializer::toJson(restored2);
    auto& props2 = j2["nodes"][0]["properties"];
    REQUIRE(props2["_csv_data"]["value"]["columns"].size() == 3);
    REQUIRE(props2["_csv_data"]["value"]["data"].size() == 2);
    REQUIRE(props2["_csv_data"]["value"]["data"][0][0] == 1);       // int preserved
    REQUIRE(props2["_csv_data"]["value"]["data"][0][1] == "Apple"); // string preserved
    REQUIRE(props2["_csv_data"]["value"]["data"][0][2] == 1.5);     // double preserved
}

TEST_CASE("Deserialize CSV property with wrong type string", "[NodeGraphSerializer]") {
    // The LiteGraph editor may save _csv_data with type:"string" instead of type:"csv"
    // The deserializer should auto-detect CSV from the value structure
    json j = {
        {"nodes", {
            {
                {"id", "node_1"},
                {"type", "data/csv_source"},
                {"properties", {
                    {"_csv_data", {
                        {"type", "string"},  // WRONG type — editor doesn't know about csv
                        {"value", {
                            {"columns", {"id", "question_id", "task_id"}},
                            {"schema", {
                                {{"name", "id"}, {"type", "INT"}},
                                {{"name", "question_id"}, {"type", "INT"}},
                                {{"name", "task_id"}, {"type", "INT"}}
                            }},
                            {"data", {{12361619, 629, 42068}}}
                        }}
                    }}
                }}
            }
        }},
        {"connections", json::array()}
    };

    // Should NOT throw — auto-detect CSV from value structure
    auto graph = NodeGraphSerializer::fromJson(j);

    auto csvProp = graph.getProperty("node_1", "_csv_data");
    REQUIRE(csvProp.getType() == NodeType::Csv);
    REQUIRE(csvProp.isCsv());

    auto df = csvProp.getCsv();
    REQUIRE(df != nullptr);
    REQUIRE(df->rowCount() == 1);
    REQUIRE(df->getColumnNames().size() == 3);

    auto idCol = std::dynamic_pointer_cast<dataframe::IntColumn>(df->getColumn("id"));
    REQUIRE(idCol->at(0) == 12361619);
}

TEST_CASE("Deserialize missing properties key", "[NodeGraphSerializer]") {
    json j = {
        {"nodes", {
            {{"id", "node_1"}, {"type", "test"}}  // No properties key
        }},
        {"connections", json::array()}
    };

    // Should not throw, just create node without properties
    auto graph = NodeGraphSerializer::fromJson(j);
    REQUIRE(graph.nodeCount() == 1);
}
