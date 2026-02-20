# Node System

## Overview

Native C++ node system for visual programming with fluent API, type-safe workloads, and automatic broadcasting.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         NODE SYSTEM ARCHITECTURE                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐              │
│   │ NodeBuilder │ ──► │NodeDefinit. │ ──► │NodeRegistry │              │
│   │ (fluent API)│     │ (metadata)  │     │ (singleton) │              │
│   └─────────────┘     └─────────────┘     └─────────────┘              │
│                                                  │                      │
│                                                  ▼                      │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐              │
│   │  NodeGraph  │ ──► │NodeExecutor │ ──► │ NodeContext │              │
│   │ (instances) │     │ (topo sort) │     │ (runtime)   │              │
│   └─────────────┘     └─────────────┘     └─────────────┘              │
│                                                  │                      │
│                              ┌───────────────────┴───────────────┐     │
│                              ▼                                   ▼     │
│                       ┌─────────────┐                     ┌──────────┐ │
│                       │  Workload   │                     │ DataFrame│ │
│                       │{value,type} │                     │  (CSV)   │ │
│                       └─────────────┘                     └──────────┘ │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## File Structure

```
src/nodes/
├── Types.hpp              # NodeType enum, Workload class, PortType
├── Types.cpp              # Workload implementation
├── NodeContext.hpp        # Execution context (inputs/outputs)
├── NodeContext.cpp        # NodeContext implementation
├── NodeDefinition.hpp     # Node metadata (inputs, outputs, compile function)
├── NodeBuilder.hpp        # Fluent API builder
├── NodeBuilder.cpp        # NodeBuilder implementation
├── NodeRegistry.hpp       # Central registry (singleton)
├── NodeRegistry.cpp       # NodeRegistry implementation
├── NodeExecutor.hpp       # Graph + execution engine
├── NodeExecutor.cpp       # NodeExecutor implementation
├── DynRequest.hpp         # Dynamic PostgreSQL request builder
├── DynRequest.cpp         # DynRequest implementation
├── EquationParser.hpp/cpp # Dynamic equation parser (see DYNAMIC-NODES.md)
├── LabelRegistry.hpp      # Label storage during execution
├── PluginContext.hpp      # Context passed to plugins at init
├── plugin_init.hpp.in     # CMake template for auto-generated plugin header
└── nodes/                 # Node plugins (auto-discovered by CMake)
    ├── common/            # Built-in generic nodes
    │   ├── register.*     # Plugin entry point
    │   ├── ScalarNodes.*  # int_value, double_value, string_value, ...
    │   ├── CsvNodes.*     # csv_source, field, join_flex, output
    │   ├── MathNodes.*    # add, subtract, multiply, divide, modulus
    │   ├── AggregateNodes.* # group, pivot, tree_group
    │   ├── SelectNodes.*  # select_by_name, select_by_pos, reorder_columns
    │   ├── PostgresNodes.* # postgres_config, postgres_query, postgres_func
    │   ├── StringNodes.*  # concat, split, replace, ...
    │   ├── LabelNodes.*   # label_define_*, label_ref_*
    │   ├── DynamicNodes.* # dynamic_begin, dynamic_end
    │   └── VizNodes.*     # bar_chart, timeline, ...
    └── <plugin>/          # Domain-specific plugin (see PLUGINS.md)
        ├── register.*     # Plugin entry point
        └── ...            # Custom nodes, caches, storage
```

Node implementations are organized as **plugins** under `src/nodes/nodes/`. Each subdirectory with a `CMakeLists.txt` is auto-discovered at build time. See [PLUGINS.md](../architecture/PLUGINS.md) for details.

---

## Type System

### NodeType Enum

```cpp
namespace nodes {

enum class NodeType {
    // Scalars - broadcast to all rows
    Int,        // int64_t
    Double,     // double
    String,     // std::string
    Bool,       // bool
    Null,       // monostate

    // Vector - reference to a CSV column
    Field,      // column name (string) + lookup in CSV

    // CSV - full DataFrame
    Csv         // shared_ptr<DataFrame>
};

} // namespace nodes
```

### Workload Class

The **Workload** is the core abstraction for passing data between nodes. It encapsulates `{value, type}` to handle multi-type ports.

```cpp
namespace nodes {

using NodeValue = std::variant<
    std::monostate,                           // Null
    int64_t,                                  // Int
    double,                                   // Double
    std::string,                              // String, Field
    bool,                                     // Bool
    std::shared_ptr<dataframe::DataFrame>     // Csv
>;

class Workload {
public:
    // Constructors
    Workload();  // Null workload
    Workload(int64_t value, NodeType type = NodeType::Int);
    Workload(double value, NodeType type = NodeType::Double);
    Workload(const std::string& value, NodeType type = NodeType::String);
    Workload(bool value);
    Workload(std::shared_ptr<dataframe::DataFrame> value);

    // Getters
    NodeType getType() const { return m_type; }
    const NodeValue& getValue() const { return m_value; }

    // Type-safe extraction (throws on wrong type)
    int64_t getInt() const;
    double getDouble() const;
    const std::string& getString() const;
    bool getBool() const;
    std::shared_ptr<dataframe::DataFrame> getCsv() const;

    // Broadcasting support - get value at row index
    // For scalars: returns same value regardless of row (broadcasting)
    // For fields: looks up value in CSV at given row
    int64_t getIntAtRow(size_t rowIndex,
                        const std::vector<std::string>& header,
                        const std::shared_ptr<dataframe::DataFrame>& csv) const;
    double getDoubleAtRow(size_t rowIndex,
                          const std::vector<std::string>& header,
                          const std::shared_ptr<dataframe::DataFrame>& csv) const;
    std::string getStringAtRow(size_t rowIndex,
                               const std::vector<std::string>& header,
                               const std::shared_ptr<dataframe::DataFrame>& csv) const;

    // Validity checks
    bool isNull() const;
    bool isScalar() const;  // Int, Double, String, Bool
    bool isField() const;
    bool isCsv() const;

private:
    NodeValue m_value;
    NodeType m_type;
};

} // namespace nodes
```

### PortType Class

Supports single type or multiple types for a port:

```cpp
class PortType {
public:
    explicit PortType(NodeType type);
    explicit PortType(std::initializer_list<NodeType> types);

    bool accepts(NodeType type) const;
    bool accepts(const Workload& workload) const;

    const std::vector<NodeType>& getTypes() const { return m_types; }
    bool isMultiType() const { return m_types.size() > 1; }

private:
    std::vector<NodeType> m_types;
};
```

---

## Broadcasting Logic

When a **scalar** is used in a **CSV context**, it broadcasts to all rows:

```
field("price") + int(10)

price: [1.50, 0.75, 2.00]  +  10  =  [11.50, 10.75, 12.00]
         │                     │              │
       vector              scalar          vector
                         (broadcasts)
```

Implementation in `Workload::getDoubleAtRow()`:
- Scalar types: return same value for all rows
- Field type: lookup in CSV at given row index

```cpp
double Workload::getDoubleAtRow(size_t rowIndex,
                                const std::vector<std::string>& header,
                                const std::shared_ptr<dataframe::DataFrame>& csv) const {
    // Scalar types: always return same value (broadcasting)
    if (m_type == NodeType::Int) {
        return static_cast<double>(std::get<int64_t>(m_value));
    }
    if (m_type == NodeType::Double) {
        return std::get<double>(m_value);
    }
    if (m_type == NodeType::String) {
        return std::stod(std::get<std::string>(m_value));
    }

    // Field type: lookup in CSV at rowIndex
    if (m_type == NodeType::Field) {
        const std::string& columnName = std::get<std::string>(m_value);
        auto column = csv->getColumn(columnName);
        if (!column) {
            throw std::runtime_error("Column not found: " + columnName);
        }

        if (column->getType() == dataframe::ColumnTypeOpt::INT) {
            auto intCol = std::dynamic_pointer_cast<dataframe::IntColumn>(column);
            return static_cast<double>(intCol->at(rowIndex));
        }
        if (column->getType() == dataframe::ColumnTypeOpt::DOUBLE) {
            auto dblCol = std::dynamic_pointer_cast<dataframe::DoubleColumn>(column);
            return dblCol->at(rowIndex);
        }
        if (column->getType() == dataframe::ColumnTypeOpt::STRING) {
            auto strCol = std::dynamic_pointer_cast<dataframe::StringColumn>(column);
            return std::stod(strCol->at(rowIndex));
        }
    }

    throw std::runtime_error("Cannot get double from type: " + nodeTypeToString(m_type));
}
```

---

## NodeContext

The execution context passed to node compile functions:

```cpp
class NodeContext {
public:
    NodeContext();

    // === Input Access (called by node logic) ===
    Workload getInputWorkload(const std::string& name) const;
    bool hasInput(const std::string& name) const;

    // === Output Setting (called by node logic) ===
    void setOutput(const std::string& name, const Workload& workload);
    void setOutput(const std::string& name, int64_t value);
    void setOutput(const std::string& name, double value);
    void setOutput(const std::string& name, const std::string& value);
    void setOutput(const std::string& name, bool value);
    void setOutput(const std::string& name, std::shared_ptr<dataframe::DataFrame> value);

    // === CSV Broadcasting Support ===
    std::shared_ptr<dataframe::DataFrame> getActiveCsv() const;
    void setActiveCsv(std::shared_ptr<dataframe::DataFrame> csv);
    double getDoubleAtRow(const std::string& inputName, size_t rowIndex) const;
    int64_t getIntAtRow(const std::string& inputName, size_t rowIndex) const;
    std::string getStringAtRow(const std::string& inputName, size_t rowIndex) const;

    // === Error Handling ===
    void setError(const std::string& message);
    bool hasError() const { return m_hasError; }
    const std::string& getErrorMessage() const { return m_errorMessage; }

    // === Internal (used by executor) ===
    void setInput(const std::string& name, const Workload& workload);
    Workload getOutput(const std::string& name) const;
    const std::unordered_map<std::string, Workload>& getOutputs() const;

private:
    std::unordered_map<std::string, Workload> m_inputs;
    std::unordered_map<std::string, Workload> m_outputs;
    std::shared_ptr<dataframe::DataFrame> m_activeCsv;
    bool m_hasError = false;
    std::string m_errorMessage;
};
```

---

## NodeDefinition

Immutable metadata describing a node type:

```cpp
using CompileFunction = std::function<void(NodeContext&)>;

struct InputDef {
    std::string name;
    PortType type;
    bool required = true;
};

struct OutputDef {
    std::string name;
    PortType type;
};

class NodeDefinition {
public:
    NodeDefinition(
        std::string name,
        std::string category,
        std::vector<InputDef> inputs,
        std::vector<OutputDef> outputs,
        CompileFunction compileFunc,
        bool isEntryPoint = false
    );

    const std::string& getName() const;
    const std::string& getCategory() const;
    const std::vector<InputDef>& getInputs() const;
    const std::vector<OutputDef>& getOutputs() const;
    bool isEntryPoint() const;

    const InputDef* findInput(const std::string& name) const;
    const OutputDef* findOutput(const std::string& name) const;

    void compile(NodeContext& ctx) const { m_compileFunc(ctx); }
};

using NodeDefinitionPtr = std::shared_ptr<const NodeDefinition>;
```

---

## NodeBuilder (Fluent API)

### Conventions

**CSV ports must always be declared first** (both inputs and outputs). This ensures consistent visual layout in the graph editor where CSV connections flow through the top of nodes.

```cpp
// GOOD: CSV first
.inputOptional("csv", Type::Csv)
.input("src", ...)
.output("csv", Type::Csv)
.output("result", ...)

// BAD: CSV not first
.input("src", ...)
.inputOptional("csv", Type::Csv)  // Wrong position
```

### API

```cpp
class NodeBuilder {
public:
    NodeBuilder(const std::string& name, const std::string& category);

    // Inputs
    NodeBuilder& input(const std::string& name, NodeType type);
    NodeBuilder& input(const std::string& name, std::initializer_list<NodeType> types);
    NodeBuilder& inputOptional(const std::string& name, NodeType type);
    NodeBuilder& inputOptional(const std::string& name, std::initializer_list<NodeType> types);

    // Outputs
    NodeBuilder& output(const std::string& name, NodeType type);
    NodeBuilder& output(const std::string& name, std::initializer_list<NodeType> types);

    // Compile Function
    NodeBuilder& onCompile(CompileFunction func);

    // Options
    NodeBuilder& entryPoint();  // Mark as entry point (no required inputs)

    // Build
    NodeDefinitionPtr build();
    NodeDefinitionPtr buildAndRegister();
    NodeDefinitionPtr buildAndRegister(NodeRegistry& registry);
};

using Type = NodeType;  // Convenience alias
```

### Example

```cpp
NodeBuilder("add", "math")
    .inputOptional("csv", Type::Csv)      // CSV input first
    .input("src", {Type::Int, Type::Double, Type::Field})
    .inputOptional("dest", Type::Field)
    .input("operand", {Type::Int, Type::Double, Type::Field})
    .output("csv", Type::Csv)             // CSV output first
    .output("result", Type::Double)
    .onCompile([](NodeContext& ctx) {
        auto src = ctx.getInputWorkload("src");
        auto operand = ctx.getInputWorkload("operand");
        ctx.setOutput("result", src.getDouble() + operand.getDouble());
    })
    .buildAndRegister();
```

---

## NodeRegistry

Singleton registry for all node definitions:

```cpp
class NodeRegistry {
public:
    static NodeRegistry& instance();

    void registerNode(NodeDefinitionPtr definition);
    void unregisterNode(const std::string& name);

    NodeDefinitionPtr getNode(const std::string& name) const;
    bool hasNode(const std::string& name) const;

    std::vector<std::string> getNodeNames() const;
    std::vector<std::string> getNodeNamesInCategory(const std::string& category) const;
    std::vector<std::string> getCategories() const;

    void clear();  // For testing
};
```

---

## NodeGraph & NodeExecutor

### NodeGraph

```cpp
struct NodeInstance {
    std::string id;              // Unique instance ID (e.g., "node_1")
    std::string definitionName;  // Reference to NodeDefinition (e.g., "add")
    std::unordered_map<std::string, Workload> properties;  // Widget values
};

struct Connection {
    std::string sourceNodeId;
    std::string sourcePortName;
    std::string targetNodeId;
    std::string targetPortName;
};

class NodeGraph {
public:
    std::string addNode(const std::string& definitionName);
    void removeNode(const std::string& nodeId);
    NodeInstance* getNode(const std::string& nodeId);

    void connect(const std::string& sourceNodeId, const std::string& sourcePort,
                 const std::string& targetNodeId, const std::string& targetPort);

    void setProperty(const std::string& nodeId, const std::string& name, const Workload& value);

    const std::unordered_map<std::string, NodeInstance>& getNodes() const;
    const std::vector<Connection>& getConnections() const;
};
```

### NodeExecutor

```cpp
class NodeExecutor {
public:
    explicit NodeExecutor(const NodeRegistry& registry);

    // Returns: map<nodeId, map<portName, Workload>>
    std::unordered_map<std::string, std::unordered_map<std::string, Workload>>
    execute(const NodeGraph& graph);

private:
    std::vector<std::string> topologicalSort(const NodeGraph& graph);
    void gatherInputs(const NodeGraph& graph, const std::string& nodeId,
                       const std::unordered_map<std::string, NodeContext>& executedNodes,
                       NodeContext& ctx);
};
```

### Execution Flow

```
1. topologicalSort(graph) -> [nodeA, nodeB, nodeC, ...]
   (respects dependencies: parents before children)

2. For each node in order:
   a. Create NodeContext
   b. gatherInputs() - copy outputs from connected upstream nodes
   c. Set properties as inputs (widget values)
   d. Get NodeDefinition from registry
   e. definition.compile(ctx)
   f. Store ctx for downstream nodes

3. Return all outputs
```

---

## Widgets and Properties

Widgets allow configuring nodes from the UI. Widget values are passed to the node as "properties" accessible via `ctx.getInputWorkload("_property_name")`.

### Naming Convention

Property names start with `_` (underscore) to distinguish them from actual connected inputs.

### Property Examples

| Node | Property | Type | Description |
|------|----------|------|-------------|
| `int_value` | `_value` | Int | Configured integer value |
| `string_value` | `_value` | String | Configured text value |
| `field` | `_column` | String | Selected column name |
| `group` | `_aggregation` | String | Aggregation function (sum, avg, etc.) |
| `csv_value` | `_value` | Csv | Configurable DataFrame |
| `select_by_pos` | `_default` | String | Default behavior ("true"/"false") |
| `output` | `_name` | String | Published output name |

### C++ Side (onCompile)

```cpp
// Retrieve a property (with default value if absent)
auto aggProp = ctx.getInputWorkload("_aggregation");
std::string aggFunction = aggProp.isNull() ? "sum" : aggProp.getString();

// Check if a property is defined
if (!ctx.hasInput("_column")) {
    ctx.setError("No column specified");
    return;
}
```

### Client Side (nodeTypes.ts)

Widgets are added in the `addWidgetsForNode()` function:

```typescript
case 'group':
  node.addWidget(
    'combo',                    // Widget type
    '_aggregation',             // Property name
    'sum',                      // Default value
    (v: unknown) => {           // Change callback
      node.properties._aggregation = v as NodeProperty;
    },
    { values: ['sum', 'avg', 'min', 'max', 'first', 'count'] }
  );
  break;
```

### Available Widget Types

| Type | Description | Options |
|------|-------------|---------|
| `text` | Free text field | - |
| `number` | Numeric field | `min`, `max`, `step`, `precision` |
| `combo` | Dropdown list | `values: string[]` |
| `button` | Clickable button | Click callback |

---

## Dynamic Inputs (+/- Pattern)

Some nodes allow adding/removing inputs on the fly with +/- buttons.

### Nodes with Dynamic Inputs

| Node | Base input | Dynamic inputs | Type |
|------|------------|----------------|------|
| `group` | `field` | `field_1` to `field_99` | Field |
| `tree_group` | `field` | `field_1` to `field_99` | Field |
| `select_by_name` | `column` | `column_1` to `column_99` | Field |
| `select_by_pos` | - | `col_0` to `col_99` | Bool |
| `string_as_fields` | - | widgets `_field_0` to `_field_99` | Field (dynamic outputs) |

**Note**: `string_as_fields` is a special case. It does not have dynamic inputs but **dynamic widgets** (+/-) that generate outputs `value`, `value_1`, etc. When connected to a node with dynamic inputs (e.g., `tree_group.field`), `NodeExecutor::gatherInputs()` automatically expands its outputs into numbered inputs on the target node (`value` -> `field`, `value_1` -> `field_1`, etc.).

### C++ Side (Server)

**1. Declare the inputs (NodeBuilder)**

```cpp
auto builder = NodeBuilder("select_by_name", "select")
    .input("csv", Type::Csv)
    .input("column", Type::Field);  // First input (required)

// Additional inputs (optional)
for (int i = 1; i <= 99; i++) {
    builder.inputOptional("column_" + std::to_string(i), Type::Field);
}

builder.output("csv", Type::Csv)
    .onCompile([](NodeContext& ctx) { ... })
    .buildAndRegister();
```

**2. Iterate over inputs (onCompile)**

```cpp
// Collect all connected columns
std::vector<std::string> columns;

auto col = ctx.getInputWorkload("column");
if (!col.isNull()) {
    columns.push_back(col.getString());
}

for (int i = 1; i <= 99; i++) {
    auto c = ctx.getInputWorkload("column_" + std::to_string(i));
    if (c.isNull()) break;
    columns.push_back(c.getString());
}
```

### Client Side (nodeTypes.ts)

```typescript
function setupSelectByNameNode(node: AnodeLGraphNode): void {
  // Remove dynamic inputs pre-created by the server
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('column_')) as any;
  }

  node._visibleColumnCount = 1;

  // + button to add an input
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleColumnCount ?? 1) < 100) {
      const newInputName = 'column_' + node._visibleColumnCount;
      node.addInput(newInputName, 'field');
      node._visibleColumnCount = (node._visibleColumnCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // - button to remove an input
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleColumnCount ?? 1) > 1) {
      const inputName = 'column_' + ((node._visibleColumnCount ?? 1) - 1);
      const slot = node.findInputSlot(inputName);
      if (slot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[slot]?.link) {
          node.disconnectInput(slot);
        }
        node.removeInput(slot);
      }
      node._visibleColumnCount = (node._visibleColumnCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Restoration on load
  node.restoreColumnInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith('column_')) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 1; i < count; i++) {
      this.addInput('column_' + i, 'field');
    }
    this._visibleColumnCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}
```

**Important points:**

- Remove dynamic inputs pre-created by the server at startup
- Use a `_visibleXxxCount` counter to track displayed inputs
- Implement `restoreXxxInputs()` for loading saved graphs
- Call `setSize(computeSize())` after each modification

---

## Labels (Wireless Connections)

Labels allow **naming data** to **reuse it elsewhere** in the graph without visible connections.

### How It Works

1. **`label_define_*`**: Registers a value under a unique identifier. The value is passed through as output (pass-through).
2. **`label_ref_*`**: Retrieves a value by its identifier. Error if the identifier does not exist.

### Automatic Execution Order

The `NodeExecutor` automatically detects define/ref pairs with the same `_label` and adds an **implicit dependency** to ensure the define executes before the ref.

```
[int_value] ──► [label_define_int] ──────────────────► [output]
                    _label="data"
                           │
                           │ (implicit dependency)
                           ▼
               [label_ref_int] ──► [add] ──► [output]
                    _label="data"
```

### Technical Implementation

- **`LabelRegistry`**: Singleton that stores labels during execution (`src/nodes/LabelRegistry.hpp`)
- **Cleanup**: Labels are cleared at the beginning of each execution (`LabelRegistry::instance().clear()` in `NodeExecutor::execute()`)
- **Dependency detection**: In `NodeExecutor::topologicalSort()`, `label_define_*` and `label_ref_*` nodes with the same `_label` are linked by an implicit dependency

### Implementation Pitfall

**Watch out for the node type format!** In the frontend, node types include the category with a slash (e.g., `label/label_define_int`). When detecting label nodes, you must use `find()` with `!= std::string::npos` and not `== 0`:

```cpp
// CORRECT: searches for "label_define_" anywhere in the string
if (instance.definitionName.find("label_define_") != std::string::npos)

// INCORRECT: does not work because the type is "label/label_define_int"
if (instance.definitionName.find("label_define_") == 0)
```

---

## DynRequest - Dynamic PostgreSQL Queries

The `DynRequest` class allows building PostgreSQL function calls with typed parameters.

### Basic Usage

```cpp
#include "nodes/DynRequest.hpp"

DynRequest req;
req.func("anode_identify_phase")
   .addIntArrayParam({10, 20, 30})
   .addStringArrayParam({"Phase A", "Phase B"});

std::string sql = req.buildSQL();
// -> "SELECT * FROM anode_identify_phase(ARRAY[10, 20, 30]::INT[], ARRAY['Phase A', 'Phase B']::TEXT[])"
```

### Available Methods

**Scalar parameters:**

| Method | Description |
|--------|-------------|
| `addIntParam(int64_t)` | Integer |
| `addDoubleParam(double)` | Float |
| `addStringParam(string)` | String |
| `addBoolParam(bool)` | Boolean |
| `addNullParam()` | NULL |

**Array parameters:**

| Method | Generated SQL |
|--------|---------------|
| `addIntArrayParam(vector<int64_t>)` | `ARRAY[...]::INT[]` |
| `addDoubleArrayParam(vector<double>)` | `ARRAY[...]::DOUBLE PRECISION[]` |
| `addStringArrayParam(vector<string>)` | `ARRAY[...]::TEXT[]` |
| `addIntArray2DParam(vector<vector<int64_t>>)` | `ARRAY[ARRAY[...]]::INT[][]` |

**Parameters from Workload (with broadcasting):**

| Method | Description |
|--------|-------------|
| `addIntFromWorkload(wl, csv)` | Integer from workload |
| `addStringFromWorkload(wl, csv)` | String from workload |
| `addIntArrayFromWorkload(wl, csv)` | Integer array from workload |
| `addStringArrayFromWorkload(wl, csv)` | String array from workload |
| `addDoubleArrayFromWorkload(wl, csv)` | Float array from workload |
| `addTimestampFromWorkload(wl, csv)` | Timestamp from workload (automatic conversion) |

### Timestamp Conversion

`addTimestampFromWorkload` supports multiple input formats:

- Unix timestamp (number): `1704067200`
- `dd/mm/yyyy` format: `"01/01/2024"`
- `dd/mm/yy` format: `"01/01/24"`
- Text format: `"1 janvier 2024"`, `"1 january 2024"`

For fields (Field), the method verifies that all rows in the CSV have the same value (scalar parameter constraint).

### Example with PostgreSQL Node

```cpp
void registerIdentifyPelHvNode() {
    NodeBuilder("identifyPelHv", "perimeter")
        .inputOptional("csv", Type::Csv)
        .input("pelhv label", {Type::Field, Type::String})
        .output("csv", Type::Csv)
        .output("pelhv id", Type::Field)
        .output("pelhv label", Type::Field)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto labelWL = ctx.getInputWorkload("pelhv label");
            auto csv = ctx.getActiveCsv();

            DynRequest req;
            req.func("anode_identify_pelhv")
               .addStringArrayFromWorkload(labelWL, csv, true);

            auto& pool = postgres::PostgresPool::instance();
            auto result = pool.executeQuery(req.buildSQL());

            ctx.setOutput("csv", result);
            ctx.setOutput("pelhv id", Workload("pelhv id", NodeType::Field));
            ctx.setOutput("pelhv label", Workload("pelhv label", NodeType::Field));
        })
        .buildAndRegister();
}
```

---

## Usage Example

```cpp
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeExecutor.hpp"
#include "plugin_init.hpp"  // auto-generated by CMake

// Register all nodes from all plugins
registerNodePlugins();

// Build graph: csv_source -> add(field(price), int(10))
NodeGraph graph;
auto csv = graph.addNode("csv_source");
auto field = graph.addNode("field");
auto intVal = graph.addNode("int_value");
auto add = graph.addNode("add");

graph.setProperty(field, "_column", Workload("price", Type::String));
graph.setProperty(intVal, "_value", Workload(int64_t(10), Type::Int));

graph.connect(csv, "csv", add, "csv");
graph.connect(field, "field", add, "src");
graph.connect(intVal, "value", add, "operand");

// Execute
NodeExecutor exec(NodeRegistry::instance());
auto results = exec.execute(graph);

// Result: price column overwritten with prices + 10
auto resultCsv = results[add]["csv"].getCsv();
// price column: [11.50, 10.75, 12.00, 13.50]
```

## Tests

```bash
make node_tests && ./node_tests
```

---

## Related Documentation

- [Node Catalog](CATALOG.md) - Complete reference of all built-in nodes
- [Scalar Nodes](SCALAR-NODES.md) - Scalar input nodes with parameter override support
- [Dynamic Nodes](DYNAMIC-NODES.md) - Runtime equation injection system
- [Implementation Checklist](IMPLEMENTATION-CHECKLIST.md) - Guide for adding new nodes
- [PostgreSQL Integration](../postgresql/INTEGRATION.md) - DynRequest details and database nodes
