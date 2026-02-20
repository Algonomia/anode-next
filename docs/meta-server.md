# AnodeServer Meta - Management UI

A lightweight web interface to manage AnodeServer: toggle plugins, build, start/stop the server, and configure parameters.

## Quick Start

```bash
cd meta
npm install
npm start
```

Open http://localhost:9090

## Features

### Plugin Management

Plugins live in `src/nodes/nodes/*/`. Each subdirectory with a `CMakeLists.txt` is a plugin.

- **common** — Core plugin, always enabled (locked)
- Other plugins (e.g. **myplugin**) can be toggled on/off

Disabling a plugin moves its `CMakeLists.txt` into a `disabled/` subdirectory. CMake's glob at build time only picks up root-level `CMakeLists.txt` files, so disabled plugins are excluded from the build.

A "Rebuild required" banner appears after toggling a plugin.

### Build

Clicking **Build** runs `cmake .. && make -j$(nproc)` inside the `build/` directory. Output streams in real-time to the Build Console via Server-Sent Events (SSE).

Only one build can run at a time.

### Server Control

**Start** spawns `build/anodeServer` with the current configuration. **Stop** sends SIGTERM (with a SIGKILL fallback after 5s). Server stdout/stderr streams to the Server Console.

### Configuration

All settings are saved to `meta/meta-config.json` (gitignored). Defaults:

| Setting | Default | CLI flag |
|---------|---------|----------|
| Address | `0.0.0.0` | `-a` |
| Port | `8080` | `-p` |
| Dataset path | `../examples/customers-500000.csv` | `-d` |
| Graphs DB path | `./graphs.db` | `-g` |
| Log level | `info` | `-l` |
| Profiler | enabled | `--no-profiler` |
| Postgres config | (empty) | `--postgres @file` |

The postgres config textarea accepts key=value lines (same format as `postgres.conf`). When non-empty, it's written to `build/meta-postgres.conf` and passed via `--postgres @path`.

## API Reference

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/status` | Global state (server, build, plugins, config) |
| `GET` | `/api/plugins` | List plugins `[{name, enabled, locked}]` |
| `POST` | `/api/plugins/:name/enable` | Enable a plugin |
| `POST` | `/api/plugins/:name/disable` | Disable a plugin |
| `POST` | `/api/config` | Save config (JSON body merged into current) |
| `POST` | `/api/build` | Start build (409 if already building) |
| `GET` | `/api/build/stream` | SSE stream of build output |
| `POST` | `/api/server/start` | Start anodeServer |
| `POST` | `/api/server/stop` | Stop anodeServer |
| `GET` | `/api/server/stream` | SSE stream of server output |

### SSE Message Format

```json
{"type": "stdout|stderr|system", "text": "line content"}
```

- **stdout** — standard output (white)
- **stderr** — standard error (orange)
- **system** — meta messages like "Build started" (grey)

A ring buffer of 10,000 lines is kept per stream so reconnecting clients receive history.

## File Structure

```
meta/
  package.json          # express + tree-kill
  server.js             # Backend (~290 lines)
  public/
    index.html          # Single-page UI (~310 lines)
  meta-config.json      # Auto-created, gitignored
```
