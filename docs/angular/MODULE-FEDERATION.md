# Angular Module Federation

## Overview

The Angular frontend uses **Native Federation** (`@angular-architects/native-federation`) to split the viewer UI into independent micro-frontends. A **host** shell loads **remote** components at runtime via import maps, allowing each piece to be developed, built, and deployed independently.

Currently implemented:

| Remote | Exposed component | Description |
|--------|-------------------|-------------|
| `remote-grid` | `GridViewerComponent` | AG Grid viewer for graph outputs |

## Architecture

```
src/angular/                     Angular workspace (monorepo)
├── angular.json                 Workspace configuration
├── package.json                 Shared dependencies
├── tsconfig.json
├── projects/
│   ├── host/                    Shell application (port 4200)
│   │   ├── federation.config.js
│   │   ├── proxy.conf.json
│   │   └── src/
│   │       ├── main.ts          initFederation('federation.manifest.json')
│   │       ├── bootstrap.ts     bootstrapApplication(AppComponent, appConfig)
│   │       ├── app/
│   │       │   ├── app.component.ts   Shell (header + router-outlet)
│   │       │   ├── app.config.ts      provideRouter, provideHttpClient
│   │       │   └── app.routes.ts      loadRemoteModule('remote-grid', './GridViewer')
│   │       └── assets/
│   │           └── federation.manifest.json   { "remote-grid": "http://localhost:4201/remoteEntry.json" }
│   │
│   └── remote-grid/             AG Grid micro-frontend (port 4201)
│       ├── federation.config.js
│       ├── proxy.conf.json
│       └── src/
│           ├── main.ts          initFederation()
│           ├── bootstrap.ts
│           └── app/
│               ├── app.component.ts
│               ├── app.config.ts
│               ├── app.routes.ts          Default route → ViewerComponent
│               ├── viewer/
│               │   ├── viewer.component.ts          Main component (exposed via federation)
│               │   ├── viewer.component.html         Primary + drilldown sections
│               │   └── viewer.component.scss
│               ├── components/
│               │   ├── data-grid/
│               │   │   └── data-grid.component.ts   AG Grid wrapper
│               │   └── list-viewer/
│               │       └── list-viewer.component.ts  List display + drilldown event emission
│               ├── services/
│               │   └── anode-api.service.ts          HTTP client for the C++ REST API
│               └── shared/
│                   ├── models.ts                     DrilldownEvent, ListItem, StatusEvent
│                   └── utils.ts
```

## How Federation Works

```
Browser → http://localhost:4200/viewer?graph=my-graph
                │
                ▼
        ┌──────────────┐
        │     Host     │  Reads federation.manifest.json
        │   (port 4200) │  → finds remote-grid at localhost:4201
        └──────┬───────┘
               │ loadRemoteModule('remote-grid', './GridViewer')
               ▼
        ┌──────────────┐
        │ Remote Grid  │  Serves remoteEntry.json + JS chunks
        │  (port 4201)  │  Shared deps (Angular, RxJS, AG Grid) loaded once
        └──────┬───────┘
               │ GridViewerComponent renders
               ▼
        ┌──────────────┐
        │  C++ Backend │  /api/graph/:slug/outputs
        │  (port 8080)  │  /api/graph/:slug/output/:name
        └──────────────┘
```

Shared dependencies (Angular core, RxJS, AG Grid) are loaded only once by the host. The remote reuses them at runtime via the import map — no duplication.

## Technology Stack

| Component | Version | Notes |
|-----------|---------|-------|
| Angular | 19.x | Standalone components, signals |
| `@angular-architects/native-federation` | 19.x | Must match Angular major version |
| AG Grid | 32+ | Community + Enterprise (tree data) |
| `ag-grid-angular` | 32+ | Angular wrapper |
| Build tool | esbuild | Via `@angular/build:application` |

## Development

### Prerequisites

```bash
cd src/angular
npm install
```

### Start all services (3 terminals)

```bash
# Terminal 1: C++ backend
./cmake-build-debug/anode_server -g graphs.db

# Terminal 2: Remote
cd src/angular && npx ng serve remote-grid    # → http://localhost:4201

# Terminal 3: Host
cd src/angular && npx ng serve host           # → http://localhost:4200
```

### URLs

| URL | Description |
|-----|-------------|
| `http://localhost:4200/viewer?graph=SLUG` | Host loading remote via federation |
| `http://localhost:4201/?graph=SLUG` | Remote standalone (for development) |
| `http://localhost:4201/remoteEntry.json` | Federation manifest (verify it exists) |

### Build

```bash
# Build both for production
cd src/angular
npx ng build host
npx ng build remote-grid

# Output in dist/host/ and dist/remote-grid/
```

### API Proxy

Both dev servers proxy `/api/*` to `http://localhost:8080` (the C++ backend). Configured in `projects/*/proxy.conf.json` and referenced in `angular.json` under `serve-original.options.proxyConfig`.

## Key Configuration

### Federation configs

**Host** (`projects/host/federation.config.js`):

```js
module.exports = withNativeFederation({
  name: 'host',
  shared: {
    ...shareAll({ singleton: true, strictVersion: true, requiredVersion: 'auto' }),
  },
});
```

**Remote** (`projects/remote-grid/federation.config.js`):

```js
module.exports = withNativeFederation({
  name: 'remote-grid',
  exposes: {
    './GridViewer': './projects/remote-grid/src/app/grid-viewer/grid-viewer.component.ts',
  },
  shared: {
    ...shareAll({ singleton: true, strictVersion: true, requiredVersion: 'auto' }),
  },
});
```

### Host routing

```typescript
// projects/host/src/app/app.routes.ts
import { loadRemoteModule } from '@angular-architects/native-federation';

export const routes: Routes = [
  {
    path: 'viewer',
    loadComponent: () =>
      loadRemoteModule('remote-grid', './GridViewer')
        .then(m => m.GridViewerComponent),
  },
];
```

### angular.json builder notes

Native Federation wraps the standard esbuild builder. Each project has three architect targets:

| Target | Builder | Purpose |
|--------|---------|---------|
| `build` | `@angular-architects/native-federation:build` | Production build (delegates to `esbuild` target) |
| `serve` | `@angular-architects/native-federation:build` | Dev server (delegates to `serve-original` target, `dev: true`) |
| `esbuild` | `@angular/build:application` | Actual esbuild config (outputPath, polyfills, styles, etc.) |
| `serve-original` | `@angular-devkit/build-angular:dev-server` | Actual dev server (port, proxy) |

**Important**: The `build` target must NOT have `"dev": true` in its configurations — that option is only for the `serve` target. Setting `dev: true` on `build` causes the builder to look for a `buildTarget` property that doesn't exist on the esbuild options, resulting in a `Cannot read properties of undefined (reading 'split')` error.

## GridViewerComponent

The `GridViewerComponent` replicates the AG Grid functionality from the existing `examples/viewer.html`.

### Data flow

```
1. Read ?graph=SLUG from URL
2. GET /api/graph/:slug/outputs     → list of named outputs
3. Select first output
4. AG Grid infinite scroll datasource:
   POST /api/graph/:slug/output/:name  { limit, offset, operations }
   ← { columns, data, stats }
5. columnarToRows() converts to row objects
6. Display in AG Grid with dynamic columns
```

### Features

| Feature | Implementation |
|---------|---------------|
| Dynamic columns | Generated from API response `columns` array |
| Server-side pagination | AG Grid `rowModelType: 'infinite'`, `cacheBlockSize: 100` |
| Sort | `onSortChanged` → `purgeInfiniteCache()` → new request with `orderby` operation |
| Filter | `onFilterChanged` → `purgeInfiniteCache()` → new request with `filter` operation |
| Tree data | When columns include `__tree_path`, switches to `treeData: true` with `getDataPath` |
| Output tabs | Multiple outputs shown as tabs with row count badges |
| Stats bar | Shows total rows, current page, query time |
| List outputs | `ListViewerComponent` displays list-type outputs with label/value |
| Drilldown | Clicking a list item with `metadata.event` executes a target graph and displays its outputs with orange tabs below the primary section (see [EVENTS.md](../features/EVENTS.md)) |

### Corresponding viewer.html code

| Angular method | viewer.html reference |
|---------------|----------------------|
| `columnarToRows()` | Line 2749-2757 |
| `createColumnDefs()` | Line 2759-2771 |
| `initGrid()` | Line 2773-2816 |
| `createDatasource()` | Line 2921-2976 |
| `buildOperations()` | Line 2873-2919 |
| `loadTreeData()` | Line 2819-2871 |

## Adding a New Remote

To add a new micro-frontend (e.g., a timeline viewer):

### 1. Generate the application

```bash
cd src/angular
npx ng generate application remote-timeline --routing --style=scss --standalone
npx ng g @angular-architects/native-federation:init --project remote-timeline --port 4202 --type remote
```

### 2. Fix angular.json

Remove `"dev": true` from the `build.configurations.development` section (see builder notes above).

Add the proxy config to `serve-original.options`:

```json
"proxyConfig": "projects/remote-timeline/proxy.conf.json"
```

### 3. Create proxy.conf.json

```json
{ "/api": { "target": "http://localhost:8080", "secure": false, "changeOrigin": true } }
```

### 4. Expose your component

Edit `projects/remote-timeline/federation.config.js`:

```js
exposes: {
  './TimelineViewer': './projects/remote-timeline/src/app/timeline-viewer/timeline-viewer.component.ts',
},
```

### 5. Register in host manifest

Edit `projects/host/public/federation.manifest.json`:

```json
{
  "remote-grid": "http://localhost:4201/remoteEntry.json",
  "remote-timeline": "http://localhost:4202/remoteEntry.json"
}
```

### 6. Add host route

```typescript
{
  path: 'timeline',
  loadComponent: () =>
    loadRemoteModule('remote-timeline', './TimelineViewer')
      .then(m => m.TimelineViewerComponent),
},
```

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| `Cannot read properties of undefined (reading 'split')` | `dev: true` in `build` config | Remove `dev` from `build` configurations, keep only in `serve` |
| `remoteEntry.json` not found | Remote not running or wrong port | Start remote with `ng serve remote-grid`, check port in manifest |
| Shared dependency version mismatch | Different versions in host vs remote | Both share the same `package.json`, run `npm install` from workspace root |
| CORS errors loading remote chunks | Dev server blocking cross-origin | Angular CLI dev server allows CORS by default; if custom server, add `Access-Control-Allow-Origin: *` |
| Grid shows but no data | Proxy not configured | Check `proxy.conf.json` exists and is referenced in `angular.json` `serve-original.options.proxyConfig` |
| `@angular/build` version mismatch | `native-federation` major version != Angular major version | Install matching version: `npm install @angular-architects/native-federation@19` for Angular 19 |