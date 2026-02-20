/**
 * WebSocket client for real-time sync with MCP server
 */

import type { WsMessage, SyncMessage, GraphEvent } from './types';

const WS_URL = 'ws://localhost:8081';
const RECONNECT_DELAY = 2000;

let ws: WebSocket | null = null;
let onSyncCallback: ((msg: SyncMessage) => void) | null = null;
let onDeltaCallback: ((event: GraphEvent) => void) | null = null;
let reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
let isConnecting = false;

/**
 * Connect to the MCP WebSocket server
 */
export function connectSync(
  onSync: (msg: SyncMessage) => void,
  onDelta: (event: GraphEvent) => void
): void {
  onSyncCallback = onSync;
  onDeltaCallback = onDelta;
  connect();
}

/**
 * Disconnect from the MCP WebSocket server
 */
export function disconnectSync(): void {
  if (reconnectTimeout) {
    clearTimeout(reconnectTimeout);
    reconnectTimeout = null;
  }
  if (ws) {
    ws.close();
    ws = null;
  }
  onSyncCallback = null;
  onDeltaCallback = null;
}

/**
 * Check if connected to MCP
 */
export function isSyncConnected(): boolean {
  return ws !== null && ws.readyState === WebSocket.OPEN;
}

function connect(): void {
  if (isConnecting || (ws && ws.readyState === WebSocket.OPEN)) {
    return;
  }

  isConnecting = true;

  try {
    ws = new WebSocket(WS_URL);

    ws.onopen = () => {
      isConnecting = false;
      console.log('[Sync] Connected to MCP WebSocket');
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data) as WsMessage;

        if (data.type === 'sync') {
          console.log('[Sync] Received full sync:', data.slug);
          onSyncCallback?.(data as SyncMessage);
        } else {
          console.log('[Sync] Received delta:', data.type);
          onDeltaCallback?.(data as GraphEvent);
        }
      } catch (error) {
        console.error('[Sync] Failed to parse message:', error);
      }
    };

    ws.onclose = () => {
      isConnecting = false;
      ws = null;
      console.log('[Sync] Disconnected from MCP, reconnecting...');
      scheduleReconnect();
    };

    ws.onerror = (error) => {
      isConnecting = false;
      console.error('[Sync] WebSocket error:', error);
    };
  } catch (error) {
    isConnecting = false;
    console.error('[Sync] Failed to connect:', error);
    scheduleReconnect();
  }
}

function scheduleReconnect(): void {
  if (reconnectTimeout) return;

  reconnectTimeout = setTimeout(() => {
    reconnectTimeout = null;
    if (onSyncCallback || onDeltaCallback) {
      connect();
    }
  }, RECONNECT_DELAY);
}
