/**
 * Types for real-time sync with MCP server
 */

import type { AnodeGraph, AnodeNode, AnodeConnection } from '../api/AnodeClient';

export type GraphEvent =
  | { type: 'node_added'; node: AnodeNode }
  | { type: 'node_removed'; nodeId: string }
  | { type: 'node_updated'; nodeId: string; property: string; value: unknown }
  | { type: 'connection_added'; connection: AnodeConnection }
  | { type: 'connection_removed'; connection: AnodeConnection }
  | { type: 'graph_loaded'; graph: AnodeGraph; slug: string | null; name: string | null }
  | { type: 'graph_cleared' };

export interface SyncMessage {
  type: 'sync';
  graph: AnodeGraph;
  slug: string | null;
  name: string | null;
}

export type WsMessage = GraphEvent | SyncMessage;
