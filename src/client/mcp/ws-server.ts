/**
 * WebSocket server for real-time sync with frontend
 * Broadcasts graph changes to connected clients
 */

import { WebSocketServer, WebSocket } from 'ws';
import { graphEvents, type GraphEvent } from './events.js';
import { getCurrentGraph, getGraphState } from './state.js';

const WS_PORT = 8081;
let wss: WebSocketServer | null = null;
const clients = new Set<WebSocket>();

export function startWebSocketServer(): void {
  wss = new WebSocketServer({ port: WS_PORT });

  wss.on('connection', (ws) => {
    clients.add(ws);
    console.error(`[MCP-WS] Client connected (${clients.size} total)`);

    // Send full graph state on connect
    const state = getGraphState();
    const graph = getCurrentGraph();
    const syncMessage = JSON.stringify({
      type: 'sync',
      graph,
      slug: state.slug,
      name: state.name,
    });
    ws.send(syncMessage);

    ws.on('close', () => {
      clients.delete(ws);
      console.error(`[MCP-WS] Client disconnected (${clients.size} remaining)`);
    });

    ws.on('error', (error) => {
      console.error(`[MCP-WS] Client error:`, error.message);
      clients.delete(ws);
    });
  });

  wss.on('error', (error) => {
    console.error(`[MCP-WS] Server error:`, error.message);
  });

  // Broadcast state changes to all clients
  graphEvents.on('change', (event: GraphEvent) => {
    const message = JSON.stringify(event);
    for (const client of clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    }
  });

  console.error(`[MCP-WS] WebSocket server listening on ws://localhost:${WS_PORT}`);
}

export function stopWebSocketServer(): void {
  if (wss) {
    for (const client of clients) {
      client.close();
    }
    clients.clear();
    wss.close();
    wss = null;
    console.error(`[MCP-WS] WebSocket server stopped`);
  }
}

export function getConnectedClientsCount(): number {
  return clients.size;
}
