# Node Catalog

Reference of available nodes, organized by category.

---

## Scalar

| Node | Properties | Outputs | Description |
|------|------------|---------|-------------|
| `int_value` | `_value` | value(Int) | Configurable integer |
| `double_value` | `_value` | value(Double) | Configurable float |
| `string_value` | `_value` | value(String) | Configurable string |
| `bool_value` | `_value` | value(Bool) | Configurable boolean |
| `null_value` | - | value(Null) | Null value |
| `string_as_field` | `_value` | value(Field) | String as column reference |
| `string_as_fields` | `_value` (JSON array) | value, value_1..99(Field) | List of column references (+/- widgets). Automatic expansion to nodes with dynamic inputs |
| `date_value` | `_value` | value(String) | Configurable date |
| `current_date` | - | value(String) | Current date |
| `scalars_to_csv` | - | csv(Csv) | Converts scalars to CSV |
| `csv_value` | `_value` | csv(Csv) | Configurable DataFrame (editable in the editor) |

See [SCALAR-NODES.md](SCALAR-NODES.md) for details on parameter override support.

---

## CSV / Data

| Node | Inputs | Outputs | Description |
|------|--------|---------|-------------|
| `csv_source` | csv? | csv(Csv) | DataFrame source (with identifier/override support) |
| `field` | csv, `_column` | field(Field), csv(Csv) | Column reference |
| `output` | csv, `_name` | csv(Csv), output_name(String) | Publishes a named DataFrame |
| `join_flex` | left_csv, right_csv, left_field, right_field | csv_no_match, csv_single_match, csv_multiple_match | Flexible join |

### Output Node

The `output` node "publishes" a DataFrame with a name for API access.

```bash
# List named outputs
GET /api/graph/:slug/outputs

# Retrieve a DataFrame by name
POST /api/graph/:slug/output/:name
```

---

## Math

All math nodes share the same signature:

| Node | Inputs | Outputs | Description |
|------|--------|---------|-------------|
| `add` | csv?, src, dest?, operand | csv(Csv), result(Double) | Addition |
| `subtract` | csv?, src, dest?, operand | csv(Csv), result(Double) | Subtraction |
| `multiply` | csv?, src, dest?, operand | csv(Csv), result(Double) | Multiplication |
| `divide` | csv?, src, dest?, operand | csv(Csv), result(Double) | Division |
| `modulus` | csv?, src, dest?, operand | csv(Csv), result(Double) | Modulo |

**Input types**: `src` and `operand` accept Int/Double/Field.

**`dest` behavior**:
- `dest` specified -> result written to that column
- `dest` absent and `src` is Field -> overwrites the source column
- `dest` absent and `src` is scalar -> generates `_<op>_result`

---

## Aggregate

| Node | Inputs | Properties | Outputs | Description |
|------|--------|------------|---------|-------------|
| `group` | csv, field, field_1..99 | `_aggregation` | csv(Csv) | GroupBy with aggregation |
| `tree_group` | csv, field, field_1..99 | `_aggregation` | csv(Csv) | Enriches the CSV for AG Grid tree display |

**Aggregation functions**: `sum`, `avg`, `min`, `max`, `first`, `count`

Dynamic inputs (+/-) to add grouping columns.

### tree_group

Adds metadata columns to enable tree display in AG Grid Enterprise Tree Data. Connected fields define the hierarchy (from most general to most specific).

**Columns added to the CSV:**
- `__tree_path`: hierarchical path as a JSON array (e.g., `["France","Paris"]`)
- `__tree_agg`: aggregation function to apply (e.g., `"sum"`)

AG Grid Enterprise automatically generates parent rows with aggregated values and displays expand/collapse arrows. The viewer (graph editor and viewer.html) detects `__tree_path` in columns and enables tree mode.

**Example:**
```
csv_source -> field("country") + field("city") -> tree_group(_aggregation: sum) -> output
```
Each row receives a `__tree_path` like `["France","Paris"]`. AG Grid creates a parent row "France" with the sum of numeric columns from its children.

---

## Select

| Node | Inputs | Properties | Outputs | Description |
|------|--------|------------|---------|-------------|
| `select_by_name` | csv, column, column_1..99 | - | csv(Csv) | Keeps columns by name |
| `select_by_pos` | csv, col_0..99 | `_default` | csv(Csv) | Keeps/removes by position |
| `clean_tmp_columns` | csv | - | csv(Csv) | Removes `_tmp_*` columns |
| `remap_by_name` | csv, col/dest pairs (+/-) | `_unmapped` | csv(Csv) | Renames columns by pairs. Widget: keep (default) / remove unmapped |
| `remap_by_csv` | csv, mapping(Csv), col(Field), dest(Field) | `_unmapped` | csv(Csv) | Renames columns via a mapping CSV. Widget: keep (default) / remove unmapped |

---

## String

All string nodes share the same dual signature (scalar/vector):

| Node | Inputs | Outputs | Description |
|------|--------|---------|-------------|
| `add_column` | csv?, value(String\|Field), dest(Field) | csv(Csv), result(String) | Adds/sets a column with the given value |
| `json_extract` | csv?, src(String\|Field), key(String\|Field), dest?(Field) | csv(Csv), result(String) | Extracts a value from a JSON key. Widget `_on_failure`: identity (default) / blank |
| `trim` | csv?, src(String\|Field), dest?(Field) | csv(Csv), result(String) | Removes leading/trailing whitespace |
| `to_lower` | csv?, src(String\|Field), dest?(Field) | csv(Csv), result(String) | Converts to lowercase |
| `to_upper` | csv?, src(String\|Field), dest?(Field) | csv(Csv), result(String) | Converts to uppercase |
| `replace` | csv?, src(String\|Field), dest?(Field), search, by | csv(Csv), result(String) | Replaces the first occurrence |
| `to_integer` | csv?, src(String\|Field), dest?(Field) | csv(Csv), result(Int) | Converts to integer |
| `substring` | csv?, src(String\|Field), dest?(Field), start, end | csv(Csv), result(String) | Extracts a substring |
| `split` | csv?, src(String\|Field), dest?(Field), delimiter, position | csv(Csv), result(String) | Splits and extracts an element |
| `unidecode` | csv?, src(String\|Field), dest?(Field) | csv(Csv), result(String) | Removes accents |
| `trim_integer` | csv?, src(String\|Field), dest?(Field) | csv(Csv), result(String) | Trim + removes leading zeros |
| `concat` | csv?, src(String\|Field), dest?(Field), suffix_0..N | csv(Csv), result(String) | Concatenates suffixes |
| `concat_prefix` | csv?, src(String\|Field), dest?(Field), prefix_0..N | csv(Csv), result(String) | Concatenates prefixes |

---

## Perimeter

| Node | Inputs | Outputs | PostgreSQL Function |
|------|--------|---------|---------------------|
| `identifyPelHv` | csv?, pelhv label | csv, pelhv id, pelhv label | `anode_identify_pelhv` |
| `identifyPelAt` | csv?, pelat label | csv, pelat id, pelat label | `anode_identify_pelat` |
| `identifyPevHv decomposition` | csv?, pelhv id, start?, end? | csv, pevhv id | `anode_perimeter_pevhv_pel_pev_named_composition_for_pel_id` |
| `identifyPevAt decomposition` | csv?, pelat id, start?, end? | csv, pevat id | `anode_perimeter_pevat_pel_pev_named_composition_for_pel_id` |

---

## Database

| Node | Inputs | Properties | Outputs | Description |
|------|--------|------------|---------|-------------|
| `postgres_config` | - | `_host`, `_port`, `_database`, `_user`, `_password` | connection(String) | PostgreSQL configuration |
| `postgres_query` | query? | `_query` | csv(Csv) | SQL query execution |
| `postgres_func` | csv?, function? | `_function`, `_int_N`, `_string_N`, `_double_N` | csv(Csv) | PostgreSQL function call |

---

## Process

| Node | Inputs | Outputs | PostgreSQL Function |
|------|--------|---------|---------------------|
| `identify_process` | csv?, process_labels | csv, process_id, process_label | `anode_identify_process` |
| `identify_phase` | csv?, process_ids, phase_labels | csv, process_id, phase_label, phase_id | `anode_identify_phase` |
| `get_or_clone_phase` | csv?, process_id, start, end | csv, process_id, phase_label, phase_id | `anode_get_or_clone_phase` |
| `identify_metatask` | csv?, process_ids, phase_ids, metatask_labels | csv, process_id, phase_id, metatask_label, metatask_id | `anode_identify_metatask` |
| `identify_task` | csv?, process_ids, phase_ids, metatask_ids | csv, process_id, phase_id, metatask_id, task_id, ... | `anode_identify_task` |

---

## Form

| Node | Inputs | Properties | Outputs | PostgreSQL Function |
|------|--------|------------|---------|---------------------|
| `identify_form` | csv?, form_labels | - | csv, form_id, form_label | `anode_identify_form` |
| `identify_section` | csv?, form_ids, section_labels | - | csv, form_id, section_id, section_label | `anode_identify_section` |
| `identify_question` | csv?, form_ids, section_ids, question_names | `_with_numerotation` | csv, form_id, section_id, question_number, question_name, question_id | `anode_identify_questions` |

---

## Label (Wireless Connections)

| Node | Inputs | Properties | Outputs | Description |
|------|--------|------------|---------|-------------|
| `label_define_csv` | value(Csv) | `_label` | value(Csv) | Defines a CSV label |
| `label_define_field` | value(Field) | `_label` | value(Field) | Defines a Field label |
| `label_define_int` | value(Int) | `_label` | value(Int) | Defines an Int label |
| `label_define_double` | value(Double) | `_label` | value(Double) | Defines a Double label |
| `label_define_string` | value(String) | `_label` | value(String) | Defines a String label |
| `label_ref_csv` | - | `_label` | value(Csv) | Retrieves a CSV label |
| `label_ref_field` | - | `_label` | value(Field) | Retrieves a Field label |
| `label_ref_int` | - | `_label` | value(Int) | Retrieves an Int label |
| `label_ref_double` | - | `_label` | value(Double) | Retrieves a Double label |
| `label_ref_string` | - | `_label` | value(String) | Retrieves a String label |

The `NodeExecutor` automatically adds implicit dependencies between define/ref nodes with the same `_label`.

---

## Data

| Node | Inputs | Outputs | PostgreSQL Function |
|------|--------|---------|---------------------|
| `export_data_from_tasks` | in_tasks?, in_questions?, in_datavalues?, task_ids, question_ids, metadatavalue_ids | csv, task_id, question_id, datavalue_id, datavalue_value, ... | `anode_export_datavalue_from_task` |
| `datavalue_extractor` | csv?, value, dest_id?, dest_label?, dest_value? | csv, id, label, value | JSON extraction of checkbox/ref values |

---

## History

| Node | Inputs | Outputs | PostgreSQL Function |
|------|--------|---------|---------------------|
| `get_history_value` | in_tasks?, in_questions?, in_metadatavalues?, task_ids?, question_ids?, metadatavalue_ids? | csv, r_date, r_user_id, r_type, r_question_id, r_question_title, r_value, r_is_restorable | `anode_get_history_value_for_task` |
| `get_history_snapshot` | in_questions?, task_id, question_ids, date | csv, source_type, question_id, question_title, value, dv_pev_*, metadatavalue_id, mdv_pev_* | `anode_get_history_snapshot_for_task` |

`get_history_value`: returns the complete modification history. At least one of the 3 _ids must be provided.

`get_history_snapshot`: reconstructs the questionnaire state at a given date (one row per value, excludes deletes). All 3 inputs (task_id, question_ids, date) are required.

---

## Dynamic

| Node | Inputs | Properties | Outputs | Description |
|------|--------|------------|---------|-------------|
| `dynamic_begin` | csv? | `_equation` | csv(Csv) | Dynamic equation entry point |
| `dynamic_end` | csv | - | csv(Csv) | Dynamic equation exit point |

See [DYNAMIC-NODES.md](DYNAMIC-NODES.md) for the complete equation injection system.

---

## Viz

| Node | Inputs | Properties | Outputs | Description |
|------|--------|------------|---------|-------------|
| `timeline_output` | csv, start_date(Field), name(Field), end_date?(Field), parent?(Field), color?(Field\|String), event?(Field\|String) | `_timeline_name` | csv(Csv), output_name(String), output_type(String), output_metadata(String) | Publishes a vis-timeline |
| `diff_output` | left(Csv), right(Csv), key?(Field) | `_diff_name` | csv(Csv), output_name(String), output_type(String), output_metadata(String) | Compares two CSVs side by side |
| `bar_chart_output` | csv, category?(Field), value(Field), color?(Field\|String), event?(Field\|String) | `_chart_name` | csv(Csv), output_name(String), output_type(String), output_metadata(String) | Publishes an amCharts 5 bar chart (supports tree_group) |

### timeline_output

Transforms a CSV into a timeline visualization in the viewer. CSV columns are mapped via Field inputs.

**Generated metadata** (JSON in `output_metadata`):
```json
{
  "start_date": "col_debut",
  "name": "col_label",
  "end_date": "col_fin",
  "parent": "col_groupe",
  "color": "col_couleur",
  "color_is_field": true,
  "event": "graph_cible",
  "event_is_field": false
}
```

**`event` input**: when present, clicking a timeline entry executes the target graph with the row data as overrides. See [EVENTS.md](../features/EVENTS.md).

### diff_output

Compares two DataFrames (before/after) and produces an annotated DataFrame with side-by-side diff in the viewer. See [DIFF-OUTPUT.md](../features/DIFF-OUTPUT.md).

**Inputs**:
- `left` (Csv): "before" DataFrame
- `right` (Csv): "after" DataFrame
- `key` (Field, optional): column for matching rows by key (positional if absent)

**Generated metadata** (JSON in `output_metadata`):
```json
{
  "left_columns": ["col1", "col2"],
  "right_columns": ["col1", "col2", "col3"],
  "all_columns": ["col1", "col2", "col3"],
  "key_column": "id",
  "stats": { "added": 2, "removed": 1, "modified": 3, "unchanged": 10 }
}
```

### bar_chart_output

Transforms a CSV into a bar chart (amCharts 5) in the viewer. Two modes:

**Flat mode**: `category` + `value` define the X/Y axes. External drilldown via `event`.

**Tree mode**: when the CSV contains `__tree_path` (output of `tree_group`), hierarchical drilldown is automatic. Clicking a bar zooms into children. A breadcrumb allows navigating back up. `category` is then optional.

**Inputs**:
- `csv` (Csv): source DataFrame
- `category` (Field, optional): column for the X axis. Required in flat mode, auto-detected in tree mode.
- `value` (Field, required): column for the Y axis (values)
- `color` (Field|String, optional): per-bar color (column) or fixed (hex)
- `event` (Field|String, optional): target graph slug for external drilldown

**Generated metadata** (JSON in `output_metadata`):
```json
{
  "chart_type": "bar",
  "category": "col_categorie",
  "value": "col_valeur",
  "tree_mode": true,
  "tree_agg": "sum",
  "color": "#3498db",
  "color_is_field": false
}
```

**Tree mode**: `tree_group` -> `bar_chart_output(value: Chiffre_Affaires)`. The chart detects `__tree_path`, aggregates by hierarchy level, and allows navigation via click + breadcrumb.

**`event` input**: when present (flat mode), clicking a bar executes the target graph with the row data as overrides (same pattern as timeline_output).
