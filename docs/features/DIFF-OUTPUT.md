# Diff Output (Side-by-Side CSV Comparison)

## Context

The `diff_output` node compares two DataFrames (before/after) and produces a side-by-side visualization in the viewer with coloring of modified rows and cells.

## Principle

```
csv_source ──left──┐
                   │
                   ├──► diff_output ──► viewer: two grids side by side
                   │     _diff_name:      (Before | After)
csv_source ──right─┘     "my_diff"
```

The node produces a **single annotated DataFrame** containing all the information for both panels:

| Column | Type | Description |
|--------|------|-------------|
| `__diff__` | String | `"added"` / `"removed"` / `"modified"` / `"unchanged"` |
| `col1`, `col2`, ... | original types | Values from the **right** CSV (after) — empty for removed rows |
| `__old_col1`, `__old_col2`, ... | original types | Values from the **left** CSV (before) — empty for added rows |
| `__changed_col1`, `__changed_col2`, ... | Int (0/1) | 1 if the cell differs in modified rows |

Row order is: removed, modified, added, unchanged.

## Matching algorithm

### Positional mode (default, without key)

Rows are compared by position (row i on the left vs row i on the right). Excess rows on the longer side are marked as added or removed.

```
Left (3 rows)       Right (4 rows)
row 0  <--->  row 0    (compare)
row 1  <--->  row 1    (compare)
row 2  <--->  row 2    (compare)
              row 3    (added)
```

### Key mode (with key connected)

Rows are matched by the value of a key column (hash map). Unmatched rows on the left side are "removed", on the right side are "added".

```
Left (key=id)     Right (key=id)
id=1  <--->  id=1    (compare)
id=2                  (removed)
             id=3    (added)
id=4  <--->  id=4    (compare)
```

## Node configuration

### Inputs

| Input | Type | Required | Description |
|-------|------|----------|-------------|
| `left` | Csv | yes | "Before" DataFrame |
| `right` | Csv | yes | "After" DataFrame |
| `key` | Field | no | Column for key-based matching |

### Properties

| Property | Widget | Default | Description |
|----------|--------|---------|-------------|
| `_diff_name` | text | `my_diff` | Name of the diff tab in the viewer |

### Outputs

| Output | Type | Description |
|--------|------|-------------|
| `csv` | Csv | Annotated DataFrame with `__diff__`, `__old_*`, `__changed_*` columns |
| `output_name` | String | Diff name |
| `output_type` | String | `"diff"` |
| `output_metadata` | String | JSON with stats and column info |

### Generated metadata

```json
{
  "left_columns": ["id", "name", "value"],
  "right_columns": ["id", "name", "value", "new_col"],
  "all_columns": ["id", "name", "value", "new_col"],
  "key_column": "id",
  "stats": {
    "added": 2,
    "removed": 1,
    "modified": 3,
    "unchanged": 10
  }
}
```

## Frontend (viewer.html)

### Display

The viewer shows a dedicated section between the timeline and the main grid:

```
┌──────────────────────────────────────────────┐
│  [my_diff]              +2 -1 ~3 10 unchanged│  <- tabs + stats
├──────────────────────────────────────────────┤
│  [x] Show unchanged rows                    │  <- toggle
├──────────────────────┬───────────────────────┤
│  Before (left)       │  After (right)        │
├──────────────────────┼───────────────────────┤
│  AG Grid (old vals)  │  AG Grid (new vals)   │
│                      │                       │
│  red = removed       │  green = added        │
│  yellow = modified   │  yellow = modified    │
│                      │  orange = cell changed│
└──────────────────────┴───────────────────────┘
```

### Coloring

| Status | Row color | Description |
|--------|-----------|-------------|
| `added` | green (`#d4edda`) | Row present only on the right |
| `removed` | red (`#f8d7da`) | Row present only on the left |
| `modified` | yellow (`#fff3cd`) | Row present on both sides with differences |
| `unchanged` | (none) | Identical rows |

Individually modified cells in `modified` rows are highlighted in orange (`#ffc107`, bold) in the right grid.

### Features

- **Synchronized scroll**: both grids scroll together (vertical and horizontal)
- **Synchronized resize**: resizing a column resizes it in both grids
- **Toggle unchanged**: checkbox to hide/show unchanged rows
- **Stats bar**: displays the number of rows per status

## Usage example

### Comparing before/after a transformation

```
csv_source ──────────────────────────left──┐
  _identifier: "data"                      │
                                           ├──► diff_output
csv_source ──► transform ──────────right──┘     _diff_name: "changes"
  _identifier: "data"
```

### Comparing two sources with a key

```
csv_source ──────left──┐
                       ├──► diff_output
csv_source ──────right─┤     _diff_name: "compare"
                       │
field ───────────key───┘
  _column: "id"
```

## Related files

| File | Role |
|------|------|
| `src/nodes/nodes/VizNodes.hpp` | `registerDiffOutputNode()` declaration |
| `src/nodes/nodes/VizNodes.cpp` | Diff algorithm, annotated DataFrame construction |
| `src/server/RequestHandler.cpp` | output_name/type/metadata extraction for `viz/diff_output` |
| `src/client/src/graph/nodeTypes.ts` | `_diff_name` widget |
| `src/client/examples/viewer.html` | Diff section, two AG Grids, scroll sync, coloring |

## See also

- [CATALOG.md](../nodes/CATALOG.md) — `diff_output` node reference
- [EVENTS.md](EVENTS.md) — Similar feature: `timeline_output` with drill-down
