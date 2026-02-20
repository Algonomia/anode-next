# AnodeServer - Architecture Overview

## Project Structure

```
anodeServer/
├── src/
│   ├── dataframe/          # Core DataFrame library
│   │   ├── DataFrame.hpp/cpp
│   │   ├── DataFrameFilter.hpp/cpp
│   │   ├── DataFrameSorter.hpp/cpp
│   │   ├── DataFrameAggregator.hpp/cpp
│   │   ├── DataFrameSerializer.hpp/cpp
│   │   ├── DataFrameIO.hpp/cpp
│   │   ├── Column.hpp
│   │   └── StringPool.hpp
│   ├── nodes/              # Node system framework
│   │   ├── Types.hpp/cpp           # NodeType, Workload, PortType
│   │   ├── NodeBuilder.hpp/cpp     # Fluent API for node registration
│   │   ├── NodeRegistry.hpp/cpp    # Central node definition registry
│   │   ├── NodeExecutor.hpp/cpp    # Graph execution engine
│   │   ├── NodeContext.hpp/cpp     # Runtime context (I/O)
│   │   ├── PluginContext.hpp       # Context passed to plugins
│   │   ├── plugin_init.hpp.in      # CMake template for plugin header
│   │   └── nodes/                  # Plugin directories (auto-discovered)
│   │       ├── common/             # Built-in generic nodes
│   │       └── <plugin>/           # Domain-specific plugins
│   ├── server/             # HTTP Server
│   │   ├── HttpServer.hpp/cpp
│   │   ├── HttpSession.hpp/cpp
│   │   └── RequestHandler.hpp/cpp  # + plugin route extension
│   ├── storage/            # SQLite graph persistence
│   │   └── GraphStorage.hpp/cpp
│   ├── postgres/           # PostgreSQL connection pool
│   │   └── PostgresPool.hpp/cpp
│   ├── benchmark/          # Benchmarking tools
│   │   └── BenchmarkReporter.hpp/cpp
│   └── client/             # TypeScript client library
│       ├── lib/
│       │   ├── AnodeClient.ts
│       │   └── index.ts
│       └── examples/
│           └── index.html
├── examples/               # C++ examples and benchmarks
├── docs/                   # Documentation
├── build/                  # CMake build directory
│   └── generated/          # Auto-generated headers (plugin_init.hpp)
└── main.cpp                # Server entry point (uses plugin_init.hpp)
```

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Client Layer                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   Web Browser   │  │   TypeScript    │  │    curl/API     │  │
│  │   (Tabulator)   │  │   AnodeClient   │  │     clients     │  │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘  │
└───────────┼────────────────────┼────────────────────┼───────────┘
            │                    │                    │
            └────────────────────┼────────────────────┘
                                 │ HTTP/JSON
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Server Layer                              │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                      HttpServer                              ││
│  │  • Boost.Asio async I/O                                     ││
│  │  • TCP acceptor                                             ││
│  └─────────────────────────┬───────────────────────────────────┘│
│                            │                                     │
│  ┌─────────────────────────▼───────────────────────────────────┐│
│  │                      HttpSession                             ││
│  │  • Beast HTTP parsing                                       ││
│  │  • Request/Response handling                                ││
│  │  • CORS support                                             ││
│  └─────────────────────────┬───────────────────────────────────┘│
│                            │                                     │
│  ┌─────────────────────────▼───────────────────────────────────┐│
│  │                    RequestHandler                            ││
│  │  • Singleton pattern                                        ││
│  │  • Dataset management                                       ││
│  │  • Query execution pipeline                                 ││
│  └─────────────────────────┬───────────────────────────────────┘│
└────────────────────────────┼────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                      DataFrame Layer                             │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                       DataFrame                              ││
│  │  • Column management                                        ││
│  │  • Operation delegation                                     ││
│  └──────┬──────────┬──────────┬──────────┬────────────────────┘│
│         │          │          │          │                      │
│    ┌────▼────┐┌────▼────┐┌────▼────┐┌────▼────┐                │
│    │ Filter  ││ Sorter  ││Aggregat.││Serializ.│                │
│    └─────────┘└─────────┘└─────────┘└─────────┘                │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                   Column Types                               ││
│  │  IntColumn │ DoubleColumn │ StringColumn (+ StringPool)     ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

## Modular Architecture

AnodeServer uses a **plugin system** to separate core functionality from domain-specific extensions. Node implementations live in plugin directories under `src/nodes/nodes/`, each with its own CMakeLists.txt. Plugins are auto-discovered at build time -- adding a directory is enough, no core files need modification.

```
src/nodes/nodes/
├── common/        ← Built-in nodes: scalar, csv, math, aggregate, ...
└── <plugin>/      ← Domain plugins (can be git submodules)
```

Each plugin can:
- Register node types
- Load caches and create storage at startup
- Register custom HTTP endpoints
- Run background services (listeners)

See [PLUGINS.md](PLUGINS.md) for the full plugin system documentation.

## Design Principles

### 1. Single Responsibility Principle (SRP)
Each class has one clear responsibility:
- `DataFrame`: Data structure management
- `DataFrameFilter`: Filtering logic
- `DataFrameSorter`: Sorting logic
- `DataFrameAggregator`: GroupBy and aggregations
- `DataFrameSerializer`: JSON/String serialization

### 2. Performance First
- **Typed columns**: No `std::variant`, direct type access
- **String interning**: `StringPool` for O(1) string comparisons
- **Cache-friendly**: Contiguous memory layout per column
- **Lazy loading**: Server-side pagination with offset/limit

### 3. Async I/O
- Non-blocking HTTP server using Boost.Asio
- `shared_ptr` session management for connection lifecycle

## Data Flow

### Query Execution Pipeline

```
1. HTTP Request (POST /api/dataset/query)
        │
        ▼
2. JSON Parsing (nlohmann/json)
        │
        ▼
3. Operation Pipeline:
   ┌──────────────────────────────────────┐
   │  for each operation in request:      │
   │    filter → orderby → groupby → ... │
   └──────────────────────────────────────┘
        │
        ▼
4. Pagination (offset + limit)
        │
        ▼
5. JSON Serialization
        │
        ▼
6. HTTP Response
```

## Dependencies

| Library | Purpose |
|---------|---------|
| Boost.Asio | Async I/O |
| Boost.Beast | HTTP protocol |
| nlohmann/json | JSON handling |
| CMake | Build system |

## Build

```bash
mkdir build && cd build
cmake ..
make -j4
./anodeServer -d path/to/data.csv
```

## Related Documentation

- [Plugin System](PLUGINS.md)
- [DataFrame Architecture](DATAFRAME.md)
- [Server Architecture](SERVER.md)
- [Client Library](../api/CLIENT.md)
- [API Reference](../api/REST-API.md)
- [Node System](../nodes/SYSTEM.md)
- [Dynamic Nodes](../nodes/DYNAMIC-NODES.md)
