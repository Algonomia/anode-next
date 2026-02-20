# PostgreSQL Integration

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         POSTGRESQL NODE SYSTEM                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐       │
│  │ postgres_config │     │  postgres_query │     │  postgres_func  │       │
│  │ (connection)    │     │  (raw SQL)      │     │ (function call) │       │
│  └────────┬────────┘     └────────┬────────┘     └────────┬────────┘       │
│           │                       │                       │                 │
│           └───────────────────────┼───────────────────────┘                 │
│                                   ▼                                         │
│                          ┌─────────────────┐                                │
│                          │  PostgresPool   │                                │
│                          │   (singleton)   │                                │
│                          └────────┬────────┘                                │
│                                   │                                         │
│                                   ▼                                         │
│                          ┌─────────────────┐                                │
│                          │   PostgreSQL    │                                │
│                          │   (libpqxx)     │                                │
│                          └─────────────────┘                                │
└─────────────────────────────────────────────────────────────────────────────┘
```

## File Structure

```
src/
├── postgres/
│   ├── PostgresPool.hpp      # Connection pool singleton
│   └── PostgresPool.cpp
└── nodes/
    ├── DynRequest.hpp        # Dynamic SQL query builder
    ├── DynRequest.cpp
    └── nodes/
        ├── PostgresNodes.hpp/cpp  # Database nodes (postgres_config, postgres_query, postgres_func)
        ├── ProcessNodes.hpp/cpp   # Process nodes (identify_process, identify_phase, etc.)
        ├── PerimeterNodes.hpp/cpp # Perimeter nodes
        ├── FormNodes.hpp/cpp      # Form nodes (identify_form, identify_section, identify_question)
        ├── DataNodes.hpp/cpp      # Data nodes (export_data_from_tasks, datavalue_extractor)
        └── HistoryNodes.hpp/cpp   # History nodes (get_history_value, get_history_snapshot)
```

## Dependencies

- **libpqxx**: C++ client library for PostgreSQL
- **libpq**: Raw C library (used by PostgresListener)
  ```bash
  sudo apt-get install libpqxx-dev libpq-dev
  ```

## Server Configuration

PostgreSQL connection is configured at server startup via `--postgres` argument:

### Direct connection string
```bash
./anodeServer --postgres "host=localhost port=5432 dbname=anode user=postgres password=secret"
```

### Configuration file
```bash
./anodeServer --postgres @/path/to/postgres.conf
```

**postgres.conf** format:
```
# PostgreSQL connection configuration
host=localhost
port=5432
dbname=anode
user=postgres
password=secret
```

---

## PostgresPool

Singleton class managing PostgreSQL connections.

### API

```cpp
#include "postgres/PostgresPool.hpp"

// Configure connection
postgres::PostgresPool::instance().configure(
    "host=localhost port=5432 dbname=mydb user=postgres password=secret"
);

// Execute query and get DataFrame
auto df = postgres::PostgresPool::instance().executeQuery("SELECT * FROM users");

// Execute command (INSERT, UPDATE, DELETE)
size_t affected = postgres::PostgresPool::instance().executeCommand("DELETE FROM logs");

// Check status
bool configured = postgres::PostgresPool::instance().isConfigured();
bool connected = postgres::PostgresPool::instance().isConnected();

// Disconnect
postgres::PostgresPool::instance().disconnect();
```

### Type Mapping

| PostgreSQL OID | PostgreSQL Type | DataFrame Column |
|----------------|-----------------|------------------|
| 20, 21, 23, 26 | int2, int4, int8, oid | IntColumn |
| 700, 701, 1700 | float4, float8, numeric | DoubleColumn |
| 25, 1042, 1043 | text, char, varchar | StringColumn |
| (others) | - | StringColumn |

**NULL handling** : NULL -> 0 for ints, NULL -> "" for strings.

---

## DynRequest - Dynamic SQL Builder

Builder for dynamic PostgreSQL function calls with typed parameters. Generates `SELECT * FROM func(params)`.

### Basic Usage

```cpp
#include "nodes/DynRequest.hpp"

nodes::DynRequest req;
req.func("anode_identify_phase")
   .addIntArrayParam({10, 20, 30})
   .addStringArrayParam({"Planning", "Execution", "Review"});

std::string sql = req.buildSQL();
// -> "SELECT * FROM anode_identify_phase(ARRAY[10,20,30]::INT[], ARRAY['Planning','Execution','Review']::TEXT[])"
```

### Parameter Prefix System

Each parameter has a type prefix that determines the SQL cast:

| Prefix | Type | PostgreSQL Cast | Example |
|--------|------|-----------------|---------|
| `i` | Integer scalar | `42` | `addIntParam(42)` |
| `d` | Double scalar | `3.14` | `addDoubleParam(3.14)` |
| `s` | String scalar | `'hello'` | `addStringParam("hello")` |
| `b` | Boolean | `true` / `false` | `addBoolParam(true)` |
| `n` | NULL | `NULL` | `addNullParam()` |
| `I` | Integer array | `ARRAY[1,2,3]::INT[]` | `addIntArrayParam({1,2,3})` |
| `D` | Double array | `ARRAY[1.5,2.5]::DOUBLE PRECISION[]` | `addDoubleArrayParam({1.5,2.5})` |
| `S` | String array | `ARRAY['a','b']::TEXT[]` | `addStringArrayParam({"a","b"})` |
| `J` | 2D Integer array | `ARRAY[ARRAY[1,2],ARRAY[3,4]]::INT[][]` | `addIntArray2DParam(...)` |

**Naming rules**: lowercase = scalar, uppercase = array.

### Broadcasting from Workloads

When using workloads with a CSV, scalar values broadcast to all rows:

```cpp
// Scalar workload with 3-row CSV -> broadcasts to [42, 42, 42]
Workload scalarWL(int64_t(42));
req.addIntArrayFromWorkload(scalarWL, csv);

// Field workload -> extracts column values
Workload fieldWL("column_name", NodeType::Field);
req.addStringArrayFromWorkload(fieldWL, csv);
```

### Conversion Tables

#### `addIntArrayFromWorkload` (target: INT[])

| Source Type | Example Value | Conversion | Result (N=3 rows) |
|-------------|--------------|------------|-------------------|
| `int` | `42` | Broadcast | `[42, 42, 42]` |
| `timestamp` | `1704067200` | Broadcast | `[1704067200, ...]` |
| `double` | `3.14` | parseInt, broadcast | `[3, 3, 3]` |
| `string` | `"123"` | parseInt, broadcast | `[123, 123, 123]` |
| `field` | `"process_id"` | Extract CSV column | `[1, 2, 3]` |
| `null` | `null` | Broadcast null | `[null, null, null]` |

#### `addStringArrayFromWorkload` (target: TEXT[])

| Source Type | Example Value | Conversion | Result (N=3 rows) |
|-------------|--------------|------------|-------------------|
| `string` | `"hello"` | Broadcast | `["hello", "hello", ...]` |
| `int` | `42` | toString, broadcast | `["42", "42", "42"]` |
| `double` | `3.14` | toString, broadcast | `["3.14", "3.14", ...]` |
| `field` | `"phase_label"` | Extract CSV column | `["A", "B", "C"]` |
| `null` | `null` | Broadcast null | `[null, null, null]` |

#### `addIntFromWorkload` (target: INTEGER scalar)

| Source Type | Example Value | Conversion | Result |
|-------------|--------------|------------|--------|
| `int` | `42` | Direct | `42` |
| `double` | `3.14` | parseInt | `3` |
| `string` | `"123"` | parseInt | `123` |
| `field` | `"id"` | First row value | `value` |
| `null` | `null` | Direct | `null` |

### Timestamp Conversion

`addTimestampFromWorkload` supports multiple input formats:

- Unix timestamp (number): `1704067200`
- Format `dd/mm/yyyy`: `"01/01/2024"`
- Format `dd/mm/yy`: `"01/01/24"`
- Format text: `"1 janvier 2024"`, `"1 january 2024"`

For fields, the method checks that all CSV rows have the same value (scalar parameter constraint).

### Complete Example

```cpp
// CSV input (3 rows):
// process_id | phase_label
// 10         | Planning
// 10         | Execution
// 20         | Review

// Workloads
Workload processWL("process_id", NodeType::Field);  // field -> extracts [10, 10, 20]
Workload phaseWL("phase_label", NodeType::Field);    // field -> extracts ["Planning", "Execution", "Review"]

DynRequest req;
req.func("anode_identify_phase")
   .addIntArrayFromWorkload(processWL, csv)      // -> ARRAY[10, 10, 20]::INT[]
   .addStringArrayFromWorkload(phaseWL, csv);     // -> ARRAY['Planning', 'Execution', 'Review']::TEXT[]

// Generated SQL:
// SELECT * FROM anode_identify_phase(
//     ARRAY[10, 10, 20]::INT[],
//     ARRAY['Planning', 'Execution', 'Review']::TEXT[]
// )
```

---

## Database Nodes

### postgres_config

Configures the PostgreSQL connection at runtime. **Optional** if server started with `--postgres`.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_host` | string | `localhost` | Database host |
| `_port` | string | `5432` | Database port |
| `_database` | string | `postgres` | Database name |
| `_user` | string | `postgres` | Username |
| `_password` | string | (empty) | Password |

> **Note**: Prefer server-side configuration (`--postgres`) for security.

### postgres_query

Executes raw SQL and returns a DataFrame.

| Port | Direction | Type | Description |
|------|-----------|------|-------------|
| `query` | input | String | SQL query (or `_query` property) |
| `csv` | output | Csv | Query result |

### postgres_func

Calls a PostgreSQL function with typed parameters.

| Port | Direction | Type | Description |
|------|-----------|------|-------------|
| `csv` | input (optional) | Csv | CSV for field resolution |
| `function` | input | String | Function name (or `_function` property) |
| `_int_N` | property | int | Integer parameters |
| `_string_N` | property | string | String parameters |
| `_double_N` | property | double | Double parameters |
| `csv` | output | Csv | Function result |

---

> **Note:** The following Process, Form, Data, and History nodes are domain-specific plugins. They serve as examples of how plugins can extend AnodeServer with specialized PostgreSQL function wrappers for particular business domains.

## Process Nodes

| Node | SQL Function | Inputs | Outputs |
|------|-------------|--------|---------|
| `identify_process` | `anode_identify_process(TEXT[])` | csv?, process_labels | csv, process_id, process_label |
| `identify_phase` | `anode_identify_phase(INT[], TEXT[])` | csv?, process_ids, phase_labels | csv, process_id, phase_label, phase_id |
| `get_or_clone_phase` | `anode_get_or_clone_phase(INT, INT, INT)` | csv?, process_id, start, end | csv, process_id, phase_label, phase_id |
| `identify_metatask` | `anode_identify_metatask(INT[], INT[], TEXT[])` | csv?, process_ids, phase_ids, metatask_labels | csv, process_id, phase_id, metatask_label, metatask_id |
| `identify_task` | `anode_identify_task(INT[], INT[], INT[])` | csv?, process_ids, phase_ids, metatask_ids | csv, process_id, phase_id, metatask_id, task_id, ... |

---

## Form Nodes

| Node | SQL Function | Inputs | Outputs |
|------|-------------|--------|---------|
| `identify_form` | `anode_identify_form(TEXT[])` | csv?, form_labels | csv, form_id, form_label |
| `identify_section` | `anode_identify_section(INT[], TEXT[])` | csv?, form_ids, section_labels | csv, form_id, section_id, section_label |
| `identify_question` | `anode_identify_questions(INT[], INT[], TEXT[], INT)` | csv?, form_ids, section_ids, question_names | csv, form_id, section_id, question_number, question_name, question_id |

`identify_question` property: `_with_numerotation` (int, default 0) - set to 1 if question names include numbering.

---

## Data Nodes

| Node | SQL Function | Inputs | Outputs |
|------|-------------|--------|---------|
| `export_data_from_tasks` | `anode_export_datavalue_from_task(INT[], INT[], INT[])` | in_tasks?, in_questions?, in_datavalues?, task_ids, question_ids, metadatavalue_ids | csv, task_id, question_id, datavalue_id, datavalue_value, ... |
| `datavalue_extractor` | (local JSON extraction) | csv?, value, dest_id?, dest_label?, dest_value? | csv, id, label, value |

---

## History Nodes

| Node | SQL Function | Inputs | Outputs |
|------|-------------|--------|---------|
| `get_history_value` | `anode_get_history_value_for_task(INT[], INT[], INT[])` | in_tasks?, in_questions?, in_metadatavalues?, task_ids?, question_ids?, metadatavalue_ids? | csv, r_date, r_user_id, r_type, r_question_id, r_question_title, r_value, r_is_restorable |
| `get_history_snapshot` | `anode_get_history_snapshot_for_task(INT, INT[], INT)` | in_questions?, task_id, question_ids, date | csv, source_type, question_id, question_title, value, dv_pev_*, metadatavalue_id, mdv_pev_* |

### get_history_value

Returns the complete modification history for tasks/questions. At least one of the 3 _ids inputs must be provided. Each _ids input has its own input CSV (3-CSV pattern like `export_data_from_tasks`).

SQL facade: `anode_get_history_value_for_task` -> `_get_history_value_for_task_core` (logic without permission check, SRP).

### get_history_snapshot

Reconstructs the state of a questionnaire at a given date. Returns one row per value (the last modification before the date). Excludes deletes (zzz2_history_type = 3).

- `task_id`: scalar INT (via `addIntFromWorkload`)
- `question_ids`: array INT[] (via `addIntArrayFromWorkload`)
- `date`: unix timestamp INT (via `addTimestampFromWorkload`)

Algorithm: `DISTINCT ON (id)` on data_datavalue_history/file_file_history + `LEFT JOIN LATERAL` on M2M history + metadatavalue_history, all `UNION ALL` data + files.

SQL facade: `anode_get_history_snapshot_for_task` -> `_get_history_snapshot_for_task` (internal `to_timestamp(p_date)` conversion).

---

## Testing

### Unit Tests

```bash
cd build && ./postgres_tests
```

Tests cover:
- PostgresPool singleton and configuration
- DynRequest SQL generation for all parameter types
- Broadcasting from scalar and field workloads

### Integration Tests

Require a PostgreSQL database:

```bash
export POSTGRES_TEST_CONN="host=localhost port=5432 dbname=test user=postgres password=secret"
./postgres_tests
```

## Error Handling

All PostgreSQL nodes set errors via `ctx.setError()` for:
- Missing required inputs
- PostgreSQL connection not configured
- SQL execution errors
- Missing columns (for field lookups)

```cpp
NodeExecutor exec(NodeRegistry::instance());
auto results = exec.execute(graph);

if (exec.hasErrors()) {
    auto* result = exec.getResult(nodeId);
    std::cerr << result->errorMessage << std::endl;
}
```

---

## JavaScript Reference

The original JavaScript implementation used a client/server HTTP model:

```
Client JS: request_dyn_init() -> request_dyn_func() -> request_dyn_add_*() -> perform_request()
         |
         v HTTP POST /fetchDyn?func=name  (body: I0=[1,2,3] S1=["a","b"])
         |
Server JS: parse prefixes -> build SQL -> execute -> return {header, body}
```

The C++ `DynRequest` class replaces this flow by building SQL directly.

Source files (legacy Node.js -- replaced by C++ DynRequest):
- Client: `request.js`, `requestWorkload.js`
- Server: `base.js`, `bdd.js`

---

## Related Documentation

- [Node Catalog](../nodes/CATALOG.md) - All nodes including database/process/form
- [Perimeter Cache](PERIMETER-CACHE.md) - In-memory cache for perimeter queries
- [LISTEN/NOTIFY](LISTEN-NOTIFY.md) - Trigger-based graph automation
