# Perimeter Cache

> **Note:** This documents a domain-specific plugin. The perimeter cache is an example of how plugins can extend AnodeServer with specialized data structures and PostgreSQL integration.

C++ cache for perimeter decompositions (HV and AT). Loads all data at server startup and filters/pivots in memory on each node call.

## Architecture

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                          PERIMETER CACHE SYSTEM                              │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────┐                                                     │
│  │     main.cpp        │  At startup: loadHv() + loadAt()                   │
│  │  (startup)          │─────────────────────────────┐                       │
│  └─────────────────────┘                             │                       │
│                                                      ▼                       │
│                                             ┌─────────────────┐              │
│                                             │ PerimeterCache  │              │
│                                             │   (singleton)   │              │
│                                             └───────┬─────────┘              │
│                                                     │                        │
│           ┌─────────────────────────────────────────┼──────────┐             │
│           │ Loading (once)                          │          │             │
│           ▼                                         │          │             │
│  ┌──────────────────────┐                           │          │             │
│  │    PostgreSQL        │                           │          │             │
│  │ cache_load()         │     ┌─────────────────────┘          │             │
│  │ labels_load()        │     │ Queries (zero SQL)             │             │
│  └──────────────────────┘     ▼                                │             │
│                      ┌──────────────────┐    ┌─────────────────┴───┐         │
│                      │  Decomposition   │    │   Decomposition     │         │
│                      │  HV Node         │    │   AT Node           │         │
│                      │ getDecomposition │    │ getDecompositionAt  │         │
│                      │ Hv()            │    │ ()                  │         │
│                      └──────────────────┘    └─────────────────────┘         │
│                                                                              │
└───────────────────────────────────────────────────────────────────────────────┘
```

## File Structure

```
src/nodes/
├── PerimeterCache.hpp      # Singleton class, data structures
├── PerimeterCache.cpp      # Implementation: loading, filtering, pivoting
└── nodes/
    └── PerimeterNodes.cpp  # Nodes using the cache (zero SQL at execution time)
```

## Motivation

| Metric | Before (SQL on each call) | After (cache) |
|--------|---------------------------|---------------|
| Node execution time | ~2.5s | < 10ms |
| SQL queries per node | 2 | 0 |
| Startup cost | 0 | ~3s (once) |

The data volume and JOINs on `cache_pevhv_construction_tree` made the SQL query slow even with flat integer column optimization.

## SQL Loading Functions

### HV

**`anode_perimeter_pevhv_cache_load()`** - Parent-child relationships + validity

```sql
RETURNS TABLE(
    child_id INT,
    child_pel_id INT,
    real_start_validity INT,
    has_start INT,              -- 1 if real_start_validity IS NOT NULL, 0 otherwise
    real_end_validity INT,
    has_end INT,                -- 1 if real_end_validity IS NOT NULL, 0 otherwise
    parent_pel_id INT,
    parent_pev_id INT
)
```

JOINs: `perimeter_pevhv` (child) x `cache_pevhv_construction_tree` x `perimeter_pevhv` (parent) x LEFT JOIN `cache_pevhv_real_start_end_validities`.

**`anode_perimeter_pevhv_labels_load()`** - PEL + PEV Labels

```sql
RETURNS TABLE(entity_type TEXT, entity_id INT, entity_label TEXT)
```

UNION ALL of `perimeter_pelhv` (PEL labels) and `cache_pevhv_title` (PEV labels), unfiltered.

### AT

Same structure with AT tables:
- `anode_perimeter_pevat_cache_load()`
- `anode_perimeter_pevat_labels_load()`

### NULL Handling

PostgresPool converts `NULL` to `0` for integers. To distinguish `real_start_validity IS NULL` (= unlimited) from `real_start_validity = 0` (= epoch), the SQL functions return explicit `has_start`/`has_end` columns (1/0).

## C++ API

### PerimeterCache

```cpp
#include "nodes/PerimeterCache.hpp"

// Singleton
auto& cache = nodes::PerimeterCache::instance();

// Loading (at startup, after PostgresPool::configure)
cache.loadHv();
cache.loadAt();

// Status check
cache.isHvLoaded();  // true/false
cache.isAtLoaded();

// Decomposition (replaces 2 SQL queries + buildFlattenedDecomposition)
auto df = cache.getDecompositionHv(pelId, start, hasStart, end, hasEnd);
auto df = cache.getDecompositionAt(pelId, start, hasStart, end, hasEnd);

// For tests (without PostgreSQL)
cache.clear();
cache.loadHvFromData(children, byPel, pelLabels, pevLabels);
```

### Parameters for getDecompositionHv / getDecompositionAt

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `pelId` | int | (required) | Perimeter ID (ppelhv_id / ppelat_id) |
| `start` | int | 0 | Start timestamp (unix) |
| `hasStart` | bool | false | `true` if start is defined |
| `end` | int | 0 | End timestamp (unix) |
| `hasEnd` | bool | false | `true` if end is defined |

### Returned DataFrame

| Column | Type | Description |
|---------|------|-------------|
| `pevhv id` / `pevat id` | IntColumn | Child ID |
| `{pel_label}` | StringColumn | Parent PEV label |
| `{pel_label} id` | IntColumn | Parent PEV ID |
| `{pel_label} N` | StringColumn | If cnt > 1, suffixed with the number |
| `{pel_label} N id` | IntColumn | Corresponding ID |

"Root" columns are excluded.

## Internal Data Structures

```cpp
struct ParentLink {
    int pelId;   // parent pel_id
    int pevId;   // parent pev_id
};

struct ChildInfo {
    int pelId;              // child pel_id (for filtering by ppelhv_id)
    int realStart;          // real_start_validity
    int realEnd;            // real_end_validity
    bool hasStart;          // false = NULL = unlimited
    bool hasEnd;
    std::vector<ParentLink> parents;  // parent relationships
};

// Primary index: child_id -> ChildInfo
std::unordered_map<int, ChildInfo> m_hvChildren;

// Secondary index: child_pel_id -> [child_ids]
std::unordered_map<int, std::vector<int>> m_hvByPel;

// Label dictionaries
std::unordered_map<int, std::string> m_hvPelLabels;
std::unordered_map<int, std::string> m_hvPevLabels;
```

## buildDecomposition Algorithm

1. **Filter children**: `byPel[pelId]` gives the list of child_ids, then filter by validity:
   ```cpp
   // Reproduces get_perimeter_pevhv_valid_from_timestamp
   bool valid = (!hasStart || !child.hasStart || child.realStart <= start) &&
                (!hasEnd   || !child.hasEnd   || child.realEnd   >= end);
   ```

2. **Discover columns**: From the first valid child, group parents by `parent_pel_id`, sort by `pev_id`, build `finalLabel` (with suffix if cnt > 1), exclude "root".

3. **Pivot**: For each valid child, fill the columns with the corresponding PEV labels.

## Startup Integration

In `main.cpp`, after PostgreSQL configuration:

```cpp
if (!postgresConn.empty()) {
    postgres::PostgresPool::instance().configure(connString);

    try {
        nodes::PerimeterCache::instance().loadHv();
        nodes::PerimeterCache::instance().loadAt();
        LOG_INFO("Perimeter cache loaded");
    } catch (const std::exception& e) {
        LOG_WARN("Perimeter cache load failed: " + std::string(e.what()));
    }
}
```

Cache loading failure does not block server startup.

## Usage in Nodes

The `identifyPevHv decomposition` and `identifyPevAt decomposition` nodes use the cache:

```cpp
auto& cache = PerimeterCache::instance();
if (!cache.isHvLoaded()) {
    ctx.setError("Perimeter HV cache not loaded");
    return;
}

int pelId = resolveWorkloadInt(pelIdWL, csv);
int start = hasStart ? resolveWorkloadTimestamp(startWL, csv) : 0;
int end = hasEnd ? resolveWorkloadTimestamp(endWL, csv) : 0;

auto finalResult = cache.getDecompositionHv(pelId, start, hasStart, end, hasEnd);
ctx.setOutput("csv", finalResult);
```

The `identifyPelHv` and `identifyPelAt` nodes continue to use SQL (no cache needed, lightweight queries).

## Tests

```bash
cd build
cmake .. && make -j node_tests
./node_tests "[cache]"
```

Tests covered (without PostgreSQL):
- Basic decomposition: columns, labels, IDs
- Filtering by unknown pel_id (empty result)
- Date filtering: start, end, both, strict
- Suffixes cnt > 1 ("Level A 1", "Level A 2")
- Exclusion of "root" parents
- AT variant ("pevat id" columns)
- Reset via `clear()`
