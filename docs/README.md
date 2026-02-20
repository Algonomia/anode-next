# AnodeServer - Documentation

## Getting Started

| Document | Description |
|----------|-------------|
| [BUILD.md](getting-started/BUILD.md) | Prerequisites, compilation, configuration, startup |

## Architecture

| Document | Description |
|----------|-------------|
| [OVERVIEW.md](architecture/OVERVIEW.md) | Project and component overview |
| [PLUGINS.md](architecture/PLUGINS.md) | Plugin system: auto-discovery, lifecycle, HTTP routes, creation guide |
| [DATAFRAME.md](architecture/DATAFRAME.md) | DataFrame library: typed columns, operations, performance |
| [SERVER.md](architecture/SERVER.md) | Boost.Beast/Asio HTTP server, sessions, CORS, logger, profiler |

## API

| Document | Description |
|----------|-------------|
| [REST-API.md](api/REST-API.md) | Complete REST endpoint reference |
| [CLIENT.md](api/CLIENT.md) | TypeScript client library and graph editor application |

## Node System

| Document | Description |
|----------|-------------|
| [SYSTEM.md](nodes/SYSTEM.md) | Node system architecture: types, workload, builder, executor |
| [CATALOG.md](nodes/CATALOG.md) | Reference of all nodes by category |
| [SCALAR-NODES.md](nodes/SCALAR-NODES.md) | Scalar nodes and parameter override support |
| [DYNAMIC-NODES.md](nodes/DYNAMIC-NODES.md) | Dynamic equation injection at execution time |
| [IMPLEMENTATION-CHECKLIST.md](nodes/IMPLEMENTATION-CHECKLIST.md) | Checklist for implementing a new node |

## Graph

| Document | Description |
|----------|-------------|
| [EDITOR.md](graph/EDITOR.md) | Visual graph editor, keyboard shortcuts, workflows |
| [STORAGE.md](graph/STORAGE.md) | SQLite graph persistence, versioning, CRUD |
| [SERIALIZATION.md](graph/SERIALIZATION.md) | Graph JSON serialization format |

## PostgreSQL

| Document | Description |
|----------|-------------|
| [INTEGRATION.md](postgresql/INTEGRATION.md) | PostgresPool, DynRequest, database/process/form nodes |
| [PERIMETER-CACHE.md](postgresql/PERIMETER-CACHE.md) | In-memory cache for perimeter decompositions |
| [LISTEN-NOTIFY.md](postgresql/LISTEN-NOTIFY.md) | Graph automation via PostgreSQL triggers |

## Features

| Document | Description |
|----------|-------------|
| [PARAMETER-OVERRIDES.md](features/PARAMETER-OVERRIDES.md) | Value injection at execution time via overrides |
| [EVENTS.md](features/EVENTS.md) | Drill-down: graph execution on timeline click |
| [DIFF-OUTPUT.md](features/DIFF-OUTPUT.md) | Side-by-side comparison of two CSVs with colored diff |

## Tools

| Document | Description |
|----------|-------------|
| [meta-server.md](meta-server.md) | Meta management UI: plugin toggle, build, server control |
