/**
 * Execution state management for real-time graph execution feedback
 */

export type NodeExecutionStatus = 'idle' | 'running' | 'success' | 'error';

export interface CsvOutputMetadata {
  rows: number;
  columns: string[];
}

export interface NodeExecutionState {
  status: NodeExecutionStatus;
  durationMs?: number;
  errorMessage?: string;
  csvMetadata?: Record<string, CsvOutputMetadata>;
}

// Map: nodeId -> execution state
const executionState = new Map<string, NodeExecutionState>();

// Callback when state changes
type StateChangeCallback = (nodeId: string, state: NodeExecutionState) => void;
let onStateChangeCallback: StateChangeCallback | null = null;

/**
 * Reset all execution state (call before new execution)
 */
export function resetExecutionState(): void {
  executionState.clear();
}

/**
 * Set the execution status for a node
 */
export function setNodeStatus(nodeId: string, state: NodeExecutionState): void {
  executionState.set(nodeId, state);
  onStateChangeCallback?.(nodeId, state);
}

/**
 * Get the execution status for a node
 */
export function getNodeStatus(nodeId: string): NodeExecutionState {
  return executionState.get(nodeId) || { status: 'idle' };
}

/**
 * Check if a node has been executed (not idle)
 */
export function hasNodeExecuted(nodeId: string): boolean {
  const state = executionState.get(nodeId);
  return state !== undefined && state.status !== 'idle';
}

/**
 * Get all nodes that have CSV output metadata
 */
export function getNodesWithCsvOutput(): Array<{ nodeId: string; csvMetadata: Record<string, CsvOutputMetadata> }> {
  const result: Array<{ nodeId: string; csvMetadata: Record<string, CsvOutputMetadata> }> = [];
  for (const [nodeId, state] of executionState) {
    if (state.csvMetadata && Object.keys(state.csvMetadata).length > 0) {
      result.push({ nodeId, csvMetadata: state.csvMetadata });
    }
  }
  return result;
}

/**
 * Register a callback for when execution state changes
 */
export function onExecutionStateChange(callback: StateChangeCallback): void {
  onStateChangeCallback = callback;
}

/**
 * Remove the state change callback
 */
export function removeExecutionStateChangeCallback(): void {
  onStateChangeCallback = null;
}
