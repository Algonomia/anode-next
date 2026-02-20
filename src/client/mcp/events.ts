/**
 * Event system for MCP state changes
 * Used to broadcast graph modifications to connected WebSocket clients
 */

import { EventEmitter } from 'events';
import type { AnodeNode, AnodeConnection, AnodeGraph } from './types.js';

export type GraphEvent =
  | { type: 'node_added'; node: AnodeNode }
  | { type: 'node_removed'; nodeId: string }
  | { type: 'node_updated'; nodeId: string; property: string; value: unknown }
  | { type: 'connection_added'; connection: AnodeConnection }
  | { type: 'connection_removed'; connection: AnodeConnection }
  | { type: 'graph_loaded'; graph: AnodeGraph; slug: string | null; name: string | null }
  | { type: 'graph_cleared' };

export const graphEvents = new EventEmitter();

export function emitGraphEvent(event: GraphEvent): void {
  graphEvents.emit('change', event);
}
