# PostgreSQL LISTEN/NOTIFY

Mechanism for listening to changes on PostgreSQL tables via LISTEN/NOTIFY, with automatic graph triggering.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     PG LISTEN/NOTIFY SYSTEM                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────┐                                                │
│  │     PostgreSQL      │  INSERT/UPDATE on monitored tables             │
│  │  (trigger functions)│──── pg_notify('anode_xxx', id) ──┐            │
│  └─────────────────────┘                                  │            │
│                                                           ▼            │
│                                                ┌──────────────────┐    │
│                                                │ PostgresListener │    │
│                                                │  (dedicated      │    │
│                                                │   libpq conn)    │    │
│                                                └────────┬─────────┘    │
│                                                         │              │
│           ┌─────────────────────────────────────────────┼──────┐       │
│           │                                             │      │       │
│           ▼                                             ▼      │       │
│  ┌──────────────────┐                          ┌────────────┐  │       │
│  │  PostgresPool    │  SELECT to retrieve      │  SQLite    │  │       │
│  │  (query data)    │  trigger data            │  (configs) │  │       │
│  └──────────────────┘                          └──────┬─────┘  │       │
│                                                       │        │       │
│           ┌───────────────────────────────────────────┘        │       │
│           ▼                                                    │       │
│  ┌────────────────────┐                                        │       │
│  │  RequestHandler    │  handleExecuteGraph(slug, inputs)      │       │
│  │  (graph execution) │  with _identifier overrides            │       │
│  └────────────────────┘                                        │       │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

## File Structure

```
src/
├── postgres/
│   ├── PostgresListener.hpp   # Listener class, Boost.Asio integration
│   └── PostgresListener.cpp   # SQL triggers, LISTEN, dispatch, graph exec
├── storage/
│   ├── GraphMetadata.hpp      # Structs ListenMapping + ListenConfig
│   └── GraphStorage.cpp       # SQLite tables pg_listen_configs/mappings
├── server/
│   ├── RequestHandler.cpp     # pg-triggers API handlers
│   └── HttpSession.cpp        # HTTP routes
└── client/examples/
    └── viewer.html            # Triggers tab (UI)
```

## Dedicated libpq Connection

PostgresListener uses a **separate libpq connection** from PostgresPool (which uses pqxx). This is necessary because:

- `PQsocket()` provides the file descriptor for Boost.Asio integration
- `PQconsumeInput()` + `PQnotifies()` enable async reception of notifications
- pqxx does not provide easy access to the raw socket

### Boost.Asio Integration

```cpp
// Duplicate the fd to avoid double-close with PQfinish
int fd = PQsocket(m_conn);
m_pgSocket = std::make_unique<net::posix::stream_descriptor>(m_ioc, ::dup(fd));

// Async polling
m_pgSocket->async_wait(
    net::posix::stream_descriptor::wait_read,
    [this](boost::system::error_code ec) {
        if (!ec && m_running) {
            PQconsumeInput(m_conn);
            PGnotify* notify;
            while ((notify = PQnotifies(m_conn)) != nullptr) {
                handleNotification(notify->relname, notify->extra);
                PQfreemem(notify);
            }
            waitForNotification();  // re-schedule
        }
    });
```

The `::dup(fd)` is necessary because `PQfinish()` closes the original socket, and `stream_descriptor` also closes its fd on destruction. Without duplication, there would be a double-close.

## PostgreSQL Triggers

Two types of hardcoded triggers, created at server startup via `setupTriggers()`:

### 1. TaskStep Status Change (`taskstep_status`)

Listens for INSERT/UPDATE on `process_taskstep`.

```sql
CREATE OR REPLACE FUNCTION anode_notify_taskstep() RETURNS trigger AS $$
BEGIN
    PERFORM pg_notify('anode_taskstep', NEW.id::text);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER anode_taskstep_trigger
    AFTER INSERT OR UPDATE ON process_taskstep
    FOR EACH ROW EXECUTE FUNCTION anode_notify_taskstep();
```

**SELECT query** (executed upon receiving the notification):

```sql
SELECT id, task_id, status
FROM process_taskstep
WHERE id = <payload>
```

**Available fields**: `id`, `task_id`, `status`

### 2. Data Value Change (`data_change`)

Listens for INSERT/UPDATE on 3 tables. All 3 converge to the same output fields.

#### data_datavalue

```sql
CREATE OR REPLACE FUNCTION anode_notify_datavalue() RETURNS trigger AS $$
BEGIN
    PERFORM pg_notify('anode_datavalue', NEW.id::text);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

```sql
SELECT context_context.task_id, context_context.question_id, data_datavalue.id AS datavalue_id
FROM data_datavalue
INNER JOIN context_context ON data_datavalue.context_id = context_context.id
WHERE data_datavalue.id = <payload>
```

#### data_datavalue_meta_data_values

```sql
CREATE OR REPLACE FUNCTION anode_notify_datavalue_meta() RETURNS trigger AS $$
BEGIN
    PERFORM pg_notify('anode_datavalue_meta', NEW.datavalue_id::text);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

```sql
SELECT context_context.task_id, context_context.question_id, data_datavalue.id AS datavalue_id
FROM data_datavalue_meta_data_values
INNER JOIN data_datavalue ON data_datavalue_meta_data_values.datavalue_id = data_datavalue.id
INNER JOIN context_context ON data_datavalue.context_id = context_context.id
WHERE data_datavalue_meta_data_values.datavalue_id = <payload>
```

#### data_metadatavalue

```sql
CREATE OR REPLACE FUNCTION anode_notify_metadatavalue() RETURNS trigger AS $$
BEGIN
    PERFORM pg_notify('anode_metadatavalue', NEW.id::text);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

```sql
SELECT context_context.task_id, context_context.question_id, data_datavalue.id AS datavalue_id
FROM data_metadatavalue
INNER JOIN data_datavalue_meta_data_values ON data_metadatavalue.id = data_datavalue_meta_data_values.metadatavalue_id
INNER JOIN data_datavalue ON data_datavalue_meta_data_values.datavalue_id = data_datavalue.id
INNER JOIN context_context ON data_datavalue.context_id = context_context.id
WHERE data_metadatavalue.id = <payload>
```

**Available fields**: `task_id`, `question_id`, `datavalue_id`

## SQLite Schema

Trigger configurations are saved in the graph SQLite database (not in PostgreSQL).

### Table `pg_listen_configs`

| Column | Type | Description |
|---------|------|-------------|
| `id` | INTEGER PK | Auto-increment |
| `trigger_type` | TEXT NOT NULL | `"taskstep_status"` or `"data_change"` |
| `graph_slug` | TEXT NOT NULL | FK to graphs.slug (CASCADE delete) |
| `is_enabled` | INTEGER | 1 = active, 0 = disabled |
| `created_at` | TEXT | ISO 8601 timestamp |
| `updated_at` | TEXT | ISO 8601 timestamp |

UNIQUE constraint on `(trigger_type, graph_slug)`: only one config per type per graph.

### Table `pg_listen_mappings`

| Column | Type | Description |
|---------|------|-------------|
| `id` | INTEGER PK | Auto-increment |
| `config_id` | INTEGER NOT NULL | FK to pg_listen_configs.id (CASCADE delete) |
| `output_field` | TEXT NOT NULL | SELECT field (e.g., `"task_id"`) |
| `identifier` | TEXT NOT NULL | `_identifier` of the scalar node in the graph |

### C++ Structures

```cpp
// In src/storage/GraphMetadata.hpp

struct ListenMapping {
    std::string outputField;   // Trigger SELECT column
    std::string identifier;    // Scalar node _identifier
};

struct ListenConfig {
    int64_t id = 0;
    std::string triggerType;   // "taskstep_status" | "data_change"
    std::string graphSlug;
    bool isEnabled = true;
    std::vector<ListenMapping> mappings;
    std::string createdAt;
    std::string updatedAt;
};
```

### GraphStorage Methods

```cpp
int64_t saveListenConfig(const ListenConfig& config);
std::optional<ListenConfig> getListenConfig(int64_t configId);
std::vector<ListenConfig> listListenConfigs(const std::string& triggerType = "");
std::vector<ListenConfig> listListenConfigsForGraph(const std::string& slug);
void updateListenConfig(const ListenConfig& config);
void deleteListenConfig(int64_t configId);
```

## REST API

### GET /api/pg-listeners

Returns the available trigger types (hardcoded).

**Response:**
```json
{
  "status": "ok",
  "listeners": [
    {
      "type": "taskstep_status",
      "label": "Task Step Status Change",
      "description": "Triggered when a process_taskstep row is inserted or updated",
      "output_fields": ["id", "task_id", "status"]
    },
    {
      "type": "data_change",
      "label": "Data Value Change",
      "description": "Triggered when data_datavalue, data_datavalue_meta_data_values, or data_metadatavalue is modified",
      "output_fields": ["task_id", "question_id", "datavalue_id"]
    }
  ]
}
```

### GET /api/graph/:slug/pg-triggers

Lists the triggers configured for a graph.

**Response:**
```json
{
  "status": "ok",
  "triggers": [
    {
      "id": 1,
      "trigger_type": "taskstep_status",
      "graph_slug": "my-pipeline",
      "is_enabled": true,
      "mappings": [
        { "output_field": "task_id", "identifier": "task_id_param" },
        { "output_field": "status", "identifier": "status_param" }
      ],
      "created_at": "2026-02-06T10:00:00.000Z",
      "updated_at": "2026-02-06T10:00:00.000Z"
    }
  ]
}
```

### POST /api/graph/:slug/pg-triggers

Creates a new trigger for a graph.

**Body:**
```json
{
  "trigger_type": "taskstep_status",
  "is_enabled": true,
  "mappings": [
    { "output_field": "task_id", "identifier": "task_id_param" },
    { "output_field": "status", "identifier": "status_param" }
  ]
}
```

**Response:** `201 Created`
```json
{
  "status": "ok",
  "id": 1,
  "message": "Trigger config created"
}
```

### PUT /api/graph/:slug/pg-triggers/:id

Updates a trigger (partial: only provided fields are modified).

**Body:**
```json
{
  "is_enabled": false,
  "mappings": [
    { "output_field": "task_id", "identifier": "new_param" }
  ]
}
```

### DELETE /api/graph/:slug/pg-triggers/:id

Deletes a trigger. Mappings are deleted via cascade.

## Dispatch to Graphs

When a notification arrives:

1. **Channel** identifies the type: `anode_taskstep` -> `"taskstep_status"`, `anode_datavalue`/`anode_datavalue_meta`/`anode_metadatavalue` -> `"data_change"`
2. **Payload** = ID of the modified row
3. **SELECT**: executes the hardcoded query to retrieve the data (via PostgresPool)
4. **Configs**: loads ListenConfig entries from SQLite for the triggerType
5. **Mappings**: builds a JSON `inputs` object by mapping `output_field` -> `identifier`
6. **Execution**: calls `RequestHandler::handleExecuteGraph(slug, {inputs})` which injects the values into the scalar nodes via `_identifier`

```cpp
void PostgresListener::dispatchToGraphs(const std::string& triggerType, const json& data) {
    auto configs = m_storage->listListenConfigs(triggerType);

    for (const auto& config : configs) {
        if (!config.isEnabled) continue;

        json inputs = json::object();
        for (const auto& mapping : config.mappings) {
            if (data.contains(mapping.outputField)) {
                inputs[mapping.identifier] = data[mapping.outputField];
            }
        }

        json request = {{"inputs", inputs}};
        RequestHandler::instance().handleExecuteGraph(config.graphSlug, request);
    }
}
```

A single trigger can trigger **multiple graphs** in parallel. Each graph has its own mapping.

## Frontend (viewer.html)

### Triggers Tab

A "Triggers" tab in the side panel (next to Parameters and Equations) allows you to:

- **Add a trigger**: "+ Add Trigger" button with a dropdown of available types (filters out those already configured)
- **Configure mappings**: for each `output_field`, a dropdown lists the `_identifier` values of the graph's scalar nodes
- **Enable/disable**: ON/OFF toggle per trigger
- **Delete**: Delete button per trigger
- **Auto-save**: 500ms debounce on mappings, 300ms on the toggle

### Data Loading

```javascript
async function loadTriggers() {
    // 1. Available types
    const listeners = await fetch(`${BASE_URL}/api/pg-listeners`);

    // 2. Triggers configured for this graph
    const triggers = await fetch(`${BASE_URL}/api/graph/${GRAPH_SLUG}/pg-triggers`);

    // 3. Available _identifiers (from the graph's scalar nodes)
    const identifiers = extractIdentifiers(graphData);
}
```

## Startup Integration

In `main.cpp`, after PostgresPool and GraphStorage configuration:

```cpp
#include "postgres/PostgresListener.hpp"

// Start the listener if PostgreSQL is configured
std::unique_ptr<postgres::PostgresListener> pgListener;
if (postgres::PostgresPool::instance().isConfigured() &&
    RequestHandler::instance().hasGraphStorage()) {
    try {
        pgListener = std::make_unique<postgres::PostgresListener>(
            ioc, RequestHandler::instance().getGraphStorage());
        pgListener->start(postgres::PostgresPool::instance().getConnectionString());
    } catch (const std::exception& e) {
        LOG_WARN("Failed to start PostgreSQL listener: " + std::string(e.what()));
    }
}

// Shutdown
shutdown_handler = [&]() {
    if (pgListener) pgListener->stop();
    server.stop();
    ioc.stop();
};
```

Listener startup failure does not block the server.

## CMake

PostgresListener is in the `server` lib (not `postgres`) to avoid circular dependencies: it needs `RequestHandler` and `GraphStorage` which are in `server` and `storage`.

```cmake
# libpq (raw C API for LISTEN/NOTIFY)
pkg_check_modules(LIBPQ REQUIRED libpq)

# server lib includes PostgresListener
add_library(server
    ...
    src/postgres/PostgresListener.cpp
)

target_link_libraries(server PUBLIC
    ...
    postgres   # for PostgresPool + libpq
)
```

## Verification

```bash
# Build
cd build && cmake .. && make -j4

# Run with PostgreSQL
./anodeServer --postgres @../postgres.conf -g ../graphs.db

# Check triggers in psql
\df anode_notify_*
SELECT tgname FROM pg_trigger WHERE tgname LIKE 'anode_%';

# Test manually
UPDATE process_taskstep SET status = 'completed' WHERE id = 1;
# → Logs: [PG-LISTEN] Notification received: channel=anode_taskstep payload=1

# Configure a trigger via the viewer.html UI → Triggers tab
# Verify in SQLite
sqlite3 graphs.db "SELECT * FROM pg_listen_configs;"
sqlite3 graphs.db "SELECT * FROM pg_listen_mappings;"
```
