/**
 * Apply delta events from MCP to LiteGraph
 */

import { LiteGraph, type LGraph, type LGraphCanvas } from '@comfyorg/litegraph';
import type { GraphEvent } from './types';
import type { AnodeLGraphNode, NodeProperty } from '../types/litegraph';

/**
 * Apply a delta event to the LiteGraph graph
 */
export function applyDelta(
  graph: LGraph,
  canvas: LGraphCanvas,
  event: GraphEvent
): void {
  const nodes = (graph as any)._nodes as AnodeLGraphNode[] | undefined;

  switch (event.type) {
    case 'node_added': {
      // Check if node already exists (avoid duplicates)
      const existing = nodes?.find(n => n._anodeId === event.node.id);
      if (existing) {
        console.log('[Delta] Node already exists:', event.node.id);
        return;
      }

      const lgNode = LiteGraph.createNode(event.node.type) as AnodeLGraphNode | null;
      if (!lgNode) {
        console.error('[Delta] Failed to create node:', event.node.type);
        return;
      }

      lgNode._anodeId = event.node.id;

      // Set position
      if (event.node.position) {
        lgNode.pos = [event.node.position[0], event.node.position[1]];
      }

      // Copy properties
      for (const [key, prop] of Object.entries(event.node.properties || {})) {
        lgNode.properties[key] = prop.value as NodeProperty;

        // Update widget if exists
        const widgets = (lgNode as any).widgets as any[] | undefined;
        if (widgets) {
          const w = widgets.find((w: any) => w.name === key);
          if (w) w.value = prop.value;
        }
      }

      graph.add(lgNode);
      console.log('[Delta] Node added:', event.node.id, '->', lgNode.id);
      break;
    }

    case 'node_removed': {
      const lgNode = nodes?.find(n => n._anodeId === event.nodeId);
      if (lgNode) {
        graph.remove(lgNode);
        console.log('[Delta] Node removed:', event.nodeId);
      }
      break;
    }

    case 'node_updated': {
      const lgNode = nodes?.find(n => n._anodeId === event.nodeId);
      if (lgNode) {
        lgNode.properties[event.property] = event.value as NodeProperty;

        // Update widget if exists
        const widgets = (lgNode as any).widgets as any[] | undefined;
        if (widgets) {
          const w = widgets.find((w: any) => w.name === event.property);
          if (w) w.value = event.value;
        }

        console.log('[Delta] Node updated:', event.nodeId, event.property, '=', event.value);
      }
      break;
    }

    case 'connection_added': {
      const fromNode = nodes?.find(n => n._anodeId === event.connection.from);
      const toNode = nodes?.find(n => n._anodeId === event.connection.to);

      if (!fromNode || !toNode) {
        console.error('[Delta] Connection failed - node not found:', event.connection);
        return;
      }

      const fromSlot = fromNode.findOutputSlot(event.connection.fromPort);
      const toSlot = toNode.findInputSlot(event.connection.toPort);

      if (fromSlot === -1 || toSlot === -1) {
        console.error('[Delta] Connection failed - slot not found:', event.connection);
        return;
      }

      fromNode.connect(fromSlot, toNode, toSlot);
      console.log('[Delta] Connected:', event.connection.from, '->', event.connection.to);
      break;
    }

    case 'connection_removed': {
      const toNode = nodes?.find(n => n._anodeId === event.connection.to);
      if (toNode) {
        const toSlot = toNode.findInputSlot(event.connection.toPort);
        if (toSlot !== -1) {
          toNode.disconnectInput(toSlot);
          console.log('[Delta] Disconnected:', event.connection.from, '->', event.connection.to);
        }
      }
      break;
    }

    case 'graph_loaded':
    case 'graph_cleared':
      // These are handled by the sync handler, not delta handler
      console.log('[Delta] Graph event:', event.type, '- handled by sync');
      break;
  }

  // Always redraw after any change
  canvas.setDirty(true, true);
}
