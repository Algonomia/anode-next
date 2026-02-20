# New Node Implementation Checklist

This document lists the points to verify when implementing a new node to avoid common bugs.

## Files to Modify

For a new node, you generally need to modify:

1. **`src/nodes/nodes/<plugin>/<Category>Nodes.cpp`** - Node backend logic
2. **`src/nodes/nodes/<plugin>/<Category>Nodes.hpp`** - Registration function declaration
3. **`src/nodes/nodes/<plugin>/register.cpp`** - Call the function in `registerNodes()`
4. **`src/client/src/graph/nodeTypes.ts`** - UI configuration (widgets, dynamic inputs)
5. **`src/client/src/graph/conversion.ts`** - If the node has dynamic inputs
6. **`src/nodes/nodes/<plugin>/tests/<Category>NodesTest.cpp`** or **`tests/<Category>NodesTest.cpp`** - Unit tests

Plugins are subdirectories of `src/nodes/nodes/` (e.g., `common/`, etc.). See [PLUGINS.md](../architecture/PLUGINS.md) to create a new plugin.

---

## Checklist

### 1. Backend (C++)

- [ ] Create the `register<NodeName>Node()` function in the `.cpp` file
- [ ] Declare the function in the `.hpp` file
- [ ] Call the function in `register<Category>Nodes()`
- [ ] Define all inputs with `.input()` or `.inputOptional()`
- [ ] Define all outputs with `.output()`
- [ ] Implement the logic in `.onCompile()`
- [ ] Handle errors with `ctx.setError()`

### 2. Frontend (TypeScript)

#### Widgets with Connectable Inputs

If the node has a widget whose value can be replaced by a connection:

```typescript
case 'my_node':
  addWidgetWithInput(node, 'text', '_name', 'default_value', 'string');
  break;
```

**IMPORTANT**: The `addWidgetWithInput` function automatically initializes `node.properties[name]` with the default value. This ensures that:
- The property is always serialized
- The server can use this value as a fallback when nothing is connected

#### Dynamic Inputs (+/- buttons)

If the node allows dynamically adding/removing inputs (like `select_by_name`, `group`, `reorder_columns`):

1. **In `nodeTypes.ts`**: Create a `setup<NodeName>Node()` function that:
   - Removes the dynamic inputs from the catalog (they will be added manually)
   - Initializes a `node._visibleXxxCount` counter
   - Adds the +/- buttons with callbacks
   - Implements `node.restoreXxxInputs(count)` for restoration

2. **In `conversion.ts`**: Add restoration on load:

   ```typescript
   // In anodeToLitegraph(), "Restore dynamic inputs" section
   if (anode.type.includes('my_node') && anode.properties._visibleXxxCount) {
     if (lgNode.restoreXxxInputs) {
       lgNode.restoreXxxInputs(anode.properties._visibleXxxCount.value as number);
     }
   }
   ```

   **WARNING**: Without this step, dynamic inputs added by the user will be lost when the graph is reloaded!

3. Counter serialization is automatic if you use a property like `_visibleXxxCount` on the node.

### 3. Tests

- [ ] Test that the node is properly registered
- [ ] Test nominal behavior
- [ ] Test error cases (missing inputs, invalid values)
- [ ] For dynamic inputs: test with multiple inputs

---

## Common Patterns

### Pattern: Dynamic Inputs (column_1, column_2, ...)

Complete example with `reorder_columns`:

**Backend (`SelectNodes.cpp`)**:
```cpp
void registerReorderColumnsNode() {
    auto builder = NodeBuilder("reorder_columns", "select")
        .input("csv", Type::Csv)
        .input("column", Type::Field);  // First input (required)

    // Additional inputs (optional)
    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("column_" + std::to_string(i), Type::Field);
    }

    builder.output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // ... logic
        })
        .buildAndRegister();
}
```

**Frontend (`nodeTypes.ts`)**:
```typescript
function setupReorderColumnsNode(node: AnodeLGraphNode): void {
  // Remove column_* inputs from the catalog
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('column_')) as any;
  }

  // Counter (starts at 1 = just "column")
  node._visibleColumnCount = 1;

  // + button
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleColumnCount ?? 1) < 100) {
      const newInputName = 'column_' + node._visibleColumnCount;
      node.addInput(newInputName, 'field');
      node._visibleColumnCount = (node._visibleColumnCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // - button
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

  // Restoration method (MANDATORY)
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

**Frontend (`conversion.ts`)** - Add in `anodeToLitegraph()`:
```typescript
// my_node node
if (anode.type.includes('reorder_columns') && anode.properties._visibleColumnCount) {
  if (lgNode.restoreColumnInputs) {
    lgNode.restoreColumnInputs(anode.properties._visibleColumnCount.value as number);
  }
}
```

---

## Common Errors

### 1. Dynamic Inputs Lost on Reload

**Symptom**: Inputs added with the + button disappear when the graph is reloaded.

**Cause**: Restoration is not implemented in `conversion.ts`.

**Solution**: Add the restoration code in `anodeToLitegraph()`.

### 2. Widget Fallback Not Working

**Symptom**: When connecting then disconnecting a node from a widget input, the widget value is not used.

**Cause**: The property was not initialized with the default value.

**Solution**: `addWidgetWithInput` now automatically initializes the property. If you create a widget manually, make sure to initialize `node.properties[name] = defaultValue`.

### 3. Node Not Found on Load

**Symptom**: The node appears as "unknown" or is not created.

**Cause**: The registration function is not called.

**Solution**: Verify that `register<NodeName>Node()` is called in `register<Category>Nodes()`.

### 4. Node Type Detection Fails (Format with Category)

**Symptom**: Logic that detects a node type by its name does not work (e.g., detecting `label_define_*` nodes).

**Cause**: The `definitionName` on the backend side is `label_define_int`, but the `type` sent by the frontend includes the category: `label/label_define_int`.

**Solution**: Use `find() != std::string::npos` instead of `find() == 0`:

```cpp
// CORRECT
if (instance.definitionName.find("label_define_") != std::string::npos)

// INCORRECT - does not match "label/label_define_int"
if (instance.definitionName.find("label_define_") == 0)
```

---

## Quick Reference for Counter Properties

| Node | Property | Restoration Method |
|------|----------|--------------------|
| `group` | `_visibleFieldCount` | `restoreFieldInputs` |
| `tree_group` | `_visibleFieldCount` | `restoreFieldInputs` |
| `pivot` | `_visibleIndexColumnCount` | `restoreIndexColumnInputs` |
| `select_by_name` | `_visibleColumnCount` | `restoreColumnInputs` |
| `select_by_pos` | `_visibleColCount` | `restoreColInputs` |
| `reorder_columns` | `_visibleColumnCount` | `restoreColumnInputs` |
| `scalars_to_csv` | `_visiblePairCount` | `restorePairInputs` |
| `string_as_fields` | `_visibleValueCount` | `restoreValueInputs` |
