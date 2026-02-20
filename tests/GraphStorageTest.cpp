#include <catch2/catch_test_macros.hpp>
#include "storage/GraphStorage.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/nodes/common/ScalarNodes.hpp"
#include <filesystem>
#include <cstdio>

using namespace storage;
using namespace nodes;

// Helper to create a temporary database file
class TempDatabase {
public:
    TempDatabase() : m_path("/tmp/test_graph_storage_" +
                            std::to_string(std::rand()) + ".db") {}

    ~TempDatabase() {
        std::filesystem::remove(m_path);
    }

    const std::string& path() const { return m_path; }

private:
    std::string m_path;
};

// Helper fixture for node registration
class StorageTestFixture {
public:
    StorageTestFixture() {
        NodeRegistry::instance().clear();
        registerScalarNodes();
    }

    ~StorageTestFixture() {
        NodeRegistry::instance().clear();
    }
};

// =============================================================================
// Graph CRUD Tests
// =============================================================================

TEST_CASE("Create and retrieve graph", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    GraphMetadata meta{
        .slug = "test-graph",
        .name = "Test Graph",
        .description = "A test graph",
        .author = "tester",
        .tags = {"test", "example"},
        .createdAt = "",
        .updatedAt = ""
    };

    db.createGraph(meta);

    auto retrieved = db.getGraph("test-graph");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->slug == "test-graph");
    REQUIRE(retrieved->name == "Test Graph");
    REQUIRE(retrieved->description == "A test graph");
    REQUIRE(retrieved->author == "tester");
    REQUIRE(retrieved->tags.size() == 2);
    REQUIRE(retrieved->tags[0] == "test");
    REQUIRE(retrieved->tags[1] == "example");
    REQUIRE(!retrieved->createdAt.empty());
    REQUIRE(!retrieved->updatedAt.empty());
}

TEST_CASE("Create graph with duplicate slug throws", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    GraphMetadata meta{.slug = "duplicate", .name = "First"};
    db.createGraph(meta);

    meta.name = "Second";
    REQUIRE_THROWS(db.createGraph(meta));
}

TEST_CASE("Update graph metadata", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    GraphMetadata meta{
        .slug = "updatable",
        .name = "Original Name",
        .description = "Original description"
    };
    db.createGraph(meta);

    auto original = db.getGraph("updatable");
    REQUIRE(original.has_value());

    meta.name = "Updated Name";
    meta.description = "Updated description";
    meta.tags = {"new-tag"};
    db.updateGraph(meta);

    auto updated = db.getGraph("updatable");
    REQUIRE(updated.has_value());
    REQUIRE(updated->name == "Updated Name");
    REQUIRE(updated->description == "Updated description");
    REQUIRE(updated->tags.size() == 1);
    REQUIRE(updated->tags[0] == "new-tag");
    REQUIRE(updated->updatedAt > original->updatedAt);
}

TEST_CASE("Update non-existent graph throws", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    GraphMetadata meta{.slug = "non-existent", .name = "Test"};
    REQUIRE_THROWS(db.updateGraph(meta));
}

TEST_CASE("Delete graph", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "to-delete", .name = "Delete Me"});
    REQUIRE(db.graphExists("to-delete"));

    db.deleteGraph("to-delete");
    REQUIRE_FALSE(db.graphExists("to-delete"));
    REQUIRE_FALSE(db.getGraph("to-delete").has_value());
}

TEST_CASE("List graphs ordered by updated_at", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "graph-a", .name = "Graph A"});
    db.createGraph({.slug = "graph-b", .name = "Graph B"});
    db.createGraph({.slug = "graph-c", .name = "Graph C"});

    // Update graph-a to make it most recent
    db.updateGraph({.slug = "graph-a", .name = "Graph A Updated"});

    auto graphs = db.listGraphs();
    REQUIRE(graphs.size() == 3);
    REQUIRE(graphs[0].slug == "graph-a");  // Most recently updated
}

TEST_CASE("Get non-existent graph returns nullopt", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    auto result = db.getGraph("does-not-exist");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("graphExists returns correct values", "[GraphStorage][CRUD]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    REQUIRE_FALSE(db.graphExists("test"));
    db.createGraph({.slug = "test", .name = "Test"});
    REQUIRE(db.graphExists("test"));
}

// =============================================================================
// Version Management Tests
// =============================================================================

TEST_CASE("Save and retrieve version", "[GraphStorage][Version]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "versioned", .name = "Versioned Graph"});

    NodeGraph graph;
    auto n = graph.addNode("int_value");
    graph.setProperty(n, "_value", Workload(int64_t(42), NodeType::Int));

    int64_t versionId = db.saveVersion("versioned", graph, "v1.0");
    REQUIRE(versionId > 0);

    auto version = db.getVersion(versionId);
    REQUIRE(version.has_value());
    REQUIRE(version->id == versionId);
    REQUIRE(version->graphSlug == "versioned");
    REQUIRE(version->versionName.has_value());
    REQUIRE(version->versionName.value() == "v1.0");
    REQUIRE(!version->graphJson.empty());
    REQUIRE(!version->createdAt.empty());
}

TEST_CASE("Save version without name", "[GraphStorage][Version]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "unnamed", .name = "Unnamed Versions"});

    NodeGraph graph;
    graph.addNode("int_value");

    int64_t versionId = db.saveVersion("unnamed", graph);

    auto version = db.getVersion(versionId);
    REQUIRE(version.has_value());
    REQUIRE_FALSE(version->versionName.has_value());
}

TEST_CASE("Save version for non-existent graph throws", "[GraphStorage][Version]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    NodeGraph graph;
    REQUIRE_THROWS(db.saveVersion("non-existent", graph));
}

TEST_CASE("Get latest version", "[GraphStorage][Version]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "multi-version", .name = "Multi Version"});

    NodeGraph graph1;
    graph1.addNode("int_value");
    db.saveVersion("multi-version", graph1, "v1");

    NodeGraph graph2;
    graph2.addNode("int_value");
    graph2.addNode("int_value");
    int64_t latestId = db.saveVersion("multi-version", graph2, "v2");

    auto latest = db.getLatestVersion("multi-version");
    REQUIRE(latest.has_value());
    REQUIRE(latest->id == latestId);
    REQUIRE(latest->versionName.value() == "v2");
}

TEST_CASE("Get latest version of graph without versions returns nullopt", "[GraphStorage][Version]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "empty", .name = "No Versions"});

    auto latest = db.getLatestVersion("empty");
    REQUIRE_FALSE(latest.has_value());
}

TEST_CASE("List versions ordered by created_at DESC", "[GraphStorage][Version]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "ordered", .name = "Ordered Versions"});

    NodeGraph graph;
    graph.addNode("int_value");

    db.saveVersion("ordered", graph, "v1");
    db.saveVersion("ordered", graph, "v2");
    db.saveVersion("ordered", graph, "v3");

    auto versions = db.listVersions("ordered");
    REQUIRE(versions.size() == 3);
    REQUIRE(versions[0].versionName.value() == "v3");  // Most recent first
    REQUIRE(versions[1].versionName.value() == "v2");
    REQUIRE(versions[2].versionName.value() == "v1");
}

TEST_CASE("Delete version", "[GraphStorage][Version]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "deletable", .name = "Deletable"});

    NodeGraph graph;
    graph.addNode("int_value");
    int64_t v1 = db.saveVersion("deletable", graph, "v1");
    int64_t v2 = db.saveVersion("deletable", graph, "v2");

    REQUIRE(db.listVersions("deletable").size() == 2);

    db.deleteVersion(v1);

    auto versions = db.listVersions("deletable");
    REQUIRE(versions.size() == 1);
    REQUIRE(versions[0].id == v2);

    REQUIRE_FALSE(db.getVersion(v1).has_value());
}

// =============================================================================
// Cascade Delete Tests
// =============================================================================

TEST_CASE("Delete graph cascades to versions", "[GraphStorage][Cascade]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "cascade", .name = "Cascade Test"});

    NodeGraph graph;
    graph.addNode("int_value");
    int64_t v1 = db.saveVersion("cascade", graph, "v1");
    int64_t v2 = db.saveVersion("cascade", graph, "v2");

    REQUIRE(db.getVersion(v1).has_value());
    REQUIRE(db.getVersion(v2).has_value());

    db.deleteGraph("cascade");

    REQUIRE_FALSE(db.getVersion(v1).has_value());
    REQUIRE_FALSE(db.getVersion(v2).has_value());
    REQUIRE(db.listVersions("cascade").empty());
}

// =============================================================================
// Load Graph Tests (Round-trip)
// =============================================================================

TEST_CASE("Load graph round-trip", "[GraphStorage][Roundtrip]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "roundtrip", .name = "Round Trip"});

    // Create a graph with nodes and connections
    NodeGraph original;
    auto n1 = original.addNode("int_value");
    auto n2 = original.addNode("int_value");
    original.setProperty(n1, "_value", Workload(int64_t(100), NodeType::Int));
    original.setProperty(n2, "_value", Workload(int64_t(200), NodeType::Int));

    db.saveVersion("roundtrip", original, "v1");

    // Load and verify
    NodeGraph loaded = db.loadGraph("roundtrip");

    REQUIRE(loaded.nodeCount() == 2);
    REQUIRE(loaded.getProperty(n1, "_value").getInt() == 100);
    REQUIRE(loaded.getProperty(n2, "_value").getInt() == 200);
}

TEST_CASE("Load specific version", "[GraphStorage][Roundtrip]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "specific", .name = "Specific Version"});

    NodeGraph v1Graph;
    v1Graph.addNode("int_value");
    int64_t v1Id = db.saveVersion("specific", v1Graph, "v1");

    NodeGraph v2Graph;
    v2Graph.addNode("int_value");
    v2Graph.addNode("int_value");
    db.saveVersion("specific", v2Graph, "v2");

    // Load v1 specifically
    NodeGraph loadedV1 = db.loadVersion(v1Id);
    REQUIRE(loadedV1.nodeCount() == 1);

    // loadGraph should return latest (v2)
    NodeGraph loadedLatest = db.loadGraph("specific");
    REQUIRE(loadedLatest.nodeCount() == 2);
}

TEST_CASE("Load graph without versions throws", "[GraphStorage][Roundtrip]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "no-versions", .name = "No Versions"});

    REQUIRE_THROWS(db.loadGraph("no-versions"));
}

TEST_CASE("Load non-existent version throws", "[GraphStorage][Roundtrip]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    REQUIRE_THROWS(db.loadVersion(99999));
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Empty tags", "[GraphStorage][EdgeCase]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "no-tags", .name = "No Tags", .tags = {}});

    auto retrieved = db.getGraph("no-tags");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->tags.empty());
}

TEST_CASE("Special characters in metadata", "[GraphStorage][EdgeCase]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({
        .slug = "special-chars",
        .name = "Graph with 'quotes' and \"double quotes\"",
        .description = "Description with\nnewlines\tand\ttabs",
        .author = "O'Connor"
    });

    auto retrieved = db.getGraph("special-chars");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->name == "Graph with 'quotes' and \"double quotes\"");
    REQUIRE(retrieved->description == "Description with\nnewlines\tand\ttabs");
    REQUIRE(retrieved->author == "O'Connor");
}

TEST_CASE("Database persistence across instances", "[GraphStorage][Persistence]") {
    TempDatabase tempDb;

    // Create and save
    {
        GraphStorage db(tempDb.path());
        db.createGraph({.slug = "persistent", .name = "Persistent Graph"});
    }

    // Reopen and verify
    {
        GraphStorage db(tempDb.path());
        auto retrieved = db.getGraph("persistent");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Persistent Graph");
    }
}

TEST_CASE("Version saves update graph updated_at", "[GraphStorage][Timestamp]") {
    StorageTestFixture fixture;
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "timestamp-test", .name = "Timestamp"});

    auto beforeVersion = db.getGraph("timestamp-test");
    REQUIRE(beforeVersion.has_value());

    NodeGraph graph;
    graph.addNode("int_value");
    db.saveVersion("timestamp-test", graph);

    auto afterVersion = db.getGraph("timestamp-test");
    REQUIRE(afterVersion.has_value());
    REQUIRE(afterVersion->updatedAt >= beforeVersion->updatedAt);
}

// =============================================================================
// Parameter Overrides Tests
// =============================================================================

TEST_CASE("Set and get parameter overrides", "[GraphStorage][ParameterOverrides]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "param-test", .name = "Param Test"});

    // Set overrides
    db.setParameterOverride("param-test", "threshold", R"({"type":"int","value":42})");
    db.setParameterOverride("param-test", "name", R"({"type":"string","value":"hello"})");

    auto overrides = db.getParameterOverrides("param-test");
    REQUIRE(overrides.size() == 2);
    REQUIRE(overrides["threshold"] == R"({"type":"int","value":42})");
    REQUIRE(overrides["name"] == R"({"type":"string","value":"hello"})");
}

TEST_CASE("Set parameter override replaces existing", "[GraphStorage][ParameterOverrides]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "param-test", .name = "Param Test"});

    db.setParameterOverride("param-test", "threshold", R"({"type":"int","value":42})");
    db.setParameterOverride("param-test", "threshold", R"({"type":"int","value":99})");

    auto overrides = db.getParameterOverrides("param-test");
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides["threshold"] == R"({"type":"int","value":99})");
}

TEST_CASE("Delete parameter override", "[GraphStorage][ParameterOverrides]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "param-test", .name = "Param Test"});

    db.setParameterOverride("param-test", "a", R"({"type":"int","value":1})");
    db.setParameterOverride("param-test", "b", R"({"type":"int","value":2})");

    db.deleteParameterOverride("param-test", "a");

    auto overrides = db.getParameterOverrides("param-test");
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides.find("a") == overrides.end());
    REQUIRE(overrides["b"] == R"({"type":"int","value":2})");
}

TEST_CASE("Clear parameter overrides", "[GraphStorage][ParameterOverrides]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "param-test", .name = "Param Test"});

    db.setParameterOverride("param-test", "a", R"({"type":"int","value":1})");
    db.setParameterOverride("param-test", "b", R"({"type":"int","value":2})");

    db.clearParameterOverrides("param-test");

    auto overrides = db.getParameterOverrides("param-test");
    REQUIRE(overrides.empty());
}

TEST_CASE("Delete graph cascades to parameter overrides", "[GraphStorage][ParameterOverrides]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "param-test", .name = "Param Test"});
    db.setParameterOverride("param-test", "x", R"({"type":"int","value":1})");

    db.deleteGraph("param-test");

    // Overrides should be gone due to CASCADE
    auto overrides = db.getParameterOverrides("param-test");
    REQUIRE(overrides.empty());
}

TEST_CASE("Get parameter overrides for non-existent graph returns empty", "[GraphStorage][ParameterOverrides]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    auto overrides = db.getParameterOverrides("no-such-graph");
    REQUIRE(overrides.empty());
}

// =============================================================================
// Graph Links Tests
// =============================================================================

TEST_CASE("Replace and get outgoing links", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "source", .name = "Source"});

    db.replaceGraphLinks("source", {"target-a", "target-b"});

    auto outgoing = db.getOutgoingLinks("source");
    REQUIRE(outgoing.size() == 2);
    REQUIRE(outgoing[0] == "target-a");
    REQUIRE(outgoing[1] == "target-b");
}

TEST_CASE("Get incoming links", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "source-a", .name = "Source A"});
    db.createGraph({.slug = "source-b", .name = "Source B"});
    db.createGraph({.slug = "target", .name = "Target"});

    db.replaceGraphLinks("source-a", {"target"});
    db.replaceGraphLinks("source-b", {"target"});

    auto incoming = db.getIncomingLinks("target");
    REQUIRE(incoming.size() == 2);
    REQUIRE(incoming[0] == "source-a");
    REQUIRE(incoming[1] == "source-b");
}

TEST_CASE("Replace links overwrites previous links", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "source", .name = "Source"});

    db.replaceGraphLinks("source", {"old-target"});
    REQUIRE(db.getOutgoingLinks("source").size() == 1);

    db.replaceGraphLinks("source", {"new-a", "new-b"});
    auto outgoing = db.getOutgoingLinks("source");
    REQUIRE(outgoing.size() == 2);
    REQUIRE(outgoing[0] == "new-a");
    REQUIRE(outgoing[1] == "new-b");
}

TEST_CASE("Replace links with empty vector clears links", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "source", .name = "Source"});

    db.replaceGraphLinks("source", {"target"});
    REQUIRE(db.getOutgoingLinks("source").size() == 1);

    db.replaceGraphLinks("source", {});
    REQUIRE(db.getOutgoingLinks("source").empty());
}

TEST_CASE("Duplicate targets are ignored", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "source", .name = "Source"});

    db.replaceGraphLinks("source", {"target", "target", "target"});

    auto outgoing = db.getOutgoingLinks("source");
    REQUIRE(outgoing.size() == 1);
    REQUIRE(outgoing[0] == "target");
}

TEST_CASE("Delete graph cascades to graph links", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    db.createGraph({.slug = "source", .name = "Source"});
    db.createGraph({.slug = "target", .name = "Target"});

    db.replaceGraphLinks("source", {"target"});
    REQUIRE(db.getOutgoingLinks("source").size() == 1);
    REQUIRE(db.getIncomingLinks("target").size() == 1);

    db.deleteGraph("source");

    REQUIRE(db.getOutgoingLinks("source").empty());
    REQUIRE(db.getIncomingLinks("target").empty());
}

TEST_CASE("Get links for non-existent graph returns empty", "[GraphStorage][GraphLinks]") {
    TempDatabase tempDb;
    GraphStorage db(tempDb.path());

    REQUIRE(db.getOutgoingLinks("no-such-graph").empty());
    REQUIRE(db.getIncomingLinks("no-such-graph").empty());
}
