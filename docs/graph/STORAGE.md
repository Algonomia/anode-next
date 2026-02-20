# Graph Storage - Architecture

## Overview

SQLite-based persistence layer for NodeGraphs with versioning support. Each graph is identified by a unique slug and can have multiple saved versions.

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────┐
│  GraphStorage   │────▶│ NodeGraphSerial. │────▶│  NodeGraph  │
│    (SQLite)     │     │     (JSON)       │     │  (runtime)  │
└─────────────────┘     └──────────────────┘     └─────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────┐
│                        SQLite DB                            │
│  ┌─────────────┐          ┌──────────────────┐             │
│  │   graphs    │ 1──────* │  graph_versions  │             │
│  │  (metadata) │          │   (JSON blobs)   │             │
│  └─────────────┘          └──────────────────┘             │
└─────────────────────────────────────────────────────────────┘
```

## File Structure

```
src/storage/
├── GraphMetadata.hpp    # Data structures (GraphMetadata, GraphVersion)
├── GraphStorage.hpp     # Public API
└── GraphStorage.cpp     # SQLite implementation
```

## Database Schema

### Table: `graphs`

Stores graph metadata (one row per graph).

| Column | Type | Description |
|--------|------|-------------|
| `slug` | TEXT PRIMARY KEY | Unique identifier (URL-friendly) |
| `name` | TEXT NOT NULL | Display name |
| `description` | TEXT | Optional description |
| `author` | TEXT | Optional author |
| `tags` | TEXT | JSON array of tags |
| `created_at` | TEXT | ISO 8601 timestamp |
| `updated_at` | TEXT | ISO 8601 timestamp |

### Table: `graph_versions`

Stores graph content versions (many rows per graph).

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY | Auto-incremented version ID |
| `graph_slug` | TEXT NOT NULL | FK to graphs.slug |
| `version_name` | TEXT | Optional version label |
| `graph_json` | TEXT NOT NULL | Serialized NodeGraph (JSON) |
| `created_at` | TEXT | ISO 8601 timestamp |

### Table: `graph_links`

Auto-detected event navigation links between graphs (rebuilt on each save).

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY | Auto-incremented |
| `source_slug` | TEXT NOT NULL | Graph that emits the event |
| `target_slug` | TEXT NOT NULL | Graph targeted by the event |

Constraints: `UNIQUE(source_slug, target_slug)`, `FOREIGN KEY (source_slug) REFERENCES graphs(slug) ON DELETE CASCADE`

### Indexes

```sql
CREATE INDEX idx_versions_graph ON graph_versions(graph_slug);
CREATE INDEX idx_versions_created ON graph_versions(graph_slug, created_at DESC);
CREATE INDEX idx_graph_links_source ON graph_links(source_slug);
CREATE INDEX idx_graph_links_target ON graph_links(target_slug);
```

### Cascade Delete

Deleting a graph automatically deletes all its versions and outgoing links:
```sql
FOREIGN KEY (graph_slug) REFERENCES graphs(slug) ON DELETE CASCADE
```

## Data Structures

### GraphMetadata

```cpp
struct GraphMetadata {
    std::string slug;                 // "my-pipeline"
    std::string name;                 // "My Pipeline"
    std::string description;          // "Processes sales data"
    std::string author;               // "alice"
    std::vector<std::string> tags;    // ["sales", "etl"]
    std::string createdAt;            // "2025-01-15T10:30:00.123Z"
    std::string updatedAt;            // "2025-01-15T14:45:00.456Z"
};
```

### GraphVersion

```cpp
struct GraphVersion {
    int64_t id;                              // 42
    std::string graphSlug;                   // "my-pipeline"
    std::optional<std::string> versionName;  // "v1.0" or nullopt
    std::string graphJson;                   // "{\"nodes\":[...],\"connections\":[...]}"
    std::string createdAt;                   // "2025-01-15T14:45:00.456Z"
};
```

## API Reference

### Constructor

```cpp
GraphStorage(const std::string& dbPath);
```

Opens or creates a SQLite database. Tables are created automatically if they don't exist.

```cpp
storage::GraphStorage db("./data/graphs.db");
```

### Graph CRUD

#### createGraph

```cpp
void createGraph(const GraphMetadata& metadata);
```

Creates a new graph. Timestamps are set automatically. Throws if slug already exists.

```cpp
db.createGraph({
    .slug = "sales-pipeline",
    .name = "Sales Pipeline",
    .description = "ETL for sales data",
    .author = "alice",
    .tags = {"sales", "etl", "daily"}
});
```

#### updateGraph

```cpp
void updateGraph(const GraphMetadata& metadata);
```

Updates metadata (name, description, author, tags). Updates `updated_at` timestamp. Throws if graph doesn't exist.

```cpp
db.updateGraph({
    .slug = "sales-pipeline",
    .name = "Sales Pipeline v2",
    .tags = {"sales", "etl", "hourly"}
});
```

#### deleteGraph

```cpp
void deleteGraph(const std::string& slug);
```

Deletes graph and all its versions (cascade).

```cpp
db.deleteGraph("sales-pipeline");
```

#### getGraph

```cpp
std::optional<GraphMetadata> getGraph(const std::string& slug);
```

Returns graph metadata or `nullopt` if not found.

```cpp
if (auto meta = db.getGraph("sales-pipeline")) {
    std::cout << "Name: " << meta->name << "\n";
    std::cout << "Author: " << meta->author << "\n";
}
```

#### listGraphs

```cpp
std::vector<GraphMetadata> listGraphs();
```

Returns all graphs ordered by `updated_at` DESC (most recent first).

```cpp
for (const auto& graph : db.listGraphs()) {
    std::cout << graph.slug << ": " << graph.name << "\n";
}
```

#### graphExists

```cpp
bool graphExists(const std::string& slug);
```

Returns true if graph exists.

### Version Management

#### saveVersion

```cpp
int64_t saveVersion(const std::string& slug,
                    const nodes::NodeGraph& graph,
                    const std::optional<std::string>& versionName = std::nullopt);
```

Saves a new version. Returns version ID. Updates graph's `updated_at`. Throws if graph doesn't exist.

```cpp
nodes::NodeGraph graph;
// ... build graph ...

// Named version
int64_t v1 = db.saveVersion("sales-pipeline", graph, "Initial release");

// Unnamed version (auto-save)
int64_t v2 = db.saveVersion("sales-pipeline", graph);
```

#### getVersion

```cpp
std::optional<GraphVersion> getVersion(int64_t versionId);
```

Returns a specific version by ID.

```cpp
if (auto v = db.getVersion(42)) {
    std::cout << "Created: " << v->createdAt << "\n";
}
```

#### getLatestVersion

```cpp
std::optional<GraphVersion> getLatestVersion(const std::string& slug);
```

Returns the most recent version of a graph.

```cpp
if (auto latest = db.getLatestVersion("sales-pipeline")) {
    std::cout << "Latest version: " << latest->id << "\n";
}
```

#### listVersions

```cpp
std::vector<GraphVersion> listVersions(const std::string& slug);
```

Returns all versions ordered by `created_at` DESC.

```cpp
for (const auto& v : db.listVersions("sales-pipeline")) {
    std::cout << "v" << v.id << ": "
              << v.versionName.value_or("(unnamed)")
              << " - " << v.createdAt << "\n";
}
```

#### deleteVersion

```cpp
void deleteVersion(int64_t versionId);
```

Deletes a specific version.

### Convenience Methods

#### loadGraph

```cpp
nodes::NodeGraph loadGraph(const std::string& slug);
```

Loads the latest version as a NodeGraph. Throws if graph doesn't exist or has no versions.

```cpp
auto graph = db.loadGraph("sales-pipeline");
// Execute graph...
```

#### loadVersion

```cpp
nodes::NodeGraph loadVersion(int64_t versionId);
```

Loads a specific version as a NodeGraph. Throws if version doesn't exist.

```cpp
auto oldGraph = db.loadVersion(42);
```

## Usage Examples

### Complete Workflow

```cpp
#include "storage/GraphStorage.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeExecutor.hpp"

// Initialize
storage::GraphStorage db("./graphs.db");

// Create a new graph project
db.createGraph({
    .slug = "price-calculator",
    .name = "Price Calculator",
    .description = "Adds markup to base prices",
    .author = "alice",
    .tags = {"pricing", "calculator"}
});

// Build initial graph
nodes::NodeGraph graph;
auto csv = graph.addNode("csv_source");
auto field = graph.addNode("field");
auto markup = graph.addNode("int_value");
auto add = graph.addNode("add");

graph.setProperty(field, "_column", nodes::Workload("price", nodes::NodeType::String));
graph.setProperty(markup, "_value", nodes::Workload(int64_t(10), nodes::NodeType::Int));

graph.connect(csv, "csv", field, "csv");
graph.connect(csv, "csv", add, "csv");
graph.connect(field, "field", add, "src");
graph.connect(markup, "value", add, "operand");

// Save v1
db.saveVersion("price-calculator", graph, "v1.0 - 10% markup");

// Modify and save v2
graph.setProperty(markup, "_value", nodes::Workload(int64_t(15), nodes::NodeType::Int));
db.saveVersion("price-calculator", graph, "v1.1 - 15% markup");

// Later: load and execute latest
auto loaded = db.loadGraph("price-calculator");
nodes::NodeExecutor executor(nodes::NodeRegistry::instance());
auto results = executor.execute(loaded);
```

### Version History UI

```cpp
// Display version history
std::cout << "Version History for " << db.getGraph("price-calculator")->name << ":\n";
std::cout << std::string(50, '-') << "\n";

for (const auto& v : db.listVersions("price-calculator")) {
    std::cout << "  #" << v.id << " - "
              << v.versionName.value_or("(auto-save)")
              << "\n     " << v.createdAt << "\n";
}

// Rollback to specific version
auto oldVersion = db.loadVersion(1);
db.saveVersion("price-calculator", oldVersion, "Rollback to v1.0");
```

### Listing All Graphs

```cpp
std::cout << "Available Graphs:\n";
for (const auto& g : db.listGraphs()) {
    auto versionCount = db.listVersions(g.slug).size();
    std::cout << "  " << g.slug << " (" << versionCount << " versions)\n"
              << "    " << g.name << "\n"
              << "    Updated: " << g.updatedAt << "\n";
}
```

### Graph Links

#### replaceGraphLinks

```cpp
void replaceGraphLinks(const std::string& sourceSlug, const std::vector<std::string>& targetSlugs);
```

Replaces all outgoing links for a graph (delete + re-insert). Called automatically on save after scanning for `timeline_output` event connections.

```cpp
db.replaceGraphLinks("history", {"detail", "summary"});
```

#### getOutgoingLinks

```cpp
std::vector<std::string> getOutgoingLinks(const std::string& slug);
```

Returns target slugs that this graph links to via events.

```cpp
auto targets = db.getOutgoingLinks("history"); // ["detail", "summary"]
```

#### getIncomingLinks

```cpp
std::vector<std::string> getIncomingLinks(const std::string& slug);
```

Returns source slugs that link to this graph via events.

```cpp
auto sources = db.getIncomingLinks("detail"); // ["history"]
```

## Error Handling

All methods throw `std::runtime_error` on failure:

```cpp
try {
    db.createGraph({.slug = "test", .name = "Test"});
    db.createGraph({.slug = "test", .name = "Duplicate"});  // Throws!
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

Common errors:
- Creating graph with duplicate slug
- Updating non-existent graph
- Saving version for non-existent graph
- Loading graph without versions
- Loading non-existent version

## Thread Safety

`GraphStorage` is **NOT thread-safe**. For concurrent access:
- Use one instance per thread, OR
- Wrap calls with a mutex, OR
- Use SQLite's WAL mode with separate connections

## Performance Notes

- Graph JSON is stored uncompressed (SQLite handles compression if needed)
- Indexes optimize: listing versions by graph, getting latest version
- Large graphs (1000+ nodes) serialize to ~100KB JSON
- SQLite handles databases up to 281 TB

## Dependencies

- SQLite3 (downloaded automatically via CMake if not installed)
- nlohmann/json (for tags serialization)
- NodeGraphSerializer (for graph JSON conversion)
