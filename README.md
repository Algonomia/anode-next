# AnodeServer

A high-performance **node-graph execution engine** built in C++23. Define data transformation pipelines as visual graphs, execute them server-side, and visualize results in a browser-based editor.

## Key Features

- **Visual Graph Editor** -- Browser-based node editor (TypeScript / LiteGraph) with real-time execution feedback
- **Columnar DataFrame Engine** -- Typed columns (Int, Double, String) with filtering, sorting, grouping, pivoting, and joins
- **Plugin Architecture** -- Auto-discovered C++ plugins with hot-reload via the meta management server
- **PostgreSQL Integration** -- Connection pool, dynamic SQL queries, and LISTEN/NOTIFY triggers for automated execution
- **REST API + SSE Streaming** -- Query datasets, execute graphs (blocking or streamed), manage graph definitions
- **SQLite Persistence** -- Versioned graph storage with full CRUD and execution history
- **Dynamic Equations** -- Inject mathematical expressions at runtime, compiled into graph nodes

## Quick Start

### Prerequisites

- C++23 compiler (GCC 13+ or Clang 17+)
- CMake >= 3.14
- pkg-config

```bash
# Ubuntu/Debian
sudo apt-get install \
    build-essential cmake pkg-config \
    libboost-system-dev \
    libpqxx-dev \
    libpq-dev \
    libsqlite3-dev
```

The following are fetched automatically by CMake if not installed:
- nlohmann/json, SQLite3 (amalgamation), Catch2, hosseinmoein/DataFrame

### Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Run

```bash
# Minimal (no PostgreSQL)
./anodeServer

# With PostgreSQL
./anodeServer --postgres @../postgres.conf

# With a dataset
./anodeServer --dataset ../examples/customers-10000.csv
```

The server listens on `http://localhost:8080` by default.

### Frontend

```bash
cd src/client
npm install
npm run dev
```

Opens the graph editor at `http://localhost:5173`.

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  Browser: Graph Editor / Tabulator / API clients         │
└──────────────┬───────────────────────────────────────────┘
               │ REST + SSE
┌──────────────▼───────────────────────────────────────────┐
│  HTTP Server (Boost.Beast/Asio)                          │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ Graph CRUD   │  │  Execution   │  │  Plugin Routes │  │
│  └─────────────┘  └──────────────┘  └────────────────┘  │
├──────────────────────────────────────────────────────────┤
│  Node System         │  DataFrame Engine                 │
│  - Registry          │  - Typed columns                  │
│  - Graph executor    │  - Filter / Sort / GroupBy        │
│  - Plugin loader     │  - Pivot / Join / Serialize       │
├──────────────────────┼───────────────────────────────────┤
│  SQLite (graphs)     │  PostgreSQL (optional)            │
└──────────────────────┴───────────────────────────────────┘
```

## Documentation

Full documentation is available in the [docs/](docs/README.md) directory:

- [Build & Configuration](docs/getting-started/BUILD.md)
- [Architecture Overview](docs/architecture/OVERVIEW.md)
- [REST API Reference](docs/api/REST-API.md)
- [Plugin System](docs/architecture/PLUGINS.md)
- [Node Catalog](docs/nodes/CATALOG.md)
- [Graph Editor](docs/graph/EDITOR.md)

## Project Structure

```
├── src/
│   ├── dataframe/       # Columnar DataFrame library
│   ├── nodes/           # Node system framework + plugins
│   ├── server/          # HTTP server (Boost.Beast/Asio)
│   ├── storage/         # SQLite graph persistence
│   ├── postgres/        # PostgreSQL connection pool
│   └── client/          # TypeScript graph editor + client library
├── meta/                # Meta management server (Node.js)
├── examples/            # Sample datasets
├── tests/               # C++ test suites
└── docs/                # Documentation
```

## Tests

```bash
cd build

# All tests
ctest

# Individual suites
./dataframe_tests
./node_tests
./postgres_tests    # requires POSTGRES_TEST_CONN env var
```

## License

This project is licensed under the [GNU Affero General Public License v3.0](LICENSE.md).