# Graph Editor

## Overview

Visual node-based graph editor built with **@comfyorg/litegraph** (ComfyUI fork). Fully written in TypeScript with Vite bundling.

```
┌─────────────────────────────────────────────────────────────┐
│ [New] [Save] [Load] [Execute]           Graph: my-pipeline  │
├─────────────────────────────────────────────────────────────┤
│ ← Back / Root / MySubgraph                    (breadcrumb)  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                    LiteGraph Canvas                         │
│              (drag nodes, connect ports)                    │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ Ctrl+G Group  Ctrl+Shift+G Subgraph  Ctrl+Z/Y Undo/Redo    │
└─────────────────────────────────────────────────────────────┘
```

## File Structure

```
src/client/
├── index.html                    # Entry point
├── package.json                  # Dependencies
├── tsconfig.json                 # TypeScript config
├── vite.config.ts                # Vite build config
└── src/
    ├── main.ts                   # App initialization
    ├── api/
    │   └── AnodeClient.ts        # HTTP client
    ├── graph/
    │   ├── nodeTypes.ts          # Node registration from catalog
    │   ├── conversion.ts         # Anode <-> LiteGraph format
    │   ├── colors.ts             # Port colors by type
    │   ├── history.ts            # Undo/redo system
    │   └── shortcuts.ts          # Keyboard shortcuts
    ├── ui/
    │   ├── dataframe.ts          # AG Grid DataFrame viewer
    │   ├── modals.ts             # Save/Load dialogs
    │   └── toolbar.ts            # Status bar
    ├── types/
    │   └── litegraph.ts          # Extended LiteGraph types
    └── styles/
        └── main.css              # All CSS styles
```

## Usage

### Starting the Editor

1. Start the C++ backend:
```bash
./build/anodeServer -d examples/customers-500000.csv
```

2. Start the frontend dev server:
```bash
cd src/client
npm run dev
```

3. Open http://localhost:3000

### Creating a Graph

1. **Right-click** on the canvas to open the node menu
2. Select a node type (organized by category)
3. **Drag** from output ports to input ports to connect nodes
4. **Edit properties** via widgets on the nodes

### Toolbar Actions

| Button | Action |
|--------|--------|
| **New** | Clear the canvas and start a new graph |
| **Save** | Save the graph (prompts for slug/name if new) |
| **Load** | Open a modal to load a saved graph |
| **Execute** | Run the graph on the backend and display results |

### Graph Link Badges

When a graph has `timeline_output` nodes with event connections, clickable badges appear after the graph name in the toolbar:

- **`→ target`** (orange) — outgoing link: this graph triggers `target` via an event
- **`← source`** (green) — incoming link: `source` graph triggers this one

Links are auto-detected at save time and stored in the `graph_links` table. Clicking a badge navigates to the linked graph.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+G` | **Group** - Create a visual group around selected nodes |
| `Ctrl+Shift+G` | **Subgraph** - Convert selected nodes to a subgraph |
| `Ctrl+C` | Copy selected nodes |
| `Ctrl+V` | Paste nodes |
| `Ctrl+X` | Cut selected nodes |
| `Ctrl+D` | Duplicate selected nodes |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+A` | Select all nodes |
| `Delete` | Delete selected nodes/groups |
| `Backspace` | Delete selected, or exit subgraph if nothing selected |
| `Escape` | Deselect all |
| `Alt+Drag` | Clone a node |
| `Shift+Click` | Break a link |

## Features

### Groups (Ctrl+G)

Select multiple nodes and press `Ctrl+G` to create a visual group around them.

- Groups can be moved (moves all contained nodes)
- Groups can be resized
- Groups can be deleted (nodes remain)
- Groups are persisted with their position, size, color, and title
- Right-click on group for options

### Subgraphs (Ctrl+Shift+G)

Subgraphs allow you to encapsulate a set of nodes into a single reusable node.

**Creating a subgraph:**
1. Select multiple nodes
2. Press `Ctrl+Shift+G`
3. The selected nodes are replaced by a single subgraph node (teal color with 📦 icon)

**Navigating subgraphs:**
- **Enter**: Double-click on a subgraph node, or click the button in its title bar
- **Exit**: Press `Backspace` (with nothing selected), or click "Back" in the breadcrumb
- **Breadcrumb**: Shows current location (Root / SubgraphName) when inside a subgraph

**Visual distinction:**
- Subgraph nodes have a teal color scheme
- Title is prefixed with 📦 emoji
- Inputs/outputs are automatically created based on the encapsulated nodes' connections

```
┌─────────────────────────────────────────────────────────────┐
│ ← Back / Root / My Subgraph                                 │  ← Breadcrumb
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌──────────────┐      ┌──────────────┐                   │
│   │ 📦 My Subgraph│      │  Other Node  │                   │
│   │ (teal color) │──────│              │                   │
│   └──────────────┘      └──────────────┘                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Reroutes

Reroutes are visual points that organize link paths.

- Handled natively by @comfyorg/litegraph
- Double-click on a link to add a reroute point
- Drag reroutes to organize complex connections

### Native Copy/Paste

Uses the built-in clipboard system from @comfyorg/litegraph:
- Copies nodes, connections, and groups
- Preserves widget values
- Pastes at cursor position

### Port Colors

Ports are colored by data type:

| Type | Color |
|------|-------|
| `int`, `double`, `string`, `bool` | Green (#4CAF50) |
| `csv` | Orange (#FF9800) |
| `field` | Violet (#9C27B0) |

Input ports are gray by default and take the color of connected outputs.

## Architecture

### Technology Stack

- **@comfyorg/litegraph** - Graph rendering (ComfyUI fork)
- **TypeScript** - Type safety
- **Vite** - Build tool and dev server
- **AG Grid** - DataFrame visualization

### Dynamic Node Registration

Node types are fetched from the backend and registered dynamically:

```typescript
// src/graph/nodeTypes.ts
export function registerNodeTypes(catalog: NodeCatalogResponse): void {
  for (const def of catalog.nodes) {
    const nodeType = `${def.category}/${def.name}`;
    const nodeClass = createNodeClass(def);
    LiteGraph.registerNodeType(nodeType, nodeClass);
  }
}

function createNodeClass(def: NodeDef): typeof LGraphNode {
  class CustomNode extends LGraphNode {
    constructor() {
      super(def.name);
      // Add inputs/outputs from definition
      for (const inp of def.inputs) {
        this.addInput(inp.name, inp.types.join(','));
      }
      for (const out of def.outputs) {
        this.addOutput(out.name, out.types.join(','));
      }
      // Add widgets
      addWidgets(this, def.name);
    }
  }
  return CustomNode;
}
```

### Format Conversion

The editor converts between LiteGraph's internal format and the Anode JSON format.

**Anode format** (stored on server):
```json
{
  "nodes": [
    {
      "id": "node_1",
      "type": "scalar/int_value",
      "properties": {"_value": {"value": 42, "type": "int"}},
      "position": [100, 200]
    }
  ],
  "connections": [
    {"from": "node_1", "fromPort": "value", "to": "node_2", "toPort": "src"}
  ]
}
```

**Conversion functions** (`src/graph/conversion.ts`):
```typescript
// Load graph from server into LiteGraph
anodeToLitegraph(anodeGraph, lgraph)

// Export LiteGraph to server format
litegraphToAnode(lgraph) → AnodeGraph
```

### Undo/Redo System

The history system (`src/graph/history.ts`) saves snapshots on:
- Node added/removed
- Connections changed
- Node moved (debounced)

```typescript
// Hooks on graph operations
graph.add = function(node) {
  origAdd(node);
  saveSnapshot();
};

graph.onConnectionChange = function() {
  updateInputColors(graph, graphCanvas);
  saveSnapshot();
};
```

## Widgets

Nodes with editable properties display widgets:

| Node | Widget | Property |
|------|--------|----------|
| `int_value` | Number input | `_value` |
| `double_value` | Number input | `_value` |
| `string_value` | Text input | `_value` |
| `bool_value` | Toggle | `_value` |
| `field` | Text input | `_column` |
| `group` | Combo + buttons | `_aggregation`, dynamic fields |

### Widget Inputs

Widgets can receive values from connections. Each widget has an associated input slot that allows its value to be set by another node:

```
┌──────────────┐         ┌──────────────┐
│  int_value   │         │  int_value   │
│              │         │              │
│ [_value: 42] ├────────►● _value [   ] │  ← Widget disabled when connected
│              │  value  │              │
└──────────────┘         └──────────────┘
```

- When an input is connected, the widget is automatically disabled
- The connected value overrides the widget value during execution
- Disconnect the input to edit the widget manually again

## Real-Time Execution Feedback

When executing a graph, you get real-time visual feedback for each node:

### Status Indicators

Each node displays a colored circle in the top-left corner of its title bar:

| Color | Status | Description |
|-------|--------|-------------|
| Gray | Idle | Node hasn't been executed yet |
| Yellow | Running | Node is currently executing |
| Green | Success | Node completed successfully |
| Red | Error | Node encountered an error |

### Execution Time

After a node completes (success or error), its execution time is displayed in the top-right corner of the title bar (e.g., `42ms` or `1.2s`).

### Progressive View Buttons

"View CSV" buttons appear immediately when each node finishes, not after the entire graph completes. This allows you to start exploring results while other nodes are still executing.

### Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    STREAMING EXECUTION FLOW                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Frontend                           Backend                             │
│  ────────                           ───────                             │
│                                                                         │
│  executeGraph() ──────────────────► POST /execute-stream (SSE)          │
│       │                                 │                               │
│       │  ◄─────────────────────────── event: execution_start            │
│       │                                 │                               │
│       │  ◄─────────────────────────── event: node_started (node_1)      │
│       │  [Show yellow indicator]        │                               │
│       │                                 ├─► Execute node_1              │
│       │  ◄─────────────────────────── event: node_completed (node_1)    │
│       │  [Show green + time + View]     │                               │
│       │                                 │                               │
│       │  ◄─────────────────────────── event: node_started (node_2)      │
│       │  [Show yellow indicator]        │                               │
│       │                                 ├─► Execute node_2              │
│       │  ◄─────────────────────────── event: node_completed (node_2)    │
│       │  [Show green + time + View]     │                               │
│       │                                 │                               │
│       │  ◄─────────────────────────── event: execution_complete         │
│       ▼                                 │                               │
│  [All nodes show final status]          │                               │
│                                                                         │
│  onClick "View" ─────────────────► handleSessionDataFrame()             │
│                                        │                               │
│  ◄──────────────────────────────── Return columnar data                │
│                                                                         │
│  renderGridInContainer(AG Grid)                                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## DataFrame Visualization

After executing a graph, nodes with CSV outputs display a "View" button widget.

### Adaptive Layout

The DataFrame viewer adapts based on screen aspect ratio (`src/ui/dataframe.ts`):

**Ultra-Wide Mode (≥ 21:9)**: Split panel with resizable divider
```
┌────────────────────────────────────────┬──────────────────┐
│                                        │◄── Resizer       │
│           Graph Canvas (2/3)           │   AG Grid (1/3)  │
│                                        │                  │
│                                        │  [Prev] 1/10 [X] │
└────────────────────────────────────────┴──────────────────┘
```

**Standard Mode (< 21:9)**: Fullscreen modal overlay
```
┌─────────────────────────────────────────────────────────┐
│                                                         │
│                      AG Grid                            │
│                   (full screen)                         │
│                                                         │
│              [Prev] Page 1 of 10 [Next] [X]             │
└─────────────────────────────────────────────────────────┘
```

### Tree Data Mode (AG Grid Enterprise)

When a DataFrame contains a `__tree_path` column (produced by the `tree_group` node), the viewer automatically switches to tree mode:

- All data is loaded client-side (no pagination)
- AG Grid Enterprise enables `treeData: true` with `getDataPath` parsing the JSON from `__tree_path`
- A "Hierarchy" column with expand/collapse arrows is added on the left
- Parent rows are automatically generated by AG Grid with aggregated values
- The `__tree_path` and `__tree_agg` columns are hidden from the display

This mode works in the graph editor (`dataframe.ts`) and in the viewer (`viewer.html`).

## API Integration

The editor uses these endpoints:

| Endpoint | Usage |
|----------|-------|
| `GET /api/nodes` | Fetch node catalog on startup |
| `GET /api/graphs` | List saved graphs for Load modal |
| `GET /api/graph/:slug` | Load a specific graph |
| `POST /api/graph` | Create new graph |
| `PUT /api/graph/:slug` | Update existing graph |
| `POST /api/graph/:slug/execute` | Execute and get results (blocking) |
| `POST /api/graph/:slug/execute-stream` | Execute with real-time SSE feedback |
| `POST /api/session/:id/dataframe/:node/:port` | Query DataFrame |

## TypeScript Types

Key types from `src/types/litegraph.ts`:

```typescript
// Extended node with Anode properties
interface AnodeLGraphNode extends LGraphNode {
  _anodeId?: string;
  _visibleFieldCount?: number;
  restoreFieldInputs?: (count: number) => void;
}

// Port colors
interface ColoredOutputSlot extends INodeOutputSlot {
  color_on?: string;
  color_off?: string;
}
```

Key types from `src/api/AnodeClient.ts`:

```typescript
interface AnodeGraph {
  nodes: AnodeNode[];
  connections: AnodeConnection[];
}

interface AnodeNode {
  id: string;
  type: string;
  properties: Record<string, { value: unknown; type: string }>;
  position?: [number, number];
}

interface AnodeConnection {
  from: string;
  fromPort: string;
  to: string;
  toPort: string;
}
```

## Building for Production

```bash
cd src/client

# Build
npm run build

# Output in dist/
# - index.html
# - assets/main-*.js
# - assets/main-*.css
```

Serve `dist/` from your web server or integrate with the C++ backend.

## Configuration

LiteGraph settings in `src/main.ts`:

```typescript
import { LiteGraph } from '@comfyorg/litegraph';

// Enable alt+drag to clone nodes
LiteGraph.alt_drag_do_clone_nodes = true;

// Show menu when releasing link on empty space
LiteGraph.release_link_on_empty_shows_menu = true;

// Shift+click to break links
LiteGraph.shift_click_do_break_link_from = true;
```

## Dependencies

- `@comfyorg/litegraph` ^0.17.2 - Graph rendering (ComfyUI fork)
- `ag-grid-community` ^32.3.3 - DataFrame grid
- `vite` ^5.0.0 - Build tool
- `typescript` ^5.3.0 - Type safety
