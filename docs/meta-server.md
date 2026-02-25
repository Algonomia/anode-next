# AnodeServer Meta - Management UI

A lightweight web interface to manage AnodeServer: toggle plugins, build, start/stop the server, and configure parameters. Supports automated addon deployment via environment variables for CI/CD and infrastructure automation.

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

### Remote Sources

Plugins can be installed from remote git repositories (GitHub, GitLab, or any git host). Each source is defined by a URL, an optional authentication token, a branch, and a plugin name.

Sources can be managed via the UI or the API (`/api/sources`). See also [Auto-Deploy](#auto-deploy) for automated installation.

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

## Auto-Deploy

The meta server supports fully automated addon deployment: define which addons to install, provide authentication tokens, and let the pipeline handle clone, build, and startup automatically.

### Environment Variables

| Variable | Description |
|----------|-------------|
| `ANODE_SOURCES` | JSON array of sources to install (see format below) |
| `ANODE_SOURCE_<NAME>_TOKEN` | Authentication token for a specific source (name uppercased) |
| `ANODE_AUTO_DEPLOY` | Set to `true` to trigger the deploy pipeline on startup |

The `--auto-deploy` CLI flag can be used instead of `ANODE_AUTO_DEPLOY=true`.

### Source Format (`ANODE_SOURCES`)

```json
[
  {
    "name": "myplugin",
    "url": "https://github.com/org/myplugin.git",
    "type": "github",
    "branch": "main"
  }
]
```

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `url` | yes | — | Git repository URL |
| `name` | no | derived from URL | Plugin directory name (must match `/^[a-z][a-z0-9_]*$/`) |
| `type` | no | `github` | `github`, `gitlab`, or `other` (affects token authentication) |
| `branch` | no | `main` | Git branch to clone |
| `token` | no | — | Auth token (prefer `ANODE_SOURCE_<NAME>_TOKEN` env var instead) |

Sources from `ANODE_SOURCES` are merged into `meta-config.json` at startup: existing sources (same name) are updated, new ones are added.

### Token Resolution

Tokens are resolved at git operation time with the following priority:

1. `ANODE_SOURCE_<NAME>_TOKEN` environment variable (never persisted to config)
2. `token` field from the source definition

For GitHub, the token is used as the username in the git URL. For GitLab, `oauth2:<token>` is used.

### Deploy Pipeline

When `ANODE_AUTO_DEPLOY=true` (or `--auto-deploy`), the following pipeline runs automatically after the meta server starts listening:

1. **Clone** — Install all configured sources that are not yet present
2. **Build** — Run `cmake .. && make -j$(nproc)`
3. **Start** — Launch `anodeServer` (+ Vite client if `autoStartClient` is enabled)

If any step fails, the pipeline stops and reports the error. Progress is streamed via SSE on `/api/deploy/stream`.

The pipeline can also be triggered manually via `POST /api/deploy`.

### Example

```bash
# Full automated deployment
ANODE_SOURCES='[{"name":"myplugin","url":"https://github.com/org/myplugin.git"}]' \
ANODE_SOURCE_MYPLUGIN_TOKEN=ghp_xxxxxxxxxxxx \
ANODE_AUTO_DEPLOY=true \
node meta/server.js
```

```bash
# With Docker
docker run \
  -e ANODE_SOURCES='[{"name":"myplugin","url":"https://github.com/org/myplugin.git"}]' \
  -e ANODE_SOURCE_MYPLUGIN_TOKEN=ghp_xxxxxxxxxxxx \
  -e ANODE_AUTO_DEPLOY=true \
  anode-meta
```

## API Reference

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/status` | Global state (server, build, deploy, plugins, sources, config) |
| `GET` | `/api/plugins` | List plugins `[{name, enabled, locked}]` |
| `POST` | `/api/plugins/:name/enable` | Enable a plugin |
| `POST` | `/api/plugins/:name/disable` | Disable a plugin |
| `POST` | `/api/config` | Save config (JSON body merged into current) |
| `POST` | `/api/build` | Start build (409 if already building) |
| `GET` | `/api/build/stream` | SSE stream of build output |
| `POST` | `/api/server/start` | Start anodeServer |
| `POST` | `/api/server/stop` | Stop anodeServer |
| `GET` | `/api/server/stream` | SSE stream of server output |
| `POST` | `/api/client/start` | Start Vite dev server |
| `POST` | `/api/client/stop` | Stop Vite dev server |
| `GET` | `/api/client/stream` | SSE stream of client output |
| `GET` | `/api/sources` | List source configurations |
| `POST` | `/api/sources` | Add a source `{type, url, token, branch, name}` |
| `PUT` | `/api/sources/:id` | Update a source |
| `DELETE` | `/api/sources/:id` | Delete a source configuration |
| `POST` | `/api/sources/:id/test` | Test git connection (returns branches) |
| `POST` | `/api/sources/:id/install` | Clone plugin from source |
| `POST` | `/api/sources/:id/update` | Pull latest changes |
| `POST` | `/api/sources/:id/remove` | Delete plugin directory |
| `GET` | `/api/sources/stream` | SSE stream of git operations |
| `POST` | `/api/deploy` | Start deploy pipeline (409 if already running) |
| `GET` | `/api/deploy/status` | Deploy status `{status, error}` |
| `GET` | `/api/deploy/stream` | SSE stream of deploy pipeline |

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
  server.js             # Backend
  public/
    index.html          # Single-page UI
  meta-config.json      # Auto-created, gitignored
```
