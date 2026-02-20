/**
 * LiteGraphAdapter - Adapter between Anode graph format and LiteGraph.js
 */

import type {
  NodeCatalogResponse,
  NodeDef,
  AnodeGraph,
  AnodeNode,
  AnodeConnection,
} from './AnodeClient';

// LiteGraph types (from litegraph.js global)
declare const LiteGraph: {
  registerNodeType(type: string, nodeClass: unknown): void;
  createNode(type: string): LGraphNode | null;
};

interface LGraphNode {
  id: number;
  type: string;
  pos: [number, number];
  properties: Record<string, unknown>;
  inputs: Array<{ name: string; type: string; link: number | null }>;
  outputs: Array<{ name: string; type: string; links: number[] }>;
  addInput(name: string, type: string): void;
  addOutput(name: string, type: string): void;
  addWidget(
    type: string,
    name: string,
    value: unknown,
    callback?: (value: unknown) => void,
    options?: Record<string, unknown>
  ): void;
  findInputSlot(name: string): number;
  findOutputSlot(name: string): number;
  connect(outputSlot: number, targetNode: LGraphNode, inputSlot: number): void;
}

interface LGraphLink {
  id: number;
  origin_id: number;
  origin_slot: number;
  target_id: number;
  target_slot: number;
}

interface LGraph {
  _nodes: LGraphNode[];
  links: Record<number, LGraphLink>;
  clear(): void;
  add(node: LGraphNode): void;
  getNodeById(id: number): LGraphNode | null;
}

// ID mapping types
type ReverseIdMap = Map<string, number>;

/**
 * Register all node types from the catalog into LiteGraph
 */
export function registerNodeTypes(catalog: NodeCatalogResponse): void {
  for (const def of catalog.nodes) {
    const nodeType = `${def.category}/${def.name}`;

    // Create node class dynamically
    const nodeClass = createNodeClass(def);

    LiteGraph.registerNodeType(nodeType, nodeClass);
  }
}

/**
 * Create a LiteGraph node class from a NodeDef
 */
function createNodeClass(def: NodeDef): new () => LGraphNode {
  // Using a named function for better debugging
  const NodeClass = function (this: LGraphNode) {
    // Add inputs
    for (const inp of def.inputs) {
      const typeStr = inp.types.join(',');
      this.addInput(inp.name, typeStr);
    }

    // Add outputs
    for (const out of def.outputs) {
      const typeStr = out.types.join(',');
      this.addOutput(out.name, typeStr);
    }

    // Initialize properties
    this.properties = {};

    // Add widgets based on node type
    addWidgetsForNode(this, def);
  } as unknown as new () => LGraphNode;

  // Set static properties
  (NodeClass as unknown as { title: string }).title = def.name;

  // Add prototype methods
  (NodeClass.prototype as { onExecute: () => void }).onExecute = function () {
    // No-op: execution happens on the backend
  };

  return NodeClass;
}

/**
 * Add appropriate widgets based on the node definition
 */
function addWidgetsForNode(node: LGraphNode, def: NodeDef): void {
  // Known property widgets by node type
  switch (def.name) {
    case 'int_value':
      node.addWidget('number', '_value', 0, (v) => {
        node.properties._value = v;
      });
      break;

    case 'double_value':
      node.addWidget('number', '_value', 0.0, (v) => {
        node.properties._value = v;
      });
      break;

    case 'string_value':
      node.addWidget('text', '_value', '', (v) => {
        node.properties._value = v;
      });
      break;

    case 'bool_value':
      node.addWidget('toggle', '_value', false, (v) => {
        node.properties._value = v;
      });
      break;

    case 'field':
      node.addWidget('text', '_column', '', (v) => {
        node.properties._column = v;
      });
      break;

    case 'csv_source':
      node.addWidget('text', '_path', '', (v) => {
        node.properties._path = v;
      });
      break;

    default:
      // For unknown nodes, no default widgets
      break;
  }
}

/**
 * Convert an Anode graph to LiteGraph format
 * @returns Map from anode node ID to LiteGraph node ID
 */
export function anodeToLitegraph(
  anodeGraph: AnodeGraph,
  lgraph: LGraph
): ReverseIdMap {
  lgraph.clear();
  const idMap: ReverseIdMap = new Map();

  // Create nodes
  for (const anode of anodeGraph.nodes) {
    // Try with category prefix first, then without
    let lgNode = LiteGraph.createNode(anode.type);
    if (!lgNode) {
      // Try to find with any category
      lgNode = findNodeByName(anode.type);
    }

    if (!lgNode) {
      console.warn(`Unknown node type: ${anode.type}`);
      continue;
    }

    // Copy properties (convert anode format -> litegraph)
    for (const [key, prop] of Object.entries(anode.properties)) {
      lgNode.properties[key] = prop.value;

      // Update widget if exists
      updateWidgetValue(lgNode, key, prop.value);
    }

    // Set position if available
    if (anode.position) {
      lgNode.pos = [anode.position[0], anode.position[1]];
    }

    lgraph.add(lgNode);
    idMap.set(anode.id, lgNode.id);
  }

  // Create connections
  for (const conn of anodeGraph.connections) {
    const fromLgId = idMap.get(conn.from);
    const toLgId = idMap.get(conn.to);
    if (fromLgId === undefined || toLgId === undefined) {
      console.warn(`Connection references unknown node: ${conn.from} -> ${conn.to}`);
      continue;
    }

    const fromNode = lgraph.getNodeById(fromLgId);
    const toNode = lgraph.getNodeById(toLgId);
    if (!fromNode || !toNode) continue;

    const fromSlot = fromNode.findOutputSlot(conn.fromPort);
    const toSlot = toNode.findInputSlot(conn.toPort);
    if (fromSlot === -1 || toSlot === -1) {
      console.warn(`Port not found: ${conn.fromPort} or ${conn.toPort}`);
      continue;
    }

    fromNode.connect(fromSlot, toNode, toSlot);
  }

  return idMap;
}

/**
 * Try to find a node type by name (without category prefix)
 */
function findNodeByName(name: string): LGraphNode | null {
  // Common categories to try
  const categories = ['scalar', 'data', 'math', 'csv', 'logic', 'io'];
  for (const cat of categories) {
    const node = LiteGraph.createNode(`${cat}/${name}`);
    if (node) return node;
  }
  return null;
}

/**
 * Update a widget value if it exists
 */
function updateWidgetValue(node: LGraphNode, propName: string, value: unknown): void {
  const widgets = (node as unknown as { widgets?: Array<{ name: string; value: unknown }> }).widgets;
  if (widgets) {
    const widget = widgets.find((w) => w.name === propName);
    if (widget) {
      widget.value = value;
    }
  }
}

/**
 * Convert a LiteGraph graph to Anode format
 */
export function litegraphToAnode(lgraph: LGraph): AnodeGraph {
  const nodes: AnodeNode[] = [];
  const connections: AnodeConnection[] = [];

  // Export nodes
  for (const lgNode of lgraph._nodes) {
    const anodeNode: AnodeNode = {
      id: `node_${lgNode.id}`,
      type: lgNode.type,
      properties: {},
      position: [lgNode.pos[0], lgNode.pos[1]],
    };

    // Convert properties (litegraph -> anode format)
    for (const [key, value] of Object.entries(lgNode.properties || {})) {
      anodeNode.properties[key] = {
        value: value,
        type: inferType(value),
      };
    }

    nodes.push(anodeNode);
  }

  // Export connections via links
  for (const link of Object.values(lgraph.links || {})) {
    if (!link) continue;

    const fromNode = lgraph.getNodeById(link.origin_id);
    const toNode = lgraph.getNodeById(link.target_id);
    if (!fromNode || !toNode) continue;

    const fromPort = fromNode.outputs[link.origin_slot]?.name;
    const toPort = toNode.inputs[link.target_slot]?.name;
    if (!fromPort || !toPort) continue;

    connections.push({
      from: `node_${link.origin_id}`,
      fromPort,
      to: `node_${link.target_id}`,
      toPort,
    });
  }

  return { nodes, connections };
}

/**
 * Infer the Anode type from a JavaScript value
 */
function inferType(value: unknown): string {
  if (value === null || value === undefined) return 'null';
  if (typeof value === 'boolean') return 'bool';
  if (typeof value === 'number') {
    return Number.isInteger(value) ? 'int' : 'double';
  }
  if (typeof value === 'string') return 'string';
  return 'string';
}
