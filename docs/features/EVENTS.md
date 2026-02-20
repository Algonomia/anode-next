# Events (Drill-down Timeline)

## Context

Events allow chaining graph executions from user interactions. When a user clicks on a timeline entry in the viewer, a second graph is automatically executed with the clicked row's data as parameters. The target graph's outputs (timelines, CSV) appear as new tabs in the viewer.

## Principle

```
Graph A (source)                          Graph B (target)
+-----------------+                       +------------------+
| csv_source      |                       | int_value        |
| (history)       |                       | _identifier:     |
|     |           |                       |   "question_id"  |
|     v           |                       |     |            |
| timeline_output |  -- click on row -->  |     v            |
|   event: "B"    |     {question_id: 42} | postgres_func    |
+-----------------+                       |     |            |
                                          |     v            |
                                          | timeline_output  |
                                          | output           |
                                          +------------------+
```

1. The source graph produces a timeline with an `event` pointing to the target graph
2. The user clicks on a timeline entry
3. The viewer sends **all columns** of the clicked row as `inputs` to the target graph
4. Scalar nodes in the target graph whose `_identifier` matches a column name receive the value
5. The target graph's outputs are displayed as "drill-down" tabs

## Source node configuration

### timeline_output: `event` input

The `event` input (optional, `Field|String`) defines the slug of the target graph to execute on click:

| Type | Behavior |
|------|----------|
| `String` | Same target graph for all entries (e.g., `"detail_questionnaire"`) |
| `Field` | The slug is read from a CSV column — each row can target a different graph |

```
string_value -----> timeline_output
  _value: "detail"    event (String) => all rows open "detail"

field -----> timeline_output
  _column: "target"   event (Field) => each row reads the slug from the "target" column
```

### Generated metadata

The node adds to its JSON metadata:

```json
{
  "start_date": "date_col",
  "name": "name_col",
  "event": "detail_questionnaire",
  "event_is_field": false
}
```

When `event_is_field: true`, `event` contains the column name (not the slug directly).

## Parameter passing

### Implicit mapping: columns = identifiers

The viewer sends **all columns** of the clicked row as `inputs` in the target graph's execution request:

```json
POST /api/graph/detail_questionnaire/execute
{
  "inputs": {
    "question_id": 42,
    "task_id": 7,
    "date_modif": "2026-01-15",
    "label": "Mon questionnaire"
  },
  "apply_overrides": true,
  "skip_unknown_inputs": true
}
```

The target graph declares scalar nodes with `_identifier` values matching the column names:

| Target node | `_identifier` | Receives |
|-------------|---------------|----------|
| `int_value` | `question_id` | `42` |
| `int_value` | `task_id` | `7` |
| `string_value` | `date_modif` | `"2026-01-15"` |

Columns without a matching `_identifier` in the target graph are ignored thanks to the `skip_unknown_inputs` flag.

### Static overrides

To pass constant values (identical on each click), simply add constant columns to the CSV upstream of the `timeline_output`. For example, an `add` node or a computed column that always produces the same value.

### `skip_unknown_inputs`

The `skip_unknown_inputs: true` flag in the execution request tells the server to:
- **Ignore** unknown identifiers (instead of returning an error)
- **Ignore** type incompatibilities (instead of returning an error)

This allows sending all columns without worrying about which ones the target graph actually uses.

## Frontend (viewer.html)

### Drill-down tabs

The target graph's outputs appear as new tabs with a distinct style:
- `->` prefix before the name
- Orange left border
- Vertical separator between the main tabs and the drill-down tabs

### Behavior

| Action | Result |
|--------|--------|
| Click on a timeline entry | Executes the target graph, displays its outputs as drill-down tabs |
| New click on another entry | Replaces the previous drill-down tabs |
| Re-execution of the main graph | Removes the drill-down tabs |
| Click on a main tab | Returns to the standard display (drill-down tabs remain visible) |

### Technical flow

```
1. timelineInstance.on('select', handler)
2. handleTimelineClick(itemId)
   |-- Resolves the target slug (metadata.event / metadata.event_is_field)
   |-- Builds inputs from all columns of the row
   |-- POST /api/graph/{slug}/execute { inputs, skip_unknown_inputs: true }
3. loadDrilldownOutputs(slug)
   |-- GET /api/graph/{slug}/outputs
   |-- renderDrilldownTabs()
   |-- Auto-selects the first timeline + grid tab
```

## Auto-detection of links between graphs

On each save (create + update), the server scans the graph to detect links between graphs:

1. Searches for all `viz/timeline_output` nodes
2. For each one, searches for the incoming connection on the `event` port
3. If the source node is a `scalar/string_value`, reads `_value` as the target slug
4. Stores the link `source_slug -> target_slug` in the `graph_links` table

Links are fully rebuilt on each save (delete all + re-insert).

### API

Links are included in existing responses:
- `GET /api/graph/:slug` -> `links: { outgoing: ["detail"], incoming: ["history"] }`
- `GET /api/graphs` -> `links` per graph
- `PUT /api/graph/:slug` -> `links` in the response

### UI (editor)

Clickable badges appear in the toolbar after the graph name:
- `-> detail` (orange) — target graph (outgoing)
- `<- history` (green) — calling graph (incoming)

Clicking a badge navigates to the corresponding graph.

### UI (graphs.html)

Badges also appear in the cards on the listing page.

## Related files

| File | Role |
|------|------|
| `src/nodes/nodes/VizNodes.cpp` | `event` input (Field\|String) + JSON metadata |
| `src/server/RequestHandler.cpp` | `skip_unknown_inputs` flag + link detection (`detectAndSaveLinks`) |
| `src/storage/GraphStorage.cpp` | `graph_links` table + `replaceGraphLinks`/`getOutgoingLinks`/`getIncomingLinks` methods |
| `src/client/src/ui/toolbar.ts` | `updateGraphLinks()` — clickable badges in the toolbar |
| `src/client/src/main.ts` | Calls `updateGraphLinks` on load/save |
| `src/client/examples/viewer.html` | Click handler, drill-down tabs, datasource routing |
| `src/client/examples/graphs.html` | Link badges in the cards |

## See also

- [PARAMETER-OVERRIDES.md](PARAMETER-OVERRIDES.md) — Underlying overrides mechanism
- [CATALOG.md](../nodes/CATALOG.md) — `timeline_output` node reference
