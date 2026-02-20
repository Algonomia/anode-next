/**
 * Real-time sync module
 */

export { connectSync, disconnectSync, isSyncConnected } from './ws-client';
export { applyDelta } from './delta-handler';
export type { GraphEvent, SyncMessage, WsMessage } from './types';
