# TypeScript Client Library

## Overview

The `@anode/client` package provides a TypeScript client for interacting with AnodeServer's REST API. It includes a fluent query builder and full type definitions.

## File Structure

```
src/client/
├── lib/                          # Client library (npm package)
│   ├── AnodeClient.ts            # Client class and types
│   ├── LiteGraphAdapter.ts       # Legacy adapter (deprecated)
│   └── index.ts                  # Public exports
├── src/                          # Graph Editor application
│   ├── main.ts                   # App entry point
│   ├── api/
│   │   └── AnodeClient.ts        # HTTP client for graph editor
│   ├── execution/
│   │   ├── state.ts              # Per-node execution state management
│   │   ├── sse-client.ts         # SSE client for streaming execution
│   │   └── index.ts              # Module exports
│   ├── graph/
│   │   ├── nodeTypes.ts          # Node registration + status indicators
│   │   ├── conversion.ts         # Anode <-> LiteGraph conversion
│   │   ├── colors.ts             # Port colors by type
│   │   ├── history.ts            # Undo/redo
│   │   └── shortcuts.ts          # Keyboard shortcuts
│   ├── ui/
│   │   ├── dataframe.ts          # AG Grid DataFrame viewer
│   │   ├── executionHistory.ts   # Execution history modal
│   │   ├── modals.ts             # Save/Load dialogs
│   │   └── toolbar.ts            # Status bar
│   ├── types/
│   │   └── litegraph.ts          # Extended types
│   └── styles/
│       └── main.css              # All CSS
├── examples/
│   ├── index.html                # Tabulator demo
│   └── graph-editor.html         # Redirect to new editor
├── index.html                    # Graph Editor entry point
├── package.json
├── tsconfig.json
└── vite.config.ts
```

## Installation

```bash
cd src/client
npm install
```

## Building

```bash
# Build Graph Editor (outputs to dist/)
npm run build

# Build client library only
npm run build:lib

# Type check
npm run typecheck
```

## Development Server

```bash
# Start Vite dev server with hot-reload
npm run dev

# Opens http://localhost:5173
# API requests proxied to http://localhost:8080
```

## API

### AnodeClient

Main client class for API communication.

```typescript
import { AnodeClient } from '@anode/client';

const client = new AnodeClient('http://localhost:8080');

// Health check
const health = await client.health();
// { status: 'ok', service: 'AnodeServer', version: '1.0.0', dataset_loaded: true }

// Dataset info
const info = await client.datasetInfo();
// { status: 'ok', path: '...', rows: 500000, columns: [...] }

// Execute query
const result = await client.query({
  operations: [...],
  limit: 100,
  offset: 0
});
```

### QueryBuilder

Fluent API for building queries.

```typescript
import { createQuery, AnodeClient } from '@anode/client';

const client = new AnodeClient();

const result = await createQuery()
  .filter([
    { column: 'age', operator: '>=', value: 18 }
  ])
  .orderBy([
    { column: 'name', order: 'asc' }
  ])
  .limit(50)
  .offset(100)
  .execute(client);
```

### Shorthand Methods

```typescript
// Single filter condition
createQuery().where('country', '==', 'France')

// Single sort
createQuery().sortBy('created_at', 'desc')

// Select columns
createQuery().select(['id', 'name', 'email'])

// GroupBy with aggregations
createQuery().groupBy(
  ['country'],
  [
    { column: 'salary', function: 'avg', alias: 'avg_salary' },
    { column: 'id', function: 'count', alias: 'total' }
  ]
)
```

## Types

### QueryRequest

```typescript
interface QueryRequest {
  operations: Operation[];
  limit?: number;
  offset?: number;
}
```

### Operation

```typescript
interface Operation {
  type: 'filter' | 'orderby' | 'groupby' | 'select';
  params: FilterCondition[] | OrderByCondition[] | GroupByParams | string[];
}
```

### FilterCondition

```typescript
interface FilterCondition {
  column: string;
  operator: '==' | '!=' | '<' | '<=' | '>' | '>=' | 'contains';
  value: string | number;
}
```

### OrderByCondition

```typescript
interface OrderByCondition {
  column: string;
  order: 'asc' | 'desc';
}
```

### Aggregation

```typescript
interface Aggregation {
  column: string;
  function: 'count' | 'sum' | 'avg' | 'min' | 'max';
  alias: string;
}
```

### QueryResponse

```typescript
interface QueryResponse<T = Record<string, unknown>> {
  status: string;
  stats: {
    input_rows: number;
    output_rows: number;
    offset: number;
    returned_rows: number;
    duration_ms: number;
  };
  data: T[];
}
```

## Tabulator Example

The example at `examples/index.html` demonstrates:

- Server-side pagination (`paginationMode: 'remote'`)
- Dynamic column generation from dataset info
- Filter and sort controls
- Pivot table support
- Tree mode (GroupByTree) with expandable rows
- Query statistics display

### Running the Example

1. Start the C++ server:
   ```bash
   cd build
   ./anodeServer -d ../examples/customers-500000.csv
   ```

2. Start Vite dev server:
   ```bash
   cd src/client
   npm run dev
   ```

3. Open http://localhost:5173/examples/index.html

## Graph Viewer Example

The example at `examples/viewer.html` demonstrates:

- Graph output visualization with AG Grid
- Server-side pagination for large DataFrames
- Dynamic equations injection UI (see [DYNAMIC-NODES.md](../nodes/DYNAMIC-NODES.md))
- Named outputs selection
- **Parameters panel** for editing scalar node values (see [SCALAR-NODES.md](../nodes/SCALAR-NODES.md))

### Features

- **Output selector**: Choose which named output to display
- **Execute button**: Run the graph and reload outputs
- **Dynamic Equations**: Enter equations for dynamic zones and apply them to the graph
- **AG Grid**: Sort, filter, paginate results
- **Parameters panel**: Retractable side panel for editing scalar node values

### Parameters Panel

The viewer includes a retractable panel on the right side that displays all scalar nodes with a defined `_identifier` property. This allows end-users to modify parameter values without opening the graph editor.

**Supported node types**:
| Node Type | Input Control |
|-----------|---------------|
| `int_value` | Number input (step=1) |
| `double_value` | Number input (step=any) |
| `string_value` | Text input |
| `bool_value` | Toggle switch |
| `date_value` | Text input |
| `string_as_field` | Text input |
| `string_as_fields` | Dynamic list with +/- buttons |
| `csv_value` | CSV file upload + preview table |
| `csv_source` | CSV file upload + preview table |

**Features**:
- **Auto-save**: Changes are saved automatically with a 500ms debounce
- **Visual feedback**: Shows "Saving...", "Saved", or error status
- **Persistence**: Panel state (open/closed) is saved to localStorage
- **Refresh on execute**: Parameters are reloaded after graph execution

**How to expose a parameter**:
1. Add a scalar node to your graph (e.g., `int_value`)
2. Set the `_identifier` property to a meaningful name (e.g., "threshold")
3. The parameter will appear in the viewer's Parameters panel

### Running the Viewer

1. Start the C++ server with a graphs database
2. Create a graph with `dynamic_begin`/`dynamic_end` nodes
3. Open http://localhost:5173/examples/viewer.html

## Error Handling

```typescript
try {
  const result = await client.query(request);
  if (result.status !== 'ok') {
    console.error('Query failed:', result);
  }
} catch (error) {
  // Network error or server unavailable
  console.error('Request failed:', error.message);
}
```

## Node Catalog API

Get all registered node definitions for building a graph editor UI.

```typescript
const catalog = await client.getNodes();
// {
//   status: 'ok',
//   nodes: [
//     { name: 'add', category: 'math', inputs: [...], outputs: [...] },
//     ...
//   ],
//   categories: ['scalar', 'data', 'math', 'csv']
// }
```

## Graph API

CRUD operations for node graphs with version history.

```typescript
// List all graphs
const list = await client.listGraphs();

// Get a graph
const response = await client.getGraph('my-pipeline');
// { metadata: {...}, version: {...}, graph: { nodes: [...], connections: [...] } }

// Create a new graph
await client.createGraph({
  slug: 'my-pipeline',
  name: 'My Pipeline',
  graph: { nodes: [...], connections: [...] }
});

// Update (save new version)
await client.updateGraph('my-pipeline', {
  version_name: 'v2.0',
  graph: { nodes: [...], connections: [...] }
});

// Delete
await client.deleteGraph('my-pipeline');

// Execute
const results = await client.executeGraph('my-pipeline');
// { results: { node_1: { value: {...} }, ... }, duration_ms: 5 }
```

## Graph Editor

The Graph Editor is now a full TypeScript application built with Vite. See [EDITOR.md](../graph/EDITOR.md) for detailed documentation.

```bash
# Start development server
cd src/client
npm run dev

# Open http://localhost:5173
```

## Node Configuration (nodeTypes.ts)

The file `src/graph/nodeTypes.ts` handles node registration in LiteGraph and their UI configuration.

### Architecture

```
nodeTypes.ts
├── registerNodeTypes()     # Register all nodes from the API
├── addWidgetsForNode()     # Add widgets based on node type
├── setupGroupNode()        # Configure the group node (dynamic inputs)
├── setupSelectByNameNode() # Configure select_by_name (dynamic inputs)
├── setupSelectByPosNode()  # Configure select_by_pos (dynamic inputs)
└── setupJoinFlexNode()     # Configure join_flex (mode widgets)
```

### Adding Widgets to a Node

In the `addWidgetsForNode()` function, add a case for the new node:

```typescript
export function addWidgetsForNode(node: AnodeLGraphNode): void {
  const nodeName = node.constructor.name.replace('AnodeLGraphNode_', '');

  switch (nodeName) {
    case 'int_value':
      addWidgetWithInput(node, 'number', '_value', 0, 'int');
      break;

    case 'string_value':
      addWidgetWithInput(node, 'text', '_value', '', 'string');
      break;

    case 'my_new_node':
      // Simple text widget
      node.addWidget('text', '_my_property', 'default', (v) => {
        node.properties._my_property = v;
      });

      // Combo widget (dropdown list)
      node.addWidget('combo', '_mode', 'option1', (v) => {
        node.properties._mode = v;
      }, { values: ['option1', 'option2', 'option3'] });
      break;
  }
}
```

### Widgets with Connected Input

Some widgets can be replaced by an input connection:

```typescript
function addWidgetWithInput(
  node: AnodeLGraphNode,
  widgetType: 'text' | 'number' | 'combo',
  name: string,
  defaultValue: string | number,
  inputType: string
): void {
  // Add the widget
  node.addWidget(widgetType, name, defaultValue, (v) => {
    node.properties[name] = v;
  });

  // Add an input that can replace the widget
  node.addInput(name, inputType);
  const inputs = node.inputs as any[];
  if (inputs && inputs.length > 0) {
    inputs[inputs.length - 1].widget = { name };
  }
}
```

### Nodes with Dynamic Inputs (+/-)

To create a node with addable/removable inputs:

**1. Server side (C++)**: Declare numbered optional inputs

```cpp
NodeBuilder("my_node", "category")
    .input("item", Type::String)              // First mandatory input
    .inputOptional("item_1", Type::String)    // Additional inputs
    .inputOptional("item_2", Type::String)
    // ... up to item_99
```

**2. Client side**: Create a setup function

```typescript
function setupMyNode(node: AnodeLGraphNode): void {
  // Remove pre-created dynamic inputs
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('item_')) as any;
  }

  // Input counter (1 = just "item")
  node._visibleItemCount = 1;

  // + button
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleItemCount ?? 1) < 100) {
      node.addInput('item_' + node._visibleItemCount, 'string');
      node._visibleItemCount++;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // - button
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleItemCount ?? 1) > 1) {
      const slot = node.findInputSlot('item_' + (node._visibleItemCount - 1));
      if (slot !== -1) {
        node.disconnectInput(slot);
        node.removeInput(slot);
      }
      node._visibleItemCount--;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Restore method on load
  node.restoreItemInputs = function(count: number) {
    // Remove item_* inputs
    const inputs = this.inputs as any[];
    for (let i = inputs.length - 1; i >= 0; i--) {
      if (inputs[i].name.startsWith('item_')) {
        this.removeInput(i);
      }
    }
    // Recreate the correct number
    for (let i = 1; i < count; i++) {
      this.addInput('item_' + i, 'string');
    }
    this._visibleItemCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}
```

**3. Register the function** in `addWidgetsForNode()`:

```typescript
case 'my_node':
  setupMyNode(node);
  break;
```

### Existing Nodes with Special Configuration

| Node | Setup Function | Configuration |
|------|----------------|---------------|
| `group` | `setupGroupNode()` | Dynamic field_* inputs + _aggregation combo |
| `select_by_name` | `setupSelectByNameNode()` | Dynamic column_* inputs |
| `select_by_pos` | `setupSelectByPosNode()` | Dynamic col_* inputs + _default combo |
| `join_flex` | `setupJoinFlexNode()` | Combos for join modes |
| `output` | - | Text widget _name with connected input |

## Bundle Size

The library has no runtime dependencies (uses native `fetch`). Approximate sizes:
- ESM: ~10KB minified
- CJS: ~11KB minified
- Types: ~8KB

## Real-Time Execution Module

The `execution/` module provides real-time feedback during graph execution via Server-Sent Events (SSE).

### State Management (`execution/state.ts`)

Tracks execution status for each node:

```typescript
import { getNodeStatus, setNodeStatus, resetExecutionState } from './execution';

// Get node status
const state = getNodeStatus('node_1');
// { status: 'success', durationMs: 42, csvMetadata: {...} }

// Status values: 'idle' | 'running' | 'success' | 'error'
```

### SSE Client (`execution/sse-client.ts`)

Execute a graph with streaming feedback:

```typescript
import { executeGraphWithStream } from './execution';

executeGraphWithStream('http://localhost:8080', 'my-pipeline', {
  onStart: (sessionId, nodeCount) => {
    console.log(`Starting execution of ${nodeCount} nodes`);
  },
  onNodeStarted: (nodeId) => {
    console.log(`Node ${nodeId} started`);
  },
  onNodeCompleted: (nodeId, durationMs, csvMeta) => {
    console.log(`Node ${nodeId} completed in ${durationMs}ms`);
  },
  onNodeFailed: (nodeId, durationMs, error) => {
    console.error(`Node ${nodeId} failed: ${error}`);
  },
  onComplete: (sessionId, hasErrors) => {
    console.log(`Execution complete, hasErrors: ${hasErrors}`);
  },
  onError: (error) => {
    console.error(`Execution error: ${error}`);
  }
});
```

### Visual Indicators

Nodes automatically display status indicators when using the execution module:

| Color | Status | Description |
|-------|--------|-------------|
| Gray | `idle` | Not yet executed |
| Yellow | `running` | Currently executing |
| Green | `success` | Completed successfully |
| Red | `error` | Failed with error |

Execution time is displayed in the top-right corner of each node after completion.

## Execution History

The Graph Editor supports persistent execution history. After executing a graph:

1. Results are stored in SQLite (not just RAM)
2. DataFrames survive server restarts
3. Results can be shared between browser tabs

### Using History

1. Execute a graph with the **Execute** button
2. Click **History** to see past executions
3. Click **Restore** on any execution to reload its results
4. Use the **View** buttons to see DataFrames

### API Methods

```typescript
// List past executions
const { executions } = await client.listExecutions('my-pipeline');

// Get execution details
const { execution, csv_metadata } = await client.getExecution(5);

// Restore an execution (reloads DataFrames to session)
const { session_id, csv_metadata } = await client.restoreExecution(5);

// Query DataFrame from restored session
const { columns, data } = await client.querySessionDataFrame(
  session_id,
  'node_1',
  'csv',
  { limit: 100 }
);
```

### Automatic Cleanup

The server automatically keeps only the **10 most recent executions** per graph to save disk space.
