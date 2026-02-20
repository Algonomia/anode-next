# Scalar Nodes

This document describes the scalar value nodes for AnodeServer.

## Runtime Value Override

All scalar nodes support an optional `_identifier` property that enables overriding their `_value` at execution time via the API. This is useful for parameterizing graphs without modifying them.

### API Usage

```bash
POST /api/graph/{slug}/execute
Content-Type: application/json

{
  "inputs": {
    "my_param": 42,
    "rate": 0.15,
    "prefix": "report_"
  }
}
```

Each key in `inputs` must match an `_identifier` defined in a scalar node. The value must have a compatible type:

| Node Type | Expected Input Type |
|-----------|---------------------|
| `int_value` | integer |
| `double_value` | number (int or float) |
| `string_value` | string |
| `bool_value` | boolean |
| `date_value` | string (date format) |
| `string_as_field` | string (field name) |
| `string_as_fields` | JSON array of strings (e.g., `["col_a", "col_b"]`) |
| `null_value` | null |
| `current_date` | integer (for offsets) |
| `csv_value` | CSV object (`{columns, schema, data}`) |

### Error Handling

- **Duplicate identifier**: Returns error if the same `_identifier` is used in multiple nodes
- **Unknown identifier**: Returns error if an input key doesn't match any node's `_identifier`
- **Type mismatch**: Returns error if the input value type doesn't match the expected type

### Example

Graph with an `int_value` node:
- `_value` = 10 (default)
- `_identifier` = "threshold"

Execute without override:
```bash
curl -X POST http://localhost:8080/api/graph/test/execute
# Node outputs: 10
```

Execute with override:
```bash
curl -X POST http://localhost:8080/api/graph/test/execute \
  -H "Content-Type: application/json" \
  -d '{"inputs": {"threshold": 42}}'
# Node outputs: 42
```

## Viewer Parameters Panel

The Graph Viewer (`examples/viewer.html`) provides a GUI for editing scalar nodes with `_identifier`. This allows end-users to modify parameter values without accessing the graph editor.

### How it works

1. Open the viewer for your graph
2. Click the "Parameters" button on the right side to open the panel
3. All scalar nodes with a non-empty `_identifier` are listed
4. Edit values directly - changes are auto-saved (500ms debounce)
5. Execute the graph to use the new values

### Supported input types

| Node Type | Input Control |
|-----------|---------------|
| `int_value` | `<input type="number" step="1">` |
| `double_value` | `<input type="number" step="any">` |
| `string_value` | `<input type="text">` |
| `bool_value` | Toggle switch |
| `date_value` | `<input type="text">` |
| `string_as_field` | `<input type="text">` |
| `string_as_fields` | Dynamic list with +/- buttons (one text input per field) |
| `csv_value` | CSV file upload + preview table |

### Visual feedback

- **Yellow border**: Saving in progress
- **Green border**: Successfully saved
- **Red border**: Save failed (with error message)

The panel state (open/closed) is persisted in localStorage.

## File Structure

```
src/nodes/nodes/
└── ScalarNodes.hpp/cpp    # All scalar node implementations
```

## Nodes

### int_value

Outputs a configurable integer value.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | int | 0 | Integer value to output |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Int | The configured integer |

### double_value

Outputs a configurable double value.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | double | 0.0 | Double value to output |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Double | The configured double |

### string_value

Outputs a configurable string value.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | string | "" | String value to output |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | String | The configured string |

### bool_value

Outputs a configurable boolean value.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | bool | false | Boolean value to output |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Bool | The configured boolean |

### null_value

Outputs a null value.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Null | Null value |

### string_as_field

Interprets a string as a field name (column reference).

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | string | "" | The field/column name |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Field | Field reference to use with CSV data |

**Use case**: When you need to dynamically specify a column name rather than hardcoding it.

### string_as_fields

Outputs multiple field references from a single node. The field names are stored as a JSON array in `_value`. The `NodeExecutor` automatically expands the outputs into numbered inputs on the target node (e.g., `field`, `field_1`, `field_2`, ...) when connected to a variable-input node like `tree_group` or `group`.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | string | `"[]"` | JSON array of field names (e.g., `'["col_a","col_b","col_c"]'`) |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Field | First field reference |
| `value_1`, `value_2`, ... | Field | Additional field references (generated dynamically from `_value` array) |

**Expansion mechanism**: When `string_as_fields` is connected to a target port (e.g., `tree_group.field`), the `NodeExecutor::gatherInputs()` automatically maps:
- `value` → `field` (target port name)
- `value_1` → `field_1`
- `value_2` → `field_2`
- etc.

This means the target node receives multiple field inputs without any modification to its own implementation.

**API override**: The caller can send a native JSON array:
```bash
curl -X POST http://localhost:8080/api/graph/test/execute \
  -H "Content-Type: application/json" \
  -d '{"inputs": {"my_fields": ["col_a", "col_b", "col_c"]}}'
```

**Use case**: When a script caller needs to dynamically control how many fields a variable-input node receives. Instead of hardcoding N `string_as_field` nodes in the graph, a single `string_as_fields` node allows the caller to set the field list at runtime.

**Example**:
```
string_as_fields(_value: '["country","city"]', _identifier: "hierarchy")
    → connected to tree_group.field
    → tree_group sees: field="country", field_1="city"

API override with 3 fields:
    {"inputs": {"hierarchy": ["region", "country", "city"]}}
    → tree_group sees: field="region", field_1="country", field_2="city"
```

### date_value

Converts a date string to a Unix timestamp (seconds since epoch).

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | string | "" | Date string to parse |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `value` | Int | Unix timestamp (seconds) |

**Supported date formats**:
- `dd/mm/yyyy` - e.g., `14/04/1990`
- `dd/mm/yy` - e.g., `14/04/90` (years < 70 → 2000s, ≥ 70 → 1900s)
- `dd month yyyy` - e.g., `14 avril 1990` or `14 april 1990`

**Supported month names** (case insensitive):
| French | English |
|--------|---------|
| janvier | january |
| février, fevrier | february |
| mars | march |
| avril | april |
| mai | may |
| juin | june |
| juillet | july |
| août, aout | august |
| septembre | september |
| octobre | october |
| novembre | november |
| décembre, decembre | december |

**Example**:
```
_value = "14/04/1990"  →  value = 640051200
_value = "14 avril 1990"  →  value = 640051200
```

### current_date

Returns the current date with optional year/month/day offsets.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_year_offset` | int | 0 | Years to add/subtract |
| `_month_offset` | int | 0 | Months to add/subtract |
| `_day_offset` | int | 0 | Days to add/subtract |
| `_identifier` | string | "" | Optional identifier for runtime override (overrides offsets) |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `year` | Int | Calculated year (e.g., 2026) |
| `month` | Int | Calculated month (1-12) |
| `day` | Int | Calculated day (1-31) |

**Note**: Offsets handle overflow automatically. For example, -1 month in January gives December of the previous year.

**Example**:
```
Today: 2026-02-02
_year_offset = -1, _month_offset = 0, _day_offset = 0
→ year = 2025, month = 2, day = 2
```

### scalars_to_csv

Combines multiple field/value pairs into a single-row CSV (DataFrame).

**Category**: `scalar`

**Inputs**:
| Port | Type | Description |
|------|------|-------------|
| `field` | Field \| String | First column name |
| `value` | Int \| Double \| String \| Bool \| Null | First column value |

**Properties** (for additional columns):
| Property | Type | Description |
|----------|------|-------------|
| `_field_0`, `_field_1`, ... | string | Additional column names |
| `_value_0`, `_value_1`, ... | any | Additional column values |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `csv` | Csv | DataFrame with 1 row containing all field/value pairs |

**Example**:
```
field = "name", value = "Alice"
_field_0 = "age", _value_0 = 30
_field_1 = "active", _value_1 = true

→ CSV with columns: name, age, active
   Row 0: "Alice", 30, 1
```

### csv_value

Outputs a configurable CSV DataFrame. The CSV data is stored directly in the graph and persists between runs. An AG Grid-based editor in the graph editor allows creating and editing the data inline.

**Category**: `scalar`

**Properties**:
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `_value` | Csv | empty DataFrame | CSV data (serialized as `{columns, schema, data}`) |
| `_identifier` | string | "" | Optional identifier for runtime override |

**Outputs**:
| Port | Type | Description |
|------|------|-------------|
| `csv` | Csv | The configured DataFrame |

**Override priority**:
1. CsvOverrides via `_identifier` (injected by NodeExecutor, skips compile)
2. `_value` property (CSV data stored in graph)
3. Fallback: empty DataFrame

**Graph editor UI**:
- "Edit CSV" button opens a modal with AG Grid (editable cells, add/remove rows and columns, import CSV file, double-click headers to rename)
- "Clear" button removes the CSV data
- Info widget displays current dimensions ("X rows × Y cols")

**Use case**: When you need a small static dataset embedded directly in the graph, with optional override capability via the viewer Parameters panel or API.

## Usage Example

```
┌─────────────────┐     ┌──────────────────┐
│   date_value    │────▶│  some_node       │
│ _value=14/04/90 │     │  timestamp input │
└─────────────────┘     └──────────────────┘

┌─────────────────┐     ┌──────────────────┐
│  current_date   │────▶│  filter_by_date  │
│ _year_offset=-1 │     │  year/month/day  │
└─────────────────┘     └──────────────────┘
```
