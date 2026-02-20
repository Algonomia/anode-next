/**
 * In-memory graph state for MCP server
 */

import type { AnodeGraph, AnodeNode, AnodeConnection, NodeDef } from './types.js';
import { emitGraphEvent } from './events.js';

export interface GraphState {
  graph: AnodeGraph;
  slug: string | null;
  name: string | null;
  dirty: boolean;
  lastSessionId: string | null;
}

let nodeCatalog: NodeDef[] = [];

const state: GraphState = {
  graph: { nodes: [], connections: [] },
  slug: null,
  name: null,
  dirty: false,
  lastSessionId: null,
};

let nodeCounter = 0;

function generateNodeId(): string {
  return `node_${++nodeCounter}`;
}

function inferType(value: unknown): string {
  if (typeof value === 'number') {
    return Number.isInteger(value) ? 'int' : 'double';
  }
  if (typeof value === 'boolean') return 'bool';
  if (typeof value === 'string') return 'string';
  return 'string';
}

export function setNodeCatalog(catalog: NodeDef[]): void {
  nodeCatalog = catalog;
}

export function getNodeCatalog(): NodeDef[] {
  return nodeCatalog;
}

export function getNodeDef(type: string): NodeDef | undefined {
  const [category, name] = type.split('/');
  return nodeCatalog.find(n => n.category === category && n.name === name);
}

export function getCurrentGraph(): AnodeGraph {
  return state.graph;
}

export function getGraphState(): GraphState {
  return { ...state };
}

export function newGraph(): void {
  state.graph = { nodes: [], connections: [] };
  state.slug = null;
  state.name = null;
  state.dirty = false;
  state.lastSessionId = null;
  nodeCounter = 0;
  emitGraphEvent({ type: 'graph_cleared' });
}

export function loadGraph(graph: AnodeGraph, slug: string, name: string): void {
  state.graph = graph;
  state.slug = slug;
  state.name = name;
  state.dirty = false;
  state.lastSessionId = null;

  // Update node counter based on existing node IDs
  const maxId = graph.nodes.reduce((max, node) => {
    const match = node.id.match(/^node_(\d+)$/);
    if (match) {
      return Math.max(max, parseInt(match[1], 10));
    }
    return max;
  }, 0);
  nodeCounter = maxId;
  emitGraphEvent({ type: 'graph_loaded', graph, slug, name });
}

export function setSlug(slug: string, name?: string): void {
  state.slug = slug;
  if (name) state.name = name;
}

export function setLastSessionId(sessionId: string): void {
  state.lastSessionId = sessionId;
}

export function getLastSessionId(): string | null {
  return state.lastSessionId;
}

export function markClean(): void {
  state.dirty = false;
}

export function addNode(
  type: string,
  id?: string,
  position?: [number, number],
  properties?: Record<string, unknown>
): AnodeNode {
  const nodeId = id || generateNodeId();

  // Check for duplicate ID
  if (state.graph.nodes.some(n => n.id === nodeId)) {
    throw new Error(`Node with ID '${nodeId}' already exists`);
  }

  // Build properties with type info
  const nodeProperties: Record<string, { value: unknown; type: string }> = {};
  if (properties) {
    for (const [key, value] of Object.entries(properties)) {
      nodeProperties[key] = {
        value,
        type: inferType(value),
      };
    }
  }

  const node: AnodeNode = {
    id: nodeId,
    type,
    properties: nodeProperties,
    position: position || [100, 100],
  };

  state.graph.nodes.push(node);
  state.dirty = true;
  emitGraphEvent({ type: 'node_added', node });

  return node;
}

export function removeNode(nodeId: string): boolean {
  const index = state.graph.nodes.findIndex(n => n.id === nodeId);
  if (index === -1) {
    return false;
  }

  // Remove the node
  state.graph.nodes.splice(index, 1);

  // Remove all connections involving this node
  state.graph.connections = state.graph.connections.filter(
    c => c.from !== nodeId && c.to !== nodeId
  );

  state.dirty = true;
  emitGraphEvent({ type: 'node_removed', nodeId });
  return true;
}

export function getNode(nodeId: string): AnodeNode | undefined {
  return state.graph.nodes.find(n => n.id === nodeId);
}

export function setProperty(nodeId: string, property: string, value: unknown): boolean {
  const node = state.graph.nodes.find(n => n.id === nodeId);
  if (!node) {
    return false;
  }

  node.properties[property] = {
    value,
    type: inferType(value),
  };

  state.dirty = true;
  emitGraphEvent({ type: 'node_updated', nodeId, property, value });
  return true;
}

export function connect(
  fromNode: string,
  fromPort: string,
  toNode: string,
  toPort: string
): AnodeConnection {
  // Validate nodes exist
  if (!state.graph.nodes.some(n => n.id === fromNode)) {
    throw new Error(`Source node '${fromNode}' not found`);
  }
  if (!state.graph.nodes.some(n => n.id === toNode)) {
    throw new Error(`Target node '${toNode}' not found`);
  }

  // Check if connection already exists
  const existing = state.graph.connections.find(
    c => c.from === fromNode && c.fromPort === fromPort &&
         c.to === toNode && c.toPort === toPort
  );
  if (existing) {
    throw new Error('Connection already exists');
  }

  // Check if input port is already connected (inputs can only have one connection)
  const inputConnected = state.graph.connections.find(
    c => c.to === toNode && c.toPort === toPort
  );
  if (inputConnected) {
    throw new Error(`Input port '${toPort}' on node '${toNode}' is already connected`);
  }

  const connection: AnodeConnection = {
    from: fromNode,
    fromPort,
    to: toNode,
    toPort,
  };

  state.graph.connections.push(connection);
  state.dirty = true;
  emitGraphEvent({ type: 'connection_added', connection });

  return connection;
}

export function disconnect(
  fromNode: string,
  fromPort: string,
  toNode: string,
  toPort: string
): boolean {
  const index = state.graph.connections.findIndex(
    c => c.from === fromNode && c.fromPort === fromPort &&
         c.to === toNode && c.toPort === toPort
  );

  if (index === -1) {
    return false;
  }

  const connection = state.graph.connections[index];
  state.graph.connections.splice(index, 1);
  state.dirty = true;
  emitGraphEvent({ type: 'connection_removed', connection });

  return true;
}

export function listConnections(): AnodeConnection[] {
  return [...state.graph.connections];
}
