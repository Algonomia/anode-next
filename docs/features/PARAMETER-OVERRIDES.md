# Parameter Overrides

## Context

Parameter overrides allow the **viewer** to modify the input values of a graph without altering the graph itself. This ensures that the editor always uses the base values of the graph.

## Architecture

### Storage

Overrides are stored in a separate SQLite table `parameter_overrides`:

```sql
CREATE TABLE parameter_overrides (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    graph_slug TEXT NOT NULL,
    identifier TEXT NOT NULL,
    value_json TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    FOREIGN KEY (graph_slug) REFERENCES graphs(slug) ON DELETE CASCADE,
    UNIQUE(graph_slug, identifier)
);
```

- `identifier` corresponds to a node's `_identifier` in the graph (scalar or csv_source)
- `value_json` contains the serialized workload: `{"type":"int","value":42}` or `{"type":"csv","value":{"columns":[...],"schema":[...],"data":[...]}}`
- Deleting a graph cascades to its overrides

### API

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/graph/{slug}/parameters` | List all overrides |
| `PUT` | `/api/graph/{slug}/parameters/{identifier}` | Create/update an override |
| `DELETE` | `/api/graph/{slug}/parameters/{identifier}` | Delete an override |

**GET** returns:
```json
{
  "status": "ok",
  "parameters": {
    "threshold": {"type": "int", "value": 42},
    "my_csv": {"type": "csv", "value": {"columns": [...], "schema": [...], "data": [...]}}
  }
}
```

**PUT** body = the value_json directly:
```json
{"type": "int", "value": 42}
```

### Application at execution time

| Caller | Overrides applied? | Mechanism |
|--------|--------------------|-----------|
| Editor (SSE `/execute-stream`) | No | Uses the graph as-is |
| Viewer (`POST /execute` + `apply_overrides: true`) | Yes | Scalars via `_value` + CSV via CsvOverrides |
| Triggers (LISTEN/NOTIFY) | No | Its own CsvOverrides mechanism |
| Scenarios (`/scenarios/:id/run`) | No | Its own inputs/triggers |

When `apply_overrides: true` is present in the request:

1. The server loads overrides from `parameter_overrides`
2. For each override:
   - **Scalar**: `graph.setProperty(nodeId, "_value", workload)` — modifies the in-memory copy of the graph
   - **CSV**: adds to the `CsvOverrides` map (identifier -> DataFrame)
3. The `CsvOverrides` are passed to `NodeExecutor::execute()` which injects the DataFrame directly into the corresponding csv_source node, skipping its compilation

### csv_source

The `csv_source` node no longer reads `_csv_data` from its properties. Its logic is:

1. **Connected input**: if another node sends a CSV on the `csv` port, it is used (passthrough)
2. **Test data**: fallback with 4 rows (Apple, Banana, Orange, Grape)

External CSV injection (viewer overrides, triggers, scenarios) goes exclusively through the `CsvOverrides` mechanism in `NodeExecutor`, which short-circuits the node's compilation.

## Related files

| File | Role |
|------|------|
| `src/storage/GraphStorage.hpp` | CRUD method declarations |
| `src/storage/GraphStorage.cpp` | SQLite table + CRUD implementation |
| `src/server/RequestHandler.hpp` | API handler declarations |
| `src/server/RequestHandler.cpp` | Handlers + `apply_overrides` logic in `handleExecuteGraph` |
| `src/server/HttpSession.cpp` | Routing of `/parameters` endpoints |
| `src/nodes/nodes/CsvNodes.cpp` | csv_source without `_csv_data` |
| `src/nodes/NodeExecutor.cpp` | CsvOverrides injection |
| `src/client/examples/viewer.html` | Frontend: reading/writing overrides |

## Tests

- `tests/GraphStorageTest.cpp` — `[ParameterOverrides]`: CRUD, cascade delete
- `tests/TestNodesTest.cpp` — `[csv_source]`: `_csv_data` ignored, CsvOverrides injection, fallback
