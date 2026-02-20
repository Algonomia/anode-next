# Dynamic Nodes - Equation Injection System

## Overview

Dynamic Nodes allow injecting mathematical operations into a graph via API, without manually creating nodes in the visual editor. This is useful for:

- Programmatic graph modification from external systems
- User-defined calculations at runtime
- Batch processing with different formulas

```
┌─────────────────┐                              ┌─────────────────┐
│  dynamic_begin  │ ─── (direct connection) ───► │   dynamic_end   │
│   _name="calc"  │                              │   _name="calc"  │
└─────────────────┘                              └─────────────────┘

        │ API: POST /apply-dynamic               │
        │ { "params": ["D=($A+$B)*$C"] }         │
        ▼                                        ▼

┌─────────────────┐     ┌─────────┐     ┌─────────────┐     ┌─────────────────┐
│  dynamic_begin  │ ──► │   add   │ ──► │  multiply   │ ──► │   dynamic_end   │
│   _name="calc"  │     │ A+B→tmp │     │ tmp*C→D     │     │   _name="calc"  │
└─────────────────┘     └─────────┘     └─────────────┘     └─────────────────┘
```

## File Structure

```
src/nodes/
├── EquationParser.hpp/cpp      # Equation tokenizer, parser, and reconstruction
└── nodes/
    └── DynamicNodes.hpp/cpp    # dynamic_begin, dynamic_end nodes

src/server/
├── RequestHandler.hpp/cpp      # handleApplyDynamic(), handleExecuteDynamic(),
│                               # handleGetDynamicEquations()
└── HttpSession.cpp             # Routes /apply-dynamic, /execute-dynamic,
                                # /dynamic-equations

src/client/
└── examples/viewer.html        # UI for entering equations

tests/
├── EquationParserTest.cpp      # Parser and reconstruction unit tests
└── DynamicNodesTest.cpp        # Node unit tests
```

## Dynamic Nodes

### dynamic_begin

Marks the start of an injection zone. Pass-through node (CSV in → CSV out).

```cpp
NodeBuilder("dynamic_begin", "dynamic")
    .input("csv", Type::Csv)
    .output("csv", Type::Csv)
```

**Widget:** `_name` (string) - Zone identifier

### dynamic_end

Marks the end of an injection zone. Pass-through node (CSV in → CSV out).

```cpp
NodeBuilder("dynamic_end", "dynamic")
    .input("csv", Type::Csv)
    .output("csv", Type::Csv)
```

**Widget:** `_name` (string) - Must match the corresponding `dynamic_begin`

### Usage in Graph Editor

1. Add `dynamic_begin` node, set `_name` (e.g., "calculations")
2. Add `dynamic_end` node, set same `_name`
3. Connect: `source → dynamic_begin → dynamic_end → output`

The direct connection between begin and end will be replaced by injected nodes.

## Equation Parser

### Syntax

```
DESTINATION = EXPRESSION

Where:
- DESTINATION: Column name for the result (e.g., D, total, _tmp)
  - Can contain spaces (e.g., "My Result", "Total Price")
- EXPRESSION: Mathematical expression with:
  - Field references: $column_name (e.g., $A, $price, $quantity)
    - Can contain spaces (e.g., $Unit Price, $Integration rate)
  - Numbers: integers or decimals (e.g., 42, 3.14)
  - Operators: +, -, *, / (standard precedence)
  - Parentheses: ( ) for grouping
```

### Examples

```
C = $A + $B                                    # Simple addition
D = ($A + $B) * $C                             # Parentheses override precedence
E = $D * 42                                    # Field times scalar
F = 100 + $A * 2                               # Mixed expression
Total Price = $Unit Price * $Quantity          # Spaces in names
My Result = ($First Value + $Tax) * 1.2        # Complex with spaces
```

### Operator Precedence

1. Parentheses `( )` - highest
2. Multiplication `*`, Division `/`
3. Addition `+`, Subtraction `-` - lowest

### Grammar (Recursive Descent)

```
equation   = IDENT '=' expression
expression = term (('+' | '-') term)*
term       = factor (('*' | '/') factor)*
factor     = primary | '-' factor
primary    = FIELD | NUMBER | '(' expression ')'
```

### Token Types

```cpp
enum class TokenType {
    IDENT,      // Variable name (destination)
    FIELD,      // $X (field/column reference)
    NUMBER,     // Integer or double
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    LPAREN,     // (
    RPAREN,     // )
    EQ,         // =
    END         // End of input
};
```

### MathOp Structure

The parser outputs a list of `MathOp` operations:

```cpp
struct MathOp {
    std::string op;           // "add", "subtract", "multiply", "divide"
    std::string src;          // Source: field name or "_tmp_N"
    bool srcIsField;          // true if src is a column reference
    std::string operand;      // Operand: field name, "_tmp_N", or value string
    bool operandIsField;      // true if operand is a column reference
    double operandValue;      // Numeric value if scalar
    std::string dest;         // Destination column name
};
```

### Example Parsing

Input: `D = ($A + $B) * $C`

Output:
```cpp
[0] { op: "add",      src: "A",      srcIsField: true,
                      operand: "B",  operandIsField: true,
                      dest: "_tmp_0" }

[1] { op: "multiply", src: "_tmp_0", srcIsField: false,
                      operand: "C",  operandIsField: true,
                      dest: "D" }
```

## C++ API Functions

### parseEquation()

Parses an equation string into MathOp operations.

```cpp
std::vector<MathOp> parseEquation(
    const std::string& equation,
    int* tmpCounter = nullptr);
```

**Parameters:**
- `equation`: The equation string (e.g., `"D = ($A + $B) * $C"`)
- `tmpCounter`: Optional pointer to shared temp counter

**Shared Temp Counter:**

When parsing multiple equations, use a shared counter to avoid `_tmp_N` collisions:

```cpp
int tmpCounter = 0;

auto ops1 = parseEquation("D = ($A + $B) * 2", &tmpCounter);
// ops1[0].dest = "_tmp_0", ops1[1].dest = "D"
// tmpCounter is now 2

auto ops2 = parseEquation("E = ($D * $C) + 10", &tmpCounter);
// ops2[0].dest = "_tmp_2" (not _tmp_0!)
// tmpCounter is now 4

auto ops3 = parseEquation("F = ($E + $E) * 2", &tmpCounter);
// ops3[0].dest = "_tmp_4" (unique!)
// tmpCounter is now 6
```

Without the shared counter, all equations would use `_tmp_0`, causing incorrect reconstruction.

### extractMathOps()

Extracts MathOp structures from existing graph nodes between dynamic markers.

```cpp
std::vector<MathOp> extractMathOps(
    const NodeGraph& graph,
    const std::string& beginNodeId,
    const std::string& endNodeId);
```

**Algorithm:**
1. Follow CSV connections from `beginNodeId` toward `endNodeId`
2. For each `math/*` node encountered:
   - Extract operation type from definition name
   - Find connected `scalar/string_as_field` or `scalar/double_value` nodes
   - Read `_value` property from each input node
3. Return MathOps in execution order

**Type Handling:**

The graph editor may serialize `double` values as `int`. The extraction handles both:

```cpp
if (it->second.getType() == NodeType::Double) {
    val = it->second.getDouble();
} else if (it->second.getType() == NodeType::Int) {
    val = static_cast<double>(it->second.getInt());
}
```

### reconstructEquations()

Reconstructs human-readable equation strings from MathOps.

```cpp
std::vector<std::string> reconstructEquations(const std::vector<MathOp>& ops);
```

**Algorithm:**
1. Build `destMap`: maps destination names to MathOp pointers
2. Find "final" ops (dest not used as input by another op)
3. For each final op, recursively build expression:
   - If operand is `_tmp_N`, inline its expression
   - Add parentheses based on precedence rules

**Example:**

```cpp
std::vector<MathOp> ops = {
    {"add", "A", true, "B", true, 0.0, "_tmp_0"},
    {"multiply", "_tmp_0", false, "C", true, 0.0, "D"}
};
auto equations = reconstructEquations(ops);
// Returns: ["D = ($A + $B) * $C"]
```

**Precedence Rules:**

| Precedence | Operators | Parentheses needed when... |
|------------|-----------|---------------------------|
| 2 (high)   | `*`, `/`  | Never (highest precedence) |
| 1 (low)    | `+`, `-`  | Inlined into `*` or `/` context |
| 3 (atom)   | field, number | Never |

**Semantic Equivalence:**

Reconstruction produces semantically equivalent equations, not identical strings.
Redundant parentheses are removed:

```
Input:  "E = ($D * $C) + 10"   (parens are redundant)
Output: "E = $D * $C + 10"     (mathematically identical)
```

Only necessary parentheses are preserved:

```
Input:  "D = ($A + $B) * $C"   (parens change meaning)
Output: "D = ($A + $B) * $C"   (preserved)
```

## API Endpoints

### POST /api/graph/:slug/apply-dynamic

Injects nodes into the graph and **saves a new version**.

**Request:**
```json
{
    "dynamic_nodes": [
        {
            "_name": "zone_name",
            "params": [
                "D=($A+$B)*$C",
                "E=$D*42"
            ]
        }
    ],
    "version_name": "Optional version description"
}
```

**Response:**
```json
{
    "status": "ok",
    "version_id": 15,
    "nodes_added": 12,
    "nodes_removed": 8,
    "message": "Graph updated: 8 nodes removed, 12 new nodes added"
}
```

**Behavior:**
1. Loads the graph
2. For each zone in `dynamic_nodes`:
   - Finds `dynamic_begin` and `dynamic_end` with matching `_name`
   - **Removes existing intermediate nodes** (math nodes and their scalar inputs)
   - Parses equations and creates new nodes:
     - `scalar/string_as_field` for field references ($X)
     - `scalar/double_value` for numeric constants
     - `math/*` for operations (add, subtract, multiply, divide)
   - Connects nodes in chain
   - Reconnects to `dynamic_end`
3. Saves as new graph version
4. Returns version ID, nodes added, and nodes removed

**Idempotent Updates:**

The endpoint supports re-applying equations to an existing graph. Previous math nodes
between `dynamic_begin` and `dynamic_end` are automatically removed before inserting
the new ones. This allows editing and re-applying equations without manual cleanup.

### GET /api/graph/:slug/dynamic-equations

Reconstructs equations from existing math nodes between dynamic markers.

**Response:**
```json
{
    "status": "ok",
    "zones": [
        {
            "_name": "calculations",
            "equations": [
                "D = ($A + $B) * $C",
                "E = $D * 42"
            ]
        }
    ]
}
```

**Use case:** Pre-filling UI with existing equations for editing, validating roundtrip (parse → apply → reconstruct).

### POST /api/graph/:slug/execute-dynamic

Injects nodes temporarily and **executes without saving**.

**Request:** Same as apply-dynamic

**Response:**
```json
{
    "status": "ok",
    "session_id": "abc123",
    "execution_id": 42,
    "results": { ... },
    "csv_metadata": { ... },
    "duration_ms": 150
}
```

**Use case:** Testing equations before permanently applying them.

## Node Generation

For each `MathOp`, the system creates:

```
┌─────────────────────┐
│ string_as_field     │──┐
│ _value = "A"        │  │
└─────────────────────┘  │    ┌─────────────────┐
                         ├───►│      add        │
┌─────────────────────┐  │    │                 │───► csv out
│ string_as_field     │──┘    │  csv in ◄───────│
│ _value = "B"        │       └────────▲────────┘
└─────────────────────┘                │
                                       │
┌─────────────────────┐                │
│ string_as_field     │────────────────┘
│ _value = "_tmp_0"   │  (dest)
└─────────────────────┘
```

### Node Positioning

Nodes are automatically positioned relative to `dynamic_begin`:

- Math nodes: Horizontally spaced (+600px per operation, starting at +650px)
- Input nodes: Left of math node (-280px)
  - `src` input: -120px vertical offset
  - `operand` input: +20px vertical offset
  - `dest` input: +160px vertical offset

## Client Integration (viewer.html)

The viewer automatically detects dynamic zones and displays input fields:

```html
┌─────────────────────────────────────────────┐
│ Dynamic Equations              [2 zones]    │
├─────────────────────────────────────────────┤
│ ┌─ calculations ──────────────────────────┐ │
│ │ D=($A+$B)*$C                            │ │
│ │ E=$D*42                                 │ │
│ └─────────────────────────────────────────┘ │
│ ┌─ pricing ───────────────────────────────┐ │
│ │ total=$quantity*$unit_price             │ │
│ └─────────────────────────────────────────┘ │
│                                             │
│ [Apply to Graph]  [Clear All]               │
└─────────────────────────────────────────────┘
```

### JavaScript Functions

```javascript
// Load dynamic zones from graph definition
async function loadDynamicZones()

// Fetch and display existing equations from /dynamic-equations
async function loadExistingEquations()

// Get equations from textarea for a zone
function getEquationsForZone(zoneName)

// Apply equations to graph (calls /apply-dynamic)
async function applyDynamic()

// Clear all equation fields
function clearEquations()
```

### Prefilling Textareas

When loading a graph, existing equations are automatically reconstructed and displayed:

```javascript
async function loadDynamicZones() {
    // 1. Fetch graph to find dynamic_begin nodes
    // 2. Render textareas for each zone
    // 3. Call loadExistingEquations() to prefill from /dynamic-equations
}
```

This enables editing existing equations rather than starting from scratch.

## Error Handling

### Parser Errors

- `"Unexpected character in equation: X"` - Invalid character
- `"Expected destination identifier at start of equation"` - Missing `VAR=`
- `"Unexpected token in equation"` - Syntax error

### API Errors

- `"dynamic_begin with _name='X' not found"` - Zone not found
- `"No direct connection from dynamic_begin to dynamic_end"` - Nodes not connected
- `"Missing or invalid 'dynamic_nodes' array"` - Bad request format

## Testing

### EquationParserTest.cpp

```cpp
// Tokenizer tests
TEST_CASE("Tokenizer basic tokens")
TEST_CASE("Tokenizer field references")
TEST_CASE("Tokenizer operators and parentheses")

// Parser tests
TEST_CASE("Parse simple addition")
TEST_CASE("Parse multiplication before addition")  // Precedence
TEST_CASE("Parse parentheses override precedence")
TEST_CASE("Parse complex equation from plan example")

// Reconstruction tests
TEST_CASE("Reconstruct simple addition")
TEST_CASE("Reconstruct with precedence - no parens needed")
TEST_CASE("Reconstruct with precedence - parens needed")
TEST_CASE("Reconstruct nested parentheses")
TEST_CASE("Reconstruct with scalars")
TEST_CASE("Reconstruct field names with spaces")
TEST_CASE("Reconstruct multiple final destinations")

// Roundtrip tests (parse then reconstruct)
TEST_CASE("Roundtrip: parse and reconstruct simple")
TEST_CASE("Roundtrip: parse and reconstruct with parens")
TEST_CASE("Roundtrip: parse and reconstruct complex")
TEST_CASE("Roundtrip: parse and reconstruct with scalar")
TEST_CASE("Roundtrip: parse and reconstruct with spaces in names")

// Shared temp counter test
TEST_CASE("Shared temp counter across multiple equations")
```

**36 test cases, 172 assertions** covering all parsing and reconstruction scenarios.

### DynamicNodesTest.cpp

```cpp
TEST_CASE("Dynamic nodes are registered")
TEST_CASE("dynamic_begin passes through CSV")
TEST_CASE("dynamic_end passes through CSV")
TEST_CASE("dynamic_begin errors without CSV input")
```

## Temporary Column Cleanup

The equation system creates temporary columns (`_tmp_0`, `_tmp_2`, etc.)
for intermediate results. To remove them from the final CSV, use
the `clean_tmp_columns` node:

```
csv_source → dynamic_begin → dynamic_end → clean_tmp_columns → output
```

This node automatically removes all columns whose name starts with `_tmp_`.

## Workflow Example

1. **Create graph in editor:**
   ```
   csv_source → dynamic_begin("calc") → dynamic_end("calc") → output
   ```

2. **Open viewer.html**
   - Dynamic Equations section appears with "calc" zone

3. **Enter equations:**
   ```
   profit = $revenue - $cost
   margin = $profit / $revenue * 100
   ```

4. **Click "Apply to Graph"**
   - API creates nodes and saves new version
   - Message: "Graph updated with 8 new nodes"

5. **Open graph in editor**
   - See the injected math nodes between begin/end
   - Can manually adjust positions or connections
   - Save changes if needed

6. **Reload viewer.html**
   - Equations are automatically reconstructed from graph nodes
   - Textareas show: `profit = $revenue - $cost` and `margin = $profit / $revenue * 100`
   - Can edit and re-apply

7. **Edit equations and re-apply**
   - Modify equations: `margin = $profit / $revenue * 100` → `margin = $profit / $cost * 100`
   - Click "Apply to Graph" again
   - Old math nodes are automatically removed and replaced with new ones
   - Message: "Graph updated: 8 nodes removed, 8 new nodes added"

8. **Execute graph**
   - New columns `profit` and `margin` appear in output

## Implementation Notes

### Temporary Variable Naming

When parsing multiple equations in a single API call, a shared counter ensures unique temp names:

```
D = ($A + $B) * 2      →  _tmp_0 (for A+B)
E = ($D * $C) + 10     →  _tmp_2 (for D*C, not _tmp_0)
F = ($E + $E) * 2      →  _tmp_4 (for E+E, unique)
```

This prevents collisions when reconstructing equations from the graph.

### Type Serialization

The graph editor may serialize numeric values differently:
- `10.0` (double) may become `10` (int) after save

The `extractMathOps()` function handles both `NodeType::Double` and `NodeType::Int` to ensure robust roundtrip support.

### Semantic vs Syntactic Equivalence

Reconstruction preserves **semantic** equivalence, not syntactic:

| Original | Reconstructed | Reason |
|----------|---------------|--------|
| `($A * $B) + $C` | `$A * $B + $C` | Parens redundant (mul before add) |
| `($A + $B) * $C` | `($A + $B) * $C` | Parens required (changes meaning) |
| `$A + ($B + $C)` | `$A + $B + $C` | Parens redundant (left-associative) |

### Idempotent Apply

The `/apply-dynamic` endpoint is idempotent:

1. **First call**: Creates math nodes between begin/end
2. **Subsequent calls**: Removes existing math nodes, then creates new ones

This enables an edit-apply workflow without manual node cleanup:
```
Apply "D = $A + $B"  →  Creates nodes
Edit to "D = $A * $B"
Apply again          →  Removes old nodes, creates new ones
```

Nodes removed include:
- All `math/*` nodes in the CSV chain between begin and end
- All `scalar/string_as_field` and `scalar/double_value` nodes connected to them
