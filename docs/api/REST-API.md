# REST API Reference

## Base URL

```
http://localhost:8080
```

## Endpoints

### Health Check

Check server status and dataset availability.

```
GET /api/health
```

**Response:**
```json
{
  "status": "ok",
  "service": "AnodeServer",
  "version": "1.0.0",
  "dataset_loaded": true
}
```

---

### Dataset Info

Get information about the loaded dataset.

```
GET /api/dataset/info
```

**Response:**
```json
{
  "status": "ok",
  "path": "/path/to/data.csv",
  "rows": 500000,
  "columns": [
    { "name": "id", "type": "int" },
    { "name": "name", "type": "string" },
    { "name": "price", "type": "double" }
  ]
}
```

**Errors:**
- `503 Service Unavailable` - No dataset loaded

---

### Query Dataset

Execute a query pipeline on the dataset.

```
POST /api/dataset/query
Content-Type: application/json
```

**Request Body:**
```json
{
  "operations": [
    {
      "type": "filter",
      "params": [
        { "column": "age", "operator": ">=", "value": 18 }
      ]
    },
    {
      "type": "orderby",
      "params": [
        { "column": "name", "order": "asc" }
      ]
    }
  ],
  "limit": 100,
  "offset": 0
}
```

**Response (columnar format):**
```json
{
  "status": "ok",
  "stats": {
    "input_rows": 500000,
    "output_rows": 125000,
    "offset": 0,
    "returned_rows": 100,
    "duration_ms": 45
  },
  "columns": ["id", "name", "age"],
  "data": [
    [1, "Alice", 25],
    [2, "Bob", 30]
  ]
}
```

> **Note:** The response uses a columnar format where column names are sent once in `columns` and each row in `data` is an array of values in the same order. This reduces payload size by 10-20x compared to row-based JSON.

---

## Node Definitions API

Get all registered node definitions for building a graph editor UI.

### List Node Definitions

```
GET /api/nodes
```

**Response:**
```json
{
  "status": "ok",
  "nodes": [
    {
      "name": "add",
      "category": "math",
      "isEntryPoint": false,
      "inputs": [
        {"name": "csv", "types": ["csv"], "required": false},
        {"name": "src", "types": ["int", "double", "field"], "required": true},
        {"name": "dest", "types": ["field"], "required": false},
        {"name": "operand", "types": ["int", "double", "field"], "required": true}
      ],
      "outputs": [
        {"name": "csv", "types": ["csv"]},
        {"name": "result", "types": ["double"]}
      ]
    },
    {
      "name": "int_value",
      "category": "scalar",
      "isEntryPoint": true,
      "inputs": [],
      "outputs": [
        {"name": "value", "types": ["int"]}
      ]
    }
  ],
  "categories": ["scalar", "data", "math", "csv"]
}
```

**Fields:**

| Field | Description |
|-------|-------------|
| `name` | Node type identifier (used in graph JSON) |
| `category` | Category for UI grouping |
| `isEntryPoint` | If true, node has no required inputs (can start a graph) |
| `inputs[].name` | Input port name |
| `inputs[].types` | Accepted types: `int`, `double`, `string`, `bool`, `field`, `csv` |
| `inputs[].required` | If true, must be connected for execution |
| `outputs[].name` | Output port name |
| `outputs[].types` | Output type(s) |

---

## Graph API

The Graph API allows you to create, store, retrieve, and execute NodeGraphs. Graphs are persisted in a SQLite database with version history support.

### List Graphs

Get all saved graphs.

```
GET /api/graphs
```

**Response:**
```json
{
  "status": "ok",
  "graphs": [
    {
      "slug": "my-pipeline",
      "name": "My Pipeline",
      "description": "Processes sales data",
      "author": "alice",
      "tags": ["etl", "sales"],
      "created_at": "2025-01-15T10:30:00.123Z",
      "updated_at": "2025-01-15T14:45:00.456Z",
      "links": {
        "outgoing": ["detail"],
        "incoming": ["dashboard"]
      }
    }
  ]
}
```

The `links` field contains auto-detected event navigation links:
- `outgoing`: graphs this graph targets via `timeline_output` events
- `incoming`: graphs that target this graph via events

---

### Create Graph

Create a new graph with optional initial content.

```
POST /api/graph
Content-Type: application/json
```

**Request Body:**
```json
{
  "slug": "my-pipeline",
  "name": "My Pipeline",
  "description": "Optional description",
  "author": "alice",
  "tags": ["etl", "sales"],
  "graph": {
    "nodes": [
      {"id": "n1", "type": "int_value", "properties": {"_value": {"value": 10, "type": "int"}}},
      {"id": "n2", "type": "int_value", "properties": {"_value": {"value": 20, "type": "int"}}},
      {"id": "n3", "type": "add", "properties": {}}
    ],
    "connections": [
      {"from": "n1", "fromPort": "value", "to": "n3", "toPort": "src"},
      {"from": "n2", "fromPort": "value", "to": "n3", "toPort": "operand"}
    ]
  }
}
```

**Response (201 Created):**
```json
{
  "status": "ok",
  "slug": "my-pipeline",
  "version_id": 1
}
```

**Errors:**
- `400 Bad Request` - Missing required fields (slug, name) or graph already exists

---

### Get Graph

Retrieve a graph with its latest version.

```
GET /api/graph/:slug
```

**Response:**
```json
{
  "status": "ok",
  "metadata": {
    "slug": "my-pipeline",
    "name": "My Pipeline",
    "description": "Processes sales data",
    "author": "alice",
    "tags": ["etl", "sales"],
    "created_at": "2025-01-15T10:30:00.123Z",
    "updated_at": "2025-01-15T14:45:00.456Z"
  },
  "version": {
    "id": 2,
    "version_name": "v1.1 - bug fix",
    "created_at": "2025-01-15T14:45:00.456Z"
  },
  "graph": {
    "nodes": [...],
    "connections": [...]
  },
  "links": {
    "outgoing": ["detail"],
    "incoming": ["dashboard"]
  }
}
```

**Errors:**
- `404 Not Found` - Graph not found

---

### Update Graph (Save New Version)

Save a new version of an existing graph.

```
PUT /api/graph/:slug
Content-Type: application/json
```

**Request Body:**
```json
{
  "version_name": "v1.1 - bug fix",
  "graph": {
    "nodes": [...],
    "connections": [...]
  }
}
```

**Response:**
```json
{
  "status": "ok",
  "version_id": 2,
  "links": {
    "outgoing": ["detail"],
    "incoming": []
  }
}
```

The `links` field is returned after save so the editor can update link badges without a separate fetch.

**Errors:**
- `400 Bad Request` - Missing graph field or graph not found

---

### Delete Graph

Delete a graph and all its versions.

```
DELETE /api/graph/:slug
```

**Response:**
```json
{
  "status": "ok",
  "message": "Graph deleted: my-pipeline"
}
```

**Errors:**
- `404 Not Found` - Graph not found

---

### List Graph Versions

Get all versions of a graph.

```
GET /api/graph/:slug/versions
```

**Response:**
```json
{
  "status": "ok",
  "slug": "my-pipeline",
  "versions": [
    {"id": 2, "version_name": "v1.1 - bug fix", "created_at": "2025-01-15T14:45:00.456Z"},
    {"id": 1, "version_name": "Initial version", "created_at": "2025-01-15T10:30:00.123Z"}
  ]
}
```

---

### Execute Graph

Execute a graph and return results from all nodes.

```
POST /api/graph/:slug/execute
Content-Type: application/json
```

**Request Body (optional):**
```json
{
  "version_id": 1,
  "inputs": {
    "my_param": 42,
    "rate": 0.15,
    "prefix": "report_"
  }
}
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `version_id` | int | Optional. If omitted, the latest version is executed. |
| `inputs` | object | Optional. Key-value pairs to override scalar node values at runtime. |

**Input Overrides:**

The `inputs` object allows overriding `_value` properties of scalar nodes that have an `_identifier` property defined. Each key must match a node's `_identifier`, and the value must have a compatible type:

| Node Type | Expected Input Type |
|-----------|---------------------|
| `int_value` | integer |
| `double_value` | number (int or float, int is auto-converted) |
| `string_value` | string |
| `bool_value` | boolean |
| `date_value` | string (date format) |
| `string_as_field` | string (field name) |
| `string_as_fields` | JSON array of strings (e.g., `["col_a", "col_b"]`) |
| `null_value` | null |
| `current_date` | integer (for offsets) |
| `csv_value` | CSV object (`{columns, schema, data}`) |

**Response:**
```json
{
  "status": "ok",
  "session_id": "sess_abc123",
  "execution_id": 42,
  "results": {
    "n1": {"value": {"type": "int", "value": 10}},
    "n2": {"value": {"type": "int", "value": 20}},
    "n3": {"result": {"type": "double", "value": 30.0}}
  },
  "csv_metadata": { ... },
  "duration_ms": 1
}
```

**Errors:**
- `400 Bad Request` - Duplicate identifier, unknown identifier, or type mismatch
- `404 Not Found` - Graph not found
- `500 Internal Server Error` - Execution failed

**Error Examples:**
```json
{"status": "error", "error": "Duplicate identifier 'my_param' in nodes node_1 and node_5"}
{"status": "error", "error": "Input identifier 'unknown' not found in graph"}
{"status": "error", "error": "Type mismatch for identifier 'rate': expected double, got string"}
```

---

### Execute Graph with Streaming (SSE)

Execute a graph with real-time feedback via Server-Sent Events. Each node emits events as it starts and completes, enabling live progress visualization.

```
POST /api/graph/:slug/execute-stream
Content-Type: application/json
```

**Response:** `Content-Type: text/event-stream`

**Events emitted:**

1. **execution_start** - Sent when execution begins
```
event: execution_start
data: {"session_id": "sess_abc123", "node_count": 5}
```

2. **node_started** - Sent when a node begins execution
```
event: node_started
data: {"node_id": "node_1", "status": "started", "session_id": "sess_abc123"}
```

3. **node_completed** - Sent when a node finishes successfully
```
event: node_completed
data: {"node_id": "node_1", "status": "completed", "duration_ms": 42, "session_id": "sess_abc123", "csv_metadata": {"csv": {"rows": 1000, "columns": ["id", "name"]}}}
```

4. **node_failed** - Sent when a node encounters an error
```
event: node_failed
data: {"node_id": "node_2", "status": "failed", "duration_ms": 5, "error_message": "Column not found", "session_id": "sess_abc123"}
```

5. **execution_complete** - Sent when all nodes have been processed
```
event: execution_complete
data: {"session_id": "sess_abc123", "has_errors": false}
```

6. **error** - Sent if a fatal error occurs
```
event: error
data: {"message": "Graph not found"}
```

**JavaScript Example:**
```javascript
const response = await fetch('http://localhost:8080/api/graph/my-pipeline/execute-stream', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({})
});

const reader = response.body.getReader();
const decoder = new TextDecoder();

while (true) {
  const { done, value } = await reader.read();
  if (done) break;

  const text = decoder.decode(value);
  // Parse SSE events from text
  // event: node_completed
  // data: {"node_id": "node_1", ...}
}
```

---

## Dynamic Nodes API

Inject mathematical operations into a graph between `dynamic_begin` and `dynamic_end` markers.
See [DYNAMIC-NODES.md](../nodes/DYNAMIC-NODES.md) for full documentation.

### Apply Dynamic Equations

Inject nodes into the graph and **save as a new version**.

```
POST /api/graph/:slug/apply-dynamic
Content-Type: application/json
```

**Request Body:**
```json
{
  "dynamic_nodes": [
    {
      "_name": "calculations",
      "params": [
        "Total = ($Price + $Tax) * $Quantity",
        "Margin = $Total * 0.15"
      ]
    }
  ],
  "version_name": "Added pricing calculations"
}
```

**Response:**
```json
{
  "status": "ok",
  "version_id": 15,
  "nodes_added": 8,
  "message": "Graph updated with 8 new nodes"
}
```

**Notes:**
- Field names can contain spaces: `$Unit Price`, `$Integration rate`
- Destination names can contain spaces: `Total Price = ...`
- Creates `scalar/string_as_field` nodes for field references
- Creates `scalar/double_value` nodes for numeric constants
- Creates `math/*` nodes for operations

---

### Execute Dynamic Equations

Inject nodes temporarily and **execute without saving**.

```
POST /api/graph/:slug/execute-dynamic
Content-Type: application/json
```

**Request Body:**
```json
{
  "dynamic_nodes": [
    {
      "_name": "calculations",
      "params": ["Total = $Price * $Quantity"]
    }
  ],
  "inputs": {
    "tax_rate": 0.20,
    "threshold": 100
  }
}
```

The `inputs` parameter works the same as in `/execute` - it allows overriding scalar node values at runtime.

**Response:**
```json
{
  "status": "ok",
  "session_id": "sess_abc123",
  "execution_id": 42,
  "results": { ... },
  "csv_metadata": { ... },
  "duration_ms": 150
}
```

**Use case:** Test equations before permanently applying them, with parameterized values.

---

## Execution Persistence API

Execution results (DataFrames) are automatically persisted to SQLite. This allows:
- Viewing results after server restart
- Sharing results between browser tabs
- Restoring past executions

### List Executions

Get all past executions for a graph.

```
GET /api/graph/:slug/executions
```

**Response:**
```json
{
  "status": "ok",
  "slug": "my-pipeline",
  "executions": [
    {
      "id": 5,
      "session_id": "sess_abc123...",
      "version_id": 2,
      "created_at": "2025-01-15T14:45:00.456Z",
      "duration_ms": 45,
      "node_count": 5,
      "dataframe_count": 2
    }
  ]
}
```

---

### Get Execution Details

Get details and CSV metadata for a specific execution.

```
GET /api/execution/:id
```

**Response:**
```json
{
  "status": "ok",
  "execution": {
    "id": 5,
    "graph_slug": "my-pipeline",
    "session_id": "sess_abc123...",
    "version_id": 2,
    "created_at": "2025-01-15T14:45:00.456Z",
    "duration_ms": 45,
    "node_count": 5,
    "dataframe_count": 2
  },
  "csv_metadata": {
    "node_1": {
      "csv": {
        "rows": 1000,
        "columns": ["id", "name", "price"]
      }
    }
  }
}
```

---

### Restore Execution

Restore a past execution to make its DataFrames accessible again.

```
POST /api/execution/:id/restore
```

**Response:**
```json
{
  "status": "ok",
  "session_id": "sess_abc123...",
  "execution_id": 5,
  "csv_metadata": {
    "node_1": {
      "csv": {
        "rows": 1000,
        "columns": ["id", "name", "price"]
      }
    }
  }
}
```

After restoring, use the session DataFrame API to query the data.

---

### Session DataFrame Query

Query a DataFrame from an execution session with pagination and operations.

```
POST /api/session/:sessionId/dataframe/:nodeId/:portName
Content-Type: application/json
```

**Request Body:**
```json
{
  "limit": 100,
  "offset": 0,
  "operations": [
    {"type": "filter", "params": [{"column": "price", "operator": ">", "value": 10}]},
    {"type": "orderby", "params": [{"column": "name", "order": "asc"}]}
  ]
}
```

**Response:**
```json
{
  "status": "ok",
  "stats": {
    "total_rows": 1000,
    "offset": 0,
    "returned_rows": 100,
    "duration_ms": 5
  },
  "columns": ["id", "name", "price"],
  "data": [
    [1, "Apple", 1.50],
    [2, "Banana", 0.75]
  ]
}
```

> **Note:** If the session is no longer in memory (server restarted), it automatically loads from SQLite.

---

### Automatic Cleanup

Old executions are automatically cleaned up to keep only the **10 most recent** per graph. This happens after each new execution.

---

## Named Outputs API

The `output` node allows you to "publish" a DataFrame with a name for external access. This is useful for building viewers or dashboards that consume graph results.

### List Named Outputs

Get all named outputs from the latest execution of a graph.

```
GET /api/graph/:slug/outputs
```

**Response:**
```json
{
  "status": "ok",
  "outputs": [
    {
      "name": "my_data",
      "node_id": "node_4",
      "rows": 4214,
      "columns": ["id", "name", "price"],
      "execution_id": 9,
      "created_at": "2026-02-03T10:30:00.123Z"
    }
  ]
}
```

---

### Get Named Output Data

Retrieve the DataFrame for a specific named output with pagination support.

```
POST /api/graph/:slug/output/:name
Content-Type: application/json
```

**Request Body:**
```json
{
  "limit": 100,
  "offset": 0,
  "operations": [
    {"type": "filter", "params": [{"column": "price", "operator": ">", "value": 10}]}
  ]
}
```

**Response:**
```json
{
  "status": "ok",
  "output": {
    "name": "my_data",
    "node_id": "node_4",
    "execution_id": 9,
    "created_at": "2026-02-03T10:30:00.123Z"
  },
  "stats": {
    "total_rows": 4214,
    "offset": 0,
    "returned_rows": 100,
    "duration_ms": 5
  },
  "columns": ["id", "name", "price"],
  "data": [
    [1, "Apple", 1.50],
    [2, "Banana", 0.75]
  ]
}
```

**Errors:**
- `404 Not Found` - Graph or output not found

---

### Named Outputs (cURL)

```bash
# List all named outputs
curl http://localhost:8080/api/graph/my-pipeline/outputs

# Get output data with pagination
curl -X POST http://localhost:8080/api/graph/my-pipeline/output/my_data \
  -H "Content-Type: application/json" \
  -d '{"limit": 100, "offset": 0}'

# Get output data with filter
curl -X POST http://localhost:8080/api/graph/my-pipeline/output/my_data \
  -H "Content-Type: application/json" \
  -d '{
    "limit": 50,
    "operations": [
      {"type": "filter", "params": [{"column": "price", "operator": ">", "value": 10}]}
    ]
  }'
```

---

## Operations

### Filter

Filter rows based on conditions. Multiple conditions are combined with AND logic.

```json
{
  "type": "filter",
  "params": [
    { "column": "country", "operator": "==", "value": "France" },
    { "column": "age", "operator": ">=", "value": 18 }
  ]
}
```

**Operators:**

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `"value": "Paris"` |
| `!=` | Not equal | `"value": "London"` |
| `<` | Less than | `"value": 100` |
| `<=` | Less or equal | `"value": 100` |
| `>` | Greater than | `"value": 0` |
| `>=` | Greater or equal | `"value": 18` |
| `contains` | Substring match | `"value": "gmail"` |

---

### OrderBy

Sort results by one or more columns.

```json
{
  "type": "orderby",
  "params": [
    { "column": "created_at", "order": "desc" },
    { "column": "name", "order": "asc" }
  ]
}
```

**Order values:**
- `asc` or `ascending`
- `desc` or `descending`

---

### GroupBy

Group rows and compute aggregations.

```json
{
  "type": "groupby",
  "params": {
    "groupBy": ["country", "city"],
    "aggregations": [
      { "column": "salary", "function": "avg", "alias": "avg_salary" },
      { "column": "id", "function": "count", "alias": "total" }
    ]
  }
}
```

**Aggregation functions:**

| Function | Description |
|----------|-------------|
| `count` | Number of rows |
| `sum` | Sum of values |
| `avg` | Average value |
| `min` | Minimum value |
| `max` | Maximum value |
| `first` | First value (useful for strings) |
| `blank` | Returns null |

---

### GroupByTree

Hierarchical groupBy that preserves child rows for tree visualization (e.g., Tabulator).

```json
{
  "type": "groupbytree",
  "params": {
    "groupBy": ["category"],
    "aggregations": {
      "price": "sum",
      "name": "first"
    }
  }
}
```

**Response format:**
```json
{
  "columns": ["category", "price", "name"],
  "data": [
    ["Electronics", 1500.00, null, [[...], [...]]],
    ["Books", 250.00, null, [[...]]]
  ]
}
```

The last element of each row is an array of child rows (original data) in the same columnar format.

**Aggregations format:** Object mapping column name to aggregation function. Columns not specified default to `blank`.

---

### Pivot

Transpose values from a column into multiple columns.

```json
{
  "type": "pivot",
  "params": {
    "pivotColumn": "question_id",
    "valueColumn": "value",
    "indexColumns": ["task_id", "user_id"],
    "prefix": "q_"
  }
}
```

**Parameters:**
- `pivotColumn` (required): Column whose values become new column names
- `valueColumn` (required): Column whose values fill the new columns
- `indexColumns` (optional): Columns that identify each row. If omitted, all other columns are used.
- `prefix` (optional): Prefix for new column names (default: `{valueColumn}_`)

**Example:**

Input:
```
question_id, task_id, value
1, 100, 10
2, 100, 20
1, 101, 15
2, 101, 25
```

Output with `pivotColumn=question_id, valueColumn=value, indexColumns=[task_id]`:
```json
{
  "columns": ["task_id", "value_1", "value_2"],
  "data": [
    [100, 10, 20],
    [101, 15, 25]
  ]
}
```

> **Note:** Pivot can be chained with other operations (filter, orderby, groupbytree).

---

### Select

Select specific columns (projection).

```json
{
  "type": "select",
  "params": ["id", "name", "email"]
}
```

---

## Pagination

Use `limit` and `offset` for server-side pagination:

```json
{
  "operations": [],
  "limit": 50,
  "offset": 100
}
```

- `limit`: Maximum rows to return (default: 100)
- `offset`: Number of rows to skip (default: 0)

The response `stats.output_rows` indicates total matching rows before pagination.

---

## Error Responses

### 400 Bad Request

Invalid JSON or malformed request.

```json
{
  "status": "error",
  "message": "Invalid JSON: unexpected character at position 42"
}
```

### 404 Not Found

Unknown endpoint.

```json
{
  "status": "error",
  "message": "Not found: /api/unknown"
}
```

### 500 Internal Server Error

Operation failed.

```json
{
  "status": "error",
  "message": "Column 'unknown_column' not found"
}
```

### 503 Service Unavailable

No dataset loaded.

```json
{
  "status": "error",
  "message": "No dataset loaded"
}
```

---

## CORS

All endpoints support CORS with:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

Preflight requests (`OPTIONS`) return `200 OK` with CORS headers.

---

## Examples

### cURL

```bash
# Health check
curl http://localhost:8080/api/health

# Dataset info
curl http://localhost:8080/api/dataset/info

# Query with filter
curl -X POST http://localhost:8080/api/dataset/query \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [
      {
        "type": "filter",
        "params": [{"column": "country", "operator": "==", "value": "France"}]
      }
    ],
    "limit": 10
  }'

# Pagination
curl -X POST http://localhost:8080/api/dataset/query \
  -H "Content-Type: application/json" \
  -d '{"operations": [], "limit": 50, "offset": 100}'

# GroupBy aggregation
curl -X POST http://localhost:8080/api/dataset/query \
  -H "Content-Type: application/json" \
  -d '{
    "operations": [{
      "type": "groupby",
      "params": {
        "groupBy": ["country"],
        "aggregations": [
          {"column": "id", "function": "count", "alias": "count"}
        ]
      }
    }]
  }'
```

### Node Definitions (cURL)

```bash
# Get all node definitions
curl http://localhost:8080/api/nodes
```

### Graph API (cURL)

```bash
# List all graphs
curl http://localhost:8080/api/graphs

# Create a new graph
curl -X POST http://localhost:8080/api/graph \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "my-pipeline",
    "name": "My Pipeline",
    "author": "alice",
    "graph": {
      "nodes": [
        {"id": "n1", "type": "int_value", "properties": {"_value": {"value": 10, "type": "int"}}},
        {"id": "n2", "type": "int_value", "properties": {"_value": {"value": 20, "type": "int"}}},
        {"id": "n3", "type": "add", "properties": {}}
      ],
      "connections": [
        {"from": "n1", "fromPort": "value", "to": "n3", "toPort": "src"},
        {"from": "n2", "fromPort": "value", "to": "n3", "toPort": "operand"}
      ]
    }
  }'

# Get a graph
curl http://localhost:8080/api/graph/my-pipeline

# Save a new version
curl -X PUT http://localhost:8080/api/graph/my-pipeline \
  -H "Content-Type: application/json" \
  -d '{
    "version_name": "v2.0",
    "graph": {"nodes": [...], "connections": [...]}
  }'

# List versions
curl http://localhost:8080/api/graph/my-pipeline/versions

# Execute a graph
curl -X POST http://localhost:8080/api/graph/my-pipeline/execute

# Execute a graph with input overrides
curl -X POST http://localhost:8080/api/graph/my-pipeline/execute \
  -H "Content-Type: application/json" \
  -d '{
    "inputs": {
      "threshold": 42,
      "rate": 0.15,
      "prefix": "report_"
    }
  }'

# Delete a graph
curl -X DELETE http://localhost:8080/api/graph/my-pipeline
```

### JavaScript (fetch)

```javascript
const response = await fetch('http://localhost:8080/api/dataset/query', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    operations: [
      { type: 'filter', params: [{ column: 'age', operator: '>=', value: 21 }] },
      { type: 'orderby', params: [{ column: 'name', order: 'asc' }] }
    ],
    limit: 100,
    offset: 0
  })
});

const result = await response.json();
console.log(`Found ${result.stats.output_rows} rows in ${result.stats.duration_ms}ms`);
```

### Graph API (JavaScript)

```javascript
// Create a graph
await fetch('http://localhost:8080/api/graph', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    slug: 'my-pipeline',
    name: 'My Pipeline',
    graph: { nodes: [...], connections: [...] }
  })
});

// Execute and get results
const execResponse = await fetch('http://localhost:8080/api/graph/my-pipeline/execute', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' }
});
const { results, duration_ms } = await execResponse.json();
console.log(`Executed in ${duration_ms}ms`, results);

// Execute with input overrides (parameterized execution)
const paramResponse = await fetch('http://localhost:8080/api/graph/my-pipeline/execute', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    inputs: {
      threshold: 42,
      rate: 0.15,
      prefix: 'report_'
    }
  })
});
const paramResults = await paramResponse.json();
console.log('Parameterized execution:', paramResults);
```

### Execution Persistence (cURL)

```bash
# List past executions
curl http://localhost:8080/api/graph/my-pipeline/executions

# Get execution details
curl http://localhost:8080/api/execution/5

# Restore a past execution
curl -X POST http://localhost:8080/api/execution/5/restore

# Query a DataFrame from an execution
curl -X POST http://localhost:8080/api/session/sess_abc123/dataframe/node_1/csv \
  -H "Content-Type: application/json" \
  -d '{"limit": 100, "offset": 0}'
```

### Execution Persistence (JavaScript)

```javascript
// List past executions
const executions = await fetch('http://localhost:8080/api/graph/my-pipeline/executions');
const { executions: list } = await executions.json();

// Restore an execution
const restore = await fetch(`http://localhost:8080/api/execution/${list[0].id}/restore`, {
  method: 'POST'
});
const { session_id, csv_metadata } = await restore.json();

// Query the DataFrame
const df = await fetch(`http://localhost:8080/api/session/${session_id}/dataframe/node_1/csv`, {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ limit: 100 })
});
const { columns, data } = await df.json();
```
