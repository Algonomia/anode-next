/**
 * Node type registration for LiteGraph
 */

import { LiteGraph, LGraphNode, type LGraphCanvas, type LGraph, Subgraph, SubgraphNode } from '@comfyorg/litegraph';
import type { NodeCatalogResponse, NodeDef } from '../api/AnodeClient';
import { getSlotColor } from './colors';
import type { AnodeLGraphNode, ColoredOutputSlot, NodeProperty } from '../types/litegraph';
import { getNodeStatus, type NodeExecutionStatus } from '../execution/state';
import { openCsvEditor, type CsvData } from '../ui/csvEditor';

// Status indicator colors
const STATUS_COLORS: Record<NodeExecutionStatus, string> = {
  idle: '#666666',    // Gray
  running: '#FFC107', // Yellow/Amber
  success: '#4CAF50', // Green
  error: '#F44336',   // Red
};

const INDICATOR_RADIUS = 5;
const INDICATOR_MARGIN_LEFT = 8;

// Reference to graphCanvas for redrawing
let _graphCanvas: LGraphCanvas | null = null;
let _graph: LGraph | null = null;

export function setGraphCanvas(canvas: LGraphCanvas | null): void {
  _graphCanvas = canvas;
}

// Callback when navigating in/out of subgraphs
let _onSubgraphChange: (() => void) | null = null;

/**
 * Set callback for when subgraph navigation changes
 */
export function setSubgraphChangeCallback(callback: () => void): void {
  _onSubgraphChange = callback;
}

/**
 * Setup subgraph support for a graph
 * Patches LiteGraph.createNode to handle subgraph UUIDs
 */
export function setupSubgraphSupport(graph: LGraph): void {
  _graph = graph;

  // Patch createNode to handle subgraph types
  const originalCreateNode = LiteGraph.createNode.bind(LiteGraph);

  (LiteGraph as any).createNode = function(type: string, title?: string, options?: any) {
    // Check if this is a subgraph UUID (looks like a UUID pattern)
    const isUuid = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(type);

    if (isUuid && _graph) {
      // Look up the subgraph in the graph's subgraphs map (check both graph and rootGraph)
      const rootGraph = (_graph as any).rootGraph || _graph;
      const subgraphs = (rootGraph as any).subgraphs as Map<string, Subgraph> | undefined;
      const subgraph = subgraphs?.get(type);

      if (subgraph) {
        // Create SubgraphNode directly
        const node = new SubgraphNode(_graph as any, subgraph, {
          type,
          id: 0, // Will be assigned by graph.add()
          pos: [0, 0],
          size: [200, 100],
          flags: {},
          mode: 0,
          ...options
        });
        if (title) node.title = title;

        // Style the subgraph node
        styleSubgraphNode(node);

        return node;
      }
    }

    // Fall back to original createNode
    return originalCreateNode(type, title, options);
  };

  // Patch canvas.openSubgraph to trigger callback
  if (_graphCanvas) {
    const originalOpenSubgraph = (_graphCanvas as any).openSubgraph?.bind(_graphCanvas);
    if (originalOpenSubgraph) {
      (_graphCanvas as any).openSubgraph = function(subgraph: Subgraph) {
        originalOpenSubgraph(subgraph);
        if (_onSubgraphChange) _onSubgraphChange();
      };
    }

    // Patch canvas.setGraph to trigger callback when exiting subgraph
    const originalSetGraph = (_graphCanvas as any).setGraph?.bind(_graphCanvas);
    if (originalSetGraph) {
      (_graphCanvas as any).setGraph = function(newGraph: any, ...args: any[]) {
        originalSetGraph(newGraph, ...args);
        if (_onSubgraphChange) _onSubgraphChange();
      };
    }
  }
}

/**
 * Style a SubgraphNode to make it visually distinct
 */
function styleSubgraphNode(node: SubgraphNode): void {
  // Set a distinct color for subgraph nodes
  (node as any).color = '#2a4858';  // Darker teal background
  (node as any).bgcolor = '#1a3040'; // Even darker background
  (node as any).boxcolor = '#4a9ead'; // Teal border/box color

  // Add a visual indicator in the title
  const originalTitle = node.title;
  node.title = `📦 ${originalTitle}`;
}

/**
 * Register all node types from the catalog
 */
export function registerNodeTypes(catalog: NodeCatalogResponse): void {
  // Clear all default LiteGraph node types
  (LiteGraph as any).registered_node_types = {};
  (LiteGraph as any).searchbox_extras = {};

  for (const def of catalog.nodes) {
    const nodeType = `${def.category}/${def.name}`;
    const nodeClass = createNodeClass(def);
    LiteGraph.registerNodeType(nodeType, nodeClass);
  }
}

/**
 * Create a node class from a node definition
 */
function createNodeClass(def: NodeDef): typeof LGraphNode {
  // Create a class that extends LGraphNode
  class CustomNode extends LGraphNode {
    _anodeId?: string;
    _visibleFieldCount?: number;
    restoreFieldInputs?: (count: number) => void;

    constructor() {
      super(def.name);

      // Add inputs (gray by default, colored when connected)
      for (const inp of def.inputs) {
        this.addInput(inp.name, inp.types.join(','));
      }

      // Add outputs with colors
      for (const out of def.outputs) {
        this.addOutput(out.name, out.types.join(','));
        const color = getSlotColor(out.types);
        const outputs = this.outputs as ColoredOutputSlot[] | undefined;
        if (color && outputs && outputs.length > 0) {
          const lastOutput = outputs[outputs.length - 1];
          lastOutput.color_on = color;
          lastOutput.color_off = color;
        }
      }

      // Add widgets
      addWidgets(this as AnodeLGraphNode, def.name);
    }

    onExecute() {
      // No-op: execution happens on the backend
    }

    /**
     * Draw execution status indicator on the node
     */
    onDrawForeground(ctx: CanvasRenderingContext2D) {
      const anodeId = (this as any)._anodeId || `node_${this.id}`;
      const state = getNodeStatus(anodeId);

      // Get title height from LiteGraph (default is 30)
      const titleHeight = (LiteGraph as any).NODE_TITLE_HEIGHT || 30;

      // Position: top-left corner of the title bar
      const x = INDICATOR_MARGIN_LEFT + INDICATOR_RADIUS;
      const y = -titleHeight / 2;

      // Draw status circle
      ctx.save();
      ctx.beginPath();
      ctx.arc(x, y, INDICATOR_RADIUS, 0, Math.PI * 2);
      ctx.fillStyle = STATUS_COLORS[state.status];
      ctx.fill();

      // Add glow effect for running state
      if (state.status === 'running') {
        ctx.shadowColor = STATUS_COLORS.running;
        ctx.shadowBlur = 8;
        ctx.beginPath();
        ctx.arc(x, y, INDICATOR_RADIUS, 0, Math.PI * 2);
        ctx.fill();
      }

      // Draw execution time if completed or failed (positioned at top-right)
      if (state.durationMs !== undefined && state.status !== 'running' && state.status !== 'idle') {
        const timeText = state.durationMs < 1000
          ? `${state.durationMs}ms`
          : `${(state.durationMs / 1000).toFixed(1)}s`;

        ctx.shadowBlur = 0;
        ctx.font = '9px sans-serif';
        ctx.fillStyle = '#aaa';

        // Position at the right side of the title bar
        const nodeWidth = this.size[0];
        const textWidth = ctx.measureText(timeText).width;
        const rightX = nodeWidth - textWidth - 8; // 8px padding from right edge
        ctx.fillText(timeText, rightX, y + 3);
      }

      // Draw error message below the node if status is error
      if (state.status === 'error' && state.errorMessage) {
        ctx.shadowBlur = 0;
        ctx.font = '10px sans-serif';

        const nodeWidth = this.size[0];
        const nodeHeight = this.size[1];
        const padding = 6;
        const maxWidth = Math.max(nodeWidth, 200);

        // Word wrap the error message
        const words = state.errorMessage.split(' ');
        const lines: string[] = [];
        let currentLine = '';

        for (const word of words) {
          const testLine = currentLine ? `${currentLine} ${word}` : word;
          const testWidth = ctx.measureText(testLine).width;
          if (testWidth > maxWidth - padding * 2 && currentLine) {
            lines.push(currentLine);
            currentLine = word;
          } else {
            currentLine = testLine;
          }
        }
        if (currentLine) lines.push(currentLine);

        // Limit to 3 lines max
        if (lines.length > 3) {
          lines.length = 3;
          lines[2] = lines[2].slice(0, -3) + '...';
        }

        const lineHeight = 14;
        const boxHeight = lines.length * lineHeight + padding * 2;
        const boxWidth = Math.min(maxWidth, Math.max(...lines.map(l => ctx.measureText(l).width)) + padding * 2);

        // Draw error background
        const boxX = 0;
        const boxY = nodeHeight + 4;

        ctx.fillStyle = 'rgba(244, 67, 54, 0.9)';
        ctx.beginPath();
        ctx.roundRect(boxX, boxY, boxWidth, boxHeight, 4);
        ctx.fill();

        // Draw error text
        ctx.fillStyle = '#fff';
        for (let i = 0; i < lines.length; i++) {
          ctx.fillText(lines[i], boxX + padding, boxY + padding + 10 + i * lineHeight);
        }
      }

      ctx.restore();
    }
  }

  // Set static title
  (CustomNode as any).title = def.name;

  return CustomNode as typeof LGraphNode;
}

// Widget types supported by litegraph
type WidgetType = 'number' | 'text' | 'toggle' | 'combo' | 'button' | 'slider' | 'knob';

/**
 * Add a widget with an associated input slot
 * When the input is connected, the widget is disabled and the connected value is used
 */
function addWidgetWithInput(
  node: AnodeLGraphNode,
  widgetType: WidgetType,
  name: string,
  defaultValue: string | number | boolean,
  inputType: string
): void {
  // Initialize property with default value
  node.properties[name] = defaultValue as NodeProperty;

  // Add the widget (options.property enables LiteGraph to sync widget value from properties on paste/configure)
  node.addWidget(widgetType, name, defaultValue, (v: unknown) => {
    node.properties[name] = v as NodeProperty;
  }, { property: name });

  // Add the input slot with widget association
  node.addInput(name, inputType);
  const inputs = node.inputs as any[] | undefined;
  if (inputs && inputs.length > 0) {
    const lastInput = inputs[inputs.length - 1];
    lastInput.widget = { name };
  }
}

/**
 * Add widgets based on node name
 */
function addWidgets(node: AnodeLGraphNode, nodeName: string): void {
  switch (nodeName) {
    case 'int_value':
      addWidgetWithInput(node, 'number', '_value', 0, 'int');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'double_value':
      addWidgetWithInput(node, 'number', '_value', 0.0, 'double');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'string_value':
      addWidgetWithInput(node, 'text', '_value', '', 'string');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'string_as_field':
      addWidgetWithInput(node, 'text', '_value', '', 'field');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'string_as_fields':
      setupStringAsFieldsNode(node);
      break;

    case 'bool_value':
      addWidgetWithInput(node, 'toggle', '_value', false, 'bool');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'date_value':
      addWidgetWithInput(node, 'text', '_value', '', 'string');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'null_value':
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'current_date':
      addWidgetWithInput(node, 'number', '_year_offset', 0, 'int');
      addWidgetWithInput(node, 'number', '_month_offset', 0, 'int');
      addWidgetWithInput(node, 'number', '_day_offset', 0, 'int');
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'csv_source':
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      break;

    case 'csv_value':
      addWidgetWithInput(node, 'text', '_identifier', '', 'string');
      setupCsvValueNode(node);
      break;

    case 'field':
      addWidgetWithInput(node, 'text', '_column', '', 'string');
      break;

    case 'group':
      setupGroupNode(node);
      break;

    case 'tree_group':
      setupTreeGroupNode(node);
      break;

    case 'pivot':
      setupPivotNode(node);
      break;

    case 'join_flex':
      setupJoinFlexNode(node);
      break;

    case 'json_extract':
      node.properties._on_failure = 'identity' as NodeProperty;
      node.addWidget(
        'combo',
        '_on_failure',
        'identity',
        (v: unknown) => {
          node.properties._on_failure = v as NodeProperty;
        },
        { values: ['identity', 'blank'], property: '_on_failure' }
      );
      break;

    case 'output':
      addWidgetWithInput(node, 'text', '_name', 'my_output', 'string');
      break;

    case 'timeline_output':
      addWidgetWithInput(node, 'text', '_timeline_name', 'my_timeline', 'string');
      break;

    case 'diff_output':
      addWidgetWithInput(node, 'text', '_diff_name', 'my_diff', 'string');
      break;

    case 'bar_chart_output':
      addWidgetWithInput(node, 'text', '_chart_name', 'my_chart', 'string');
      break;

    case 'scalars_to_csv':
      setupScalarsToCsvNode(node);
      break;

    case 'concat':
      setupConcatNode(node, 'suffix');
      break;

    case 'concat_prefix':
      setupConcatNode(node, 'prefix');
      break;

    case 'remap_by_name':
      setupRemapByNameNode(node);
      break;

    case 'remap_by_csv':
      node.properties._unmapped = 'keep' as NodeProperty;
      node.addWidget(
        'combo',
        '_unmapped',
        'keep',
        (v: unknown) => {
          node.properties._unmapped = v as NodeProperty;
        },
        { values: ['keep', 'remove'], property: '_unmapped' }
      );
      break;

    case 'select_by_name':
      setupSelectByNameNode(node);
      break;

    case 'select_by_pos':
      setupSelectByPosNode(node);
      break;

    case 'reorder_columns':
      setupReorderColumnsNode(node);
      break;

    case 'dynamic_begin':
    case 'dynamic_end':
      addWidgetWithInput(node, 'text', '_name', 'dynamic', 'string');
      break;

    // Label nodes - all need a _label widget (not _identifier to avoid conflict with scalar override)
    case 'label_define_csv':
    case 'label_define_field':
    case 'label_define_int':
    case 'label_define_double':
    case 'label_define_string':
    case 'label_ref_csv':
    case 'label_ref_field':
    case 'label_ref_int':
    case 'label_ref_double':
    case 'label_ref_string':
      // Initialize property FIRST with default value (ensures serialization)
      node.properties._label = '' as NodeProperty;
      // Then add the widget
      node.addWidget('text', '_label', '', (v: unknown) => {
        node.properties._label = v as NodeProperty;
      }, { property: '_label' });
      break;
  }
}

/**
 * Setup the group node with dynamic field inputs
 */
function setupGroupNode(node: AnodeLGraphNode): void {
  node.addWidget(
    'combo',
    '_aggregation',
    'sum',
    (v: unknown) => {
      node.properties._aggregation = v as NodeProperty;
    },
    { values: ['sum', 'avg', 'min', 'max', 'first', 'count'], property: '_aggregation' }
  );

  // Remove all field_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('field_')) as any;
  }

  // Track visible field count (starts at 1 = just "field")
  node._visibleFieldCount = 1;

  // Button + to add a field
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleFieldCount ?? 1) < 100) {
      const newInputName = 'field_' + node._visibleFieldCount;
      node.addInput(newInputName, 'field');
      node._visibleFieldCount = (node._visibleFieldCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove a field
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleFieldCount ?? 1) > 1) {
      const inputName = 'field_' + ((node._visibleFieldCount ?? 1) - 1);
      const slot = node.findInputSlot(inputName);
      if (slot !== -1) {
        // Disconnect if connected
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[slot]?.link) {
          node.disconnectInput(slot);
        }
        // Remove the input
        node.removeInput(slot);
      }
      node._visibleFieldCount = (node._visibleFieldCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Method to restore inputs (used when loading)
  node.restoreFieldInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      // Remove existing field_* inputs
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith('field_')) {
          this.removeInput(i);
        }
      }
    }
    // Add the right number of field_* inputs
    for (let i = 1; i < count; i++) {
      this.addInput('field_' + i, 'field');
    }
    this._visibleFieldCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup the tree_group node (same dynamic field pattern as group, for AG Grid tree data)
 */
function setupTreeGroupNode(node: AnodeLGraphNode): void {
  node.addWidget(
    'combo',
    '_aggregation',
    'sum',
    (v: unknown) => {
      node.properties._aggregation = v as NodeProperty;
    },
    { values: ['sum', 'avg', 'min', 'max', 'first', 'count'], property: '_aggregation' }
  );

  // Remove all field_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('field_')) as any;
  }

  node._visibleFieldCount = 1;

  node.addWidget('button', '+', '+', () => {
    if ((node._visibleFieldCount ?? 1) < 100) {
      const newInputName = 'field_' + node._visibleFieldCount;
      node.addInput(newInputName, 'field');
      node._visibleFieldCount = (node._visibleFieldCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  node.addWidget('button', '-', '-', () => {
    if ((node._visibleFieldCount ?? 1) > 1) {
      const inputName = 'field_' + ((node._visibleFieldCount ?? 1) - 1);
      const slot = node.findInputSlot(inputName);
      if (slot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[slot]?.link) {
          node.disconnectInput(slot);
        }
        node.removeInput(slot);
      }
      node._visibleFieldCount = (node._visibleFieldCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  node.restoreFieldInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith('field_')) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 1; i < count; i++) {
      this.addInput('field_' + i, 'field');
    }
    this._visibleFieldCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Compile _field_0, _field_1, ... into _value JSON array string
 */
function syncFieldsToValue(node: LGraphNode): void {
  const fields: string[] = [];
  for (let i = 0; i < ((node as any)._visibleValueCount ?? 1); i++) {
    const val = node.properties['_field_' + i];
    if (typeof val === 'string' && val.length > 0) {
      fields.push(val);
    }
  }
  node.properties._value = JSON.stringify(fields) as NodeProperty;
}

/**
 * Setup the string_as_fields node with dynamic field name widgets
 */
function setupStringAsFieldsNode(node: AnodeLGraphNode): void {
  // _identifier widget
  addWidgetWithInput(node, 'text', '_identifier', '', 'string');

  // Track how many field widgets are visible
  node._visibleValueCount = 1;

  // First field widget (always visible)
  node.addWidget('text', '_field_0', '', (v: unknown) => {
    node.properties._field_0 = v as NodeProperty;
    syncFieldsToValue(node);
  }, { property: '_field_0' });

  // Button + to add a field
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleValueCount ?? 1) < 100) {
      const idx = node._visibleValueCount!;
      node.addWidget('text', '_field_' + idx, '', (v: unknown) => {
        node.properties['_field_' + idx] = v as NodeProperty;
        syncFieldsToValue(node);
      }, { property: '_field_' + idx });
      node._visibleValueCount = (node._visibleValueCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
      syncFieldsToValue(node);
    }
  });

  // Button - to remove a field
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleValueCount ?? 1) > 1) {
      node._visibleValueCount = (node._visibleValueCount ?? 1) - 1;
      const idx = node._visibleValueCount!;
      // Remove the last widget
      delete node.properties['_field_' + idx];
      const widgets = node.widgets as any[];
      for (let i = widgets.length - 1; i >= 0; i--) {
        if (widgets[i].name === '_field_' + idx) {
          widgets.splice(i, 1);
          break;
        }
      }
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
      syncFieldsToValue(node);
    }
  });

  // Method to restore widgets (used when loading)
  node.restoreValueInputs = function (count: number) {
    for (let i = 1; i < count; i++) {
      const idx = i;
      this.addWidget('text', '_field_' + idx, '', (v: unknown) => {
        this.properties['_field_' + idx] = v as NodeProperty;
        syncFieldsToValue(this);
      }, { property: '_field_' + idx });
    }
    this._visibleValueCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup the pivot node with dynamic index column inputs
 */
function setupPivotNode(node: AnodeLGraphNode): void {
  // Widget for optional prefix
  node.addWidget('text', '_prefix', '', (v: unknown) => {
    node.properties._prefix = v as NodeProperty;
  }, { property: '_prefix' });

  // Remove all index_column_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('index_column_')) as any;
  }

  // Track visible index column count (starts at 1 = just "index_column")
  node._visibleIndexColumnCount = 1;

  // Button + to add an index column
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleIndexColumnCount ?? 1) < 100) {
      const newInputName = 'index_column_' + node._visibleIndexColumnCount;
      node.addInput(newInputName, 'field');
      node._visibleIndexColumnCount = (node._visibleIndexColumnCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove an index column
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleIndexColumnCount ?? 1) > 1) {
      const inputName = 'index_column_' + ((node._visibleIndexColumnCount ?? 1) - 1);
      const slot = node.findInputSlot(inputName);
      if (slot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[slot]?.link) {
          node.disconnectInput(slot);
        }
        node.removeInput(slot);
      }
      node._visibleIndexColumnCount = (node._visibleIndexColumnCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Method to restore inputs (used when loading)
  node.restoreIndexColumnInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith('index_column_')) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 1; i < count; i++) {
      this.addInput('index_column_' + i, 'field');
    }
    this._visibleIndexColumnCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup the select_by_name node with dynamic column inputs
 */
function setupSelectByNameNode(node: AnodeLGraphNode): void {
  // Remove all column_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('column_')) as any;
  }

  // Track visible column count (starts at 1 = just "column")
  node._visibleColumnCount = 1;

  // Button + to add a column
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleColumnCount ?? 1) < 100) {
      const newInputName = 'column_' + node._visibleColumnCount;
      node.addInput(newInputName, 'field');
      node._visibleColumnCount = (node._visibleColumnCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove a column
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

  // Method to restore inputs (used when loading)
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

/**
 * Setup the scalars_to_csv node with dynamic field/value pair inputs
 */
function setupScalarsToCsvNode(node: AnodeLGraphNode): void {
  // Remove all field_* and value_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) =>
      !inp.name.match(/^field_\d+$/) && !inp.name.match(/^value_\d+$/)
    ) as any;
  }

  // Track visible pair count (starts at 1 = just "field"/"value")
  node._visiblePairCount = 1;

  // Button + to add a field/value pair
  node.addWidget('button', '+', '+', () => {
    if ((node._visiblePairCount ?? 1) < 100) {
      const idx = node._visiblePairCount;
      node.addInput('field_' + idx, 'field,string');
      node.addInput('value_' + idx, 'int,double,string,bool,null');
      node._visiblePairCount = (node._visiblePairCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove a field/value pair
  node.addWidget('button', '-', '-', () => {
    if ((node._visiblePairCount ?? 1) > 1) {
      const idx = (node._visiblePairCount ?? 1) - 1;

      // Remove value_N first (it's after field_N)
      const valueSlot = node.findInputSlot('value_' + idx);
      if (valueSlot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[valueSlot]?.link) {
          node.disconnectInput(valueSlot);
        }
        node.removeInput(valueSlot);
      }

      // Then remove field_N
      const fieldSlot = node.findInputSlot('field_' + idx);
      if (fieldSlot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[fieldSlot]?.link) {
          node.disconnectInput(fieldSlot);
        }
        node.removeInput(fieldSlot);
      }

      node._visiblePairCount = (node._visiblePairCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Method to restore inputs (used when loading)
  node.restorePairInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.match(/^field_\d+$/) || inputs[i].name.match(/^value_\d+$/)) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 1; i < count; i++) {
      this.addInput('field_' + i, 'field,string');
      this.addInput('value_' + i, 'int,double,string,bool,null');
    }
    this._visiblePairCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup concat / concat_prefix nodes with dynamic suffix/prefix inputs
 */
function setupConcatNode(node: AnodeLGraphNode, kind: 'suffix' | 'prefix'): void {
  // Remove all suffix_*/prefix_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith(kind + '_')) as any;
  }

  // Track visible extra count (starts at 1 = just the base "suffix"/"prefix")
  node._visibleConcatCount = 1;

  // Button + to add an input
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleConcatCount ?? 1) < 100) {
      const idx = node._visibleConcatCount;
      node.addInput(kind + '_' + idx, 'int,double,string,field');
      node._visibleConcatCount = (node._visibleConcatCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove an input
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleConcatCount ?? 1) > 1) {
      const idx = (node._visibleConcatCount ?? 1) - 1;
      const slot = node.findInputSlot(kind + '_' + idx);
      if (slot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[slot]?.link) {
          node.disconnectInput(slot);
        }
        node.removeInput(slot);
      }
      node._visibleConcatCount = (node._visibleConcatCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Method to restore inputs (used when loading)
  node.restoreConcatInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith(kind + '_')) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 1; i < count; i++) {
      this.addInput(kind + '_' + i, 'int,double,string,field');
    }
    this._visibleConcatCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup the remap_by_name node with dynamic col/dest pair inputs
 */
function setupRemapByNameNode(node: AnodeLGraphNode): void {
  // Widget for unmapped columns behavior
  node.properties._unmapped = 'keep' as NodeProperty;
  node.addWidget(
    'combo',
    '_unmapped',
    'keep',
    (v: unknown) => {
      node.properties._unmapped = v as NodeProperty;
    },
    { values: ['keep', 'remove'], property: '_unmapped' }
  );

  // Remove all col_* and dest_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) =>
      !inp.name.startsWith('col_') && !inp.name.startsWith('dest_')
    ) as any;
  }

  // Track visible pair count (starts at 1 = just "col"/"dest")
  node._visibleRemapCount = 1;

  // Button + to add a col/dest pair
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleRemapCount ?? 1) < 100) {
      const idx = node._visibleRemapCount;
      node.addInput('col_' + idx, 'field');
      node.addInput('dest_' + idx, 'field');
      node._visibleRemapCount = (node._visibleRemapCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove a col/dest pair
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleRemapCount ?? 1) > 1) {
      const idx = (node._visibleRemapCount ?? 1) - 1;

      // Remove dest_N first (it's after col_N)
      const destSlot = node.findInputSlot('dest_' + idx);
      if (destSlot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[destSlot]?.link) {
          node.disconnectInput(destSlot);
        }
        node.removeInput(destSlot);
      }

      // Remove col_N
      const colSlot = node.findInputSlot('col_' + idx);
      if (colSlot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[colSlot]?.link) {
          node.disconnectInput(colSlot);
        }
        node.removeInput(colSlot);
      }

      node._visibleRemapCount = (node._visibleRemapCount ?? 1) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Method to restore inputs (used when loading)
  node.restoreRemapInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith('col_') || inputs[i].name.startsWith('dest_')) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 1; i < count; i++) {
      this.addInput('col_' + i, 'field');
      this.addInput('dest_' + i, 'field');
    }
    this._visibleRemapCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup the select_by_pos node with dynamic bool inputs and default widget
 */
function setupSelectByPosNode(node: AnodeLGraphNode): void {
  // Widget for default behavior
  node.addWidget(
    'combo',
    '_default',
    'true',
    (v: unknown) => {
      node.properties._default = v as NodeProperty;
    },
    { values: ['true', 'false'], property: '_default' }
  );

  // Remove all col_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('col_')) as any;
  }

  // Track visible column count (starts at 0)
  node._visibleColCount = 0;

  // Button + to add a column selector
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleColCount ?? 0) < 100) {
      const newInputName = 'col_' + node._visibleColCount;
      node.addInput(newInputName, 'bool');
      node._visibleColCount = (node._visibleColCount ?? 0) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove a column selector
  node.addWidget('button', '-', '-', () => {
    if ((node._visibleColCount ?? 0) > 0) {
      const inputName = 'col_' + ((node._visibleColCount ?? 0) - 1);
      const slot = node.findInputSlot(inputName);
      if (slot !== -1) {
        const inputs = node.inputs as any[] | undefined;
        if (inputs && inputs[slot]?.link) {
          node.disconnectInput(slot);
        }
        node.removeInput(slot);
      }
      node._visibleColCount = (node._visibleColCount ?? 0) - 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Method to restore inputs (used when loading)
  node.restoreColInputs = function (count: number) {
    const inputs = this.inputs as any[] | undefined;
    if (inputs) {
      for (let i = inputs.length - 1; i >= 0; i--) {
        if (inputs[i].name.startsWith('col_')) {
          this.removeInput(i);
        }
      }
    }
    for (let i = 0; i < count; i++) {
      this.addInput('col_' + i, 'bool');
    }
    this._visibleColCount = count;
    this.setSize(this.computeSize());
  };

  node.setSize(node.computeSize());
}

/**
 * Setup the reorder_columns node with dynamic column inputs
 */
function setupReorderColumnsNode(node: AnodeLGraphNode): void {
  // Remove all column_* inputs (they'll be added dynamically)
  const inputs = node.inputs as any[] | undefined;
  if (inputs) {
    node.inputs = inputs.filter((inp: any) => !inp.name.startsWith('column_')) as any;
  }

  // Track visible column count (starts at 1 = just "column")
  node._visibleColumnCount = 1;

  // Button + to add a column
  node.addWidget('button', '+', '+', () => {
    if ((node._visibleColumnCount ?? 1) < 100) {
      const newInputName = 'column_' + node._visibleColumnCount;
      node.addInput(newInputName, 'field');
      node._visibleColumnCount = (node._visibleColumnCount ?? 1) + 1;
      node.setSize(node.computeSize());
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    }
  });

  // Button - to remove a column
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

  // Method to restore inputs (used when loading)
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

/**
 * Setup the join_flex node with mode selectors
 */
function setupJoinFlexNode(node: AnodeLGraphNode): void {
  const modeOptions = ['yes', 'no_but_keep_header', 'no', 'skip'];

  // Widget for no_match mode
  node.addWidget(
    'combo',
    '_no_match_keep_jointure',
    'no_but_keep_header',
    (v: unknown) => {
      node.properties._no_match_keep_jointure = v as NodeProperty;
    },
    { values: modeOptions, property: '_no_match_keep_jointure' }
  );

  // Widget for single_match mode
  node.addWidget(
    'combo',
    '_single_match_keep_jointure',
    'yes',
    (v: unknown) => {
      node.properties._single_match_keep_jointure = v as NodeProperty;
    },
    { values: modeOptions, property: '_single_match_keep_jointure' }
  );

  // Widget for double_match (multiple) mode
  node.addWidget(
    'combo',
    '_double_match_keep_jointure',
    'yes',
    (v: unknown) => {
      node.properties._double_match_keep_jointure = v as NodeProperty;
    },
    { values: modeOptions, property: '_double_match_keep_jointure' }
  );

  node.setSize(node.computeSize());
}

/**
 * Setup the csv_value node with CSV editor button and info display
 */
function setupCsvValueNode(node: AnodeLGraphNode): void {
  // Info widget showing CSV dimensions (read-only text)
  const csvData = node.properties._value as CsvData | undefined;
  const infoText = getCsvInfoText(csvData);

  node.addWidget('text', '_csv_info', infoText, () => {
    // Read-only: reset to current info
    const w = (node as any).widgets?.find((w: any) => w.name === '_csv_info');
    if (w) w.value = getCsvInfoText(node.properties._value as CsvData | undefined);
  }, { property: '_csv_info' });

  // Edit CSV button
  node.addWidget('button', 'Edit CSV', 'Edit CSV', () => {
    const existing = node.properties._value as CsvData | null;
    openCsvEditor(existing, (data: CsvData) => {
      node.properties._value = data as unknown as NodeProperty;
      updateCsvInfoWidget(node);
      if (_graphCanvas) _graphCanvas.setDirty(true, true);
    });
  });

  // Clear button
  node.addWidget('button', 'Clear', 'Clear', () => {
    delete node.properties._value;
    updateCsvInfoWidget(node);
    if (_graphCanvas) _graphCanvas.setDirty(true, true);
  });

  node.setSize(node.computeSize());
}

/**
 * Get CSV info text from data
 */
function getCsvInfoText(data: CsvData | null | undefined): string {
  if (!data || !data.columns || !data.data) return 'No data';
  return `${data.data.length} rows × ${data.columns.length} cols`;
}

/**
 * Update the CSV info widget on a csv_value node
 */
export function updateCsvInfoWidget(node: AnodeLGraphNode): void {
  const widgets = (node as any).widgets as any[] | undefined;
  if (!widgets) return;
  const w = widgets.find((w: any) => w.name === '_csv_info');
  if (w) {
    w.value = getCsvInfoText(node.properties._value as CsvData | undefined);
  }
}
