# Node Graph Serialization Format

## Overview

The `NodeGraphSerializer` class provides JSON serialization/deserialization for `NodeGraph` objects. This enables saving graphs to files, sending them to frontends (graph editors), and restoring them.

## JSON Schema

```json
{
  "nodes": [
    {
      "id": "string",
      "type": "string",
      "properties": {
        "property_name": {
          "value": "any",
          "type": "string"
        }
      },
      "position": [100, 200]
    }
  ],
  "connections": [
    {
      "from": "string",
      "fromPort": "string",
      "to": "string",
      "toPort": "string"
    }
  ],
  "groups": [
    {
      "title": "string",
      "bounding": [100, 200, 300, 150],
      "color": "#3f789e",
      "font_size": 24
    }
  ]
}
```

## Fields

### Root Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `nodes` | array | Yes | List of node instances |
| `connections` | array | Yes | List of connections between nodes |
| `groups` | array | No | List of visual groups (for UI organization) |

### Node Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | Yes | Unique node instance ID (e.g., "node_1", "my_custom_id") |
| `type` | string | Yes | Node definition name (e.g., "add", "csv_source", "field") |
| `properties` | object | No | Key-value map of node properties (widget values) |
| `position` | array | No | Node position for UI as `[x, y]` coordinates |

### Property Value Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `value` | any | No | The property value (type depends on `type` field) |
| `type` | string | Yes | Value type: "int", "double", "string", "bool", "null", "field", "csv" |

### Connection Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `from` | string | Yes | Source node ID |
| `fromPort` | string | Yes | Source port name |
| `to` | string | Yes | Target node ID |
| `toPort` | string | Yes | Target port name |

### Group Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `title` | string | No | Group display name (default: "Group") |
| `bounding` | array | Yes | Position and size as `[x, y, width, height]` |
| `color` | string | No | Group color (hex, e.g., "#3f789e") |
| `font_size` | number | No | Title font size (default: 24) |

## Property Types

| Type | JSON Value Type | Example | Description |
|------|-----------------|---------|-------------|
| `int` | number (integer) | `{"value": 42, "type": "int"}` | 64-bit signed integer |
| `double` | number (float) | `{"value": 3.14, "type": "double"}` | Double precision float |
| `string` | string | `{"value": "hello", "type": "string"}` | UTF-8 string |
| `bool` | boolean | `{"value": true, "type": "bool"}` | Boolean |
| `null` | null | `{"value": null, "type": "null"}` | Null/empty value |
| `field` | string | `{"value": "price", "type": "field"}` | Column reference (name) |
| `csv` | (none) | `{"type": "csv"}` | CSV marker (data not serialized) |

### CSV Type Note

The `csv` type is serialized as a marker only (`{"type": "csv"}`). The actual DataFrame data is **not** included in the JSON. When deserializing, CSV properties are restored as null workloads. The frontend should request CSV data separately when needed.

## Examples

### Simple Graph: Two Integers Added

```json
{
  "nodes": [
    {
      "id": "node_1",
      "type": "int_value",
      "properties": {
        "_value": {"value": 10, "type": "int"}
      }
    },
    {
      "id": "node_2",
      "type": "int_value",
      "properties": {
        "_value": {"value": 20, "type": "int"}
      }
    },
    {
      "id": "node_3",
      "type": "add",
      "properties": {}
    }
  ],
  "connections": [
    {"from": "node_1", "fromPort": "value", "to": "node_3", "toPort": "src"},
    {"from": "node_2", "fromPort": "value", "to": "node_3", "toPort": "operand"}
  ]
}
```

### Pipeline: CSV -> Field -> Add

```json
{
  "nodes": [
    {
      "id": "csv_1",
      "type": "csv_source",
      "properties": {}
    },
    {
      "id": "field_1",
      "type": "field",
      "properties": {
        "_column": {"value": "price", "type": "string"}
      }
    },
    {
      "id": "int_1",
      "type": "int_value",
      "properties": {
        "_value": {"value": 10, "type": "int"}
      }
    },
    {
      "id": "add_1",
      "type": "add",
      "properties": {}
    }
  ],
  "connections": [
    {"from": "csv_1", "fromPort": "csv", "to": "field_1", "toPort": "csv"},
    {"from": "csv_1", "fromPort": "csv", "to": "add_1", "toPort": "csv"},
    {"from": "field_1", "fromPort": "field", "to": "add_1", "toPort": "src"},
    {"from": "int_1", "fromPort": "value", "to": "add_1", "toPort": "operand"}
  ]
}
```

## C++ API

### Serialization

```cpp
#include "nodes/NodeGraphSerializer.hpp"

NodeGraph graph;
// ... build graph ...

// To JSON object
json j = NodeGraphSerializer::toJson(graph);

// To formatted string (indent = 2 spaces)
std::string str = NodeGraphSerializer::toString(graph);

// To compact string (no whitespace)
std::string compact = NodeGraphSerializer::toString(graph, -1);
```

### Deserialization

```cpp
#include "nodes/NodeGraphSerializer.hpp"

// From JSON object
json j = json::parse(jsonString);
NodeGraph graph = NodeGraphSerializer::fromJson(j);

// From string directly
NodeGraph graph = NodeGraphSerializer::fromString(jsonString);
```

### Error Handling

Deserialization throws `std::runtime_error` for:
- Invalid JSON syntax
- Missing required fields (`id`, `type` in nodes)
- Missing connection fields (`from`, `fromPort`, `to`, `toPort`)
- Invalid property type strings

```cpp
try {
    auto graph = NodeGraphSerializer::fromString(jsonStr);
} catch (const std::runtime_error& e) {
    std::cerr << "Parse error: " << e.what() << std::endl;
}
```

## Node ID Handling

- Node IDs can be any string (e.g., "node_1", "my-custom-id", "uuid-1234")
- When deserializing, the `NodeGraph::addNodeWithId()` method preserves original IDs
- The internal `nextId` counter is automatically updated to avoid collisions
- IDs matching pattern `node_X` (where X is a number) update the counter to `max(X) + 1`

```cpp
// After deserializing nodes with IDs "node_5" and "node_10"
auto newId = graph.addNode("test");  // Returns "node_11"
```

## Frontend Integration

For a graph editor frontend:

1. **Load graph**: Fetch JSON from backend, render nodes and connections
2. **Edit graph**: Modify JSON locally (add/remove nodes, change properties, connect ports)
3. **Save graph**: Send complete JSON to backend for persistence
4. **Execute graph**: Send JSON to backend, backend deserializes and executes
5. **Request CSV data**: Separate API call to fetch DataFrame content for display
