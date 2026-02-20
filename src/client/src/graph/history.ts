/**
 * History management (Undo/Redo) for the graph editor
 */

import type { LGraph, LGraphCanvas } from '@comfyorg/litegraph';
import type { AnodeGraph } from '../api/AnodeClient';
import { anodeToLitegraph, litegraphToAnode } from './conversion';
import { updateInputColors } from './colors';

const MAX_HISTORY = 50;

// History state
let history: AnodeGraph[] = [];
let historyIndex = -1;
let isRestoring = false;
let isBatching = false;
let moveTimeout: ReturnType<typeof setTimeout> | null = null;

// References to graph and canvas
let _graph: LGraph | null = null;
let _graphCanvas: LGraphCanvas | null = null;

/**
 * Initialize history with graph references
 */
export function initHistory(graph: LGraph, graphCanvas: LGraphCanvas): void {
  _graph = graph;
  _graphCanvas = graphCanvas;
  setupHistoryHooks();
  resetHistory();
}

/**
 * Check if we're currently restoring from history
 */
export function isRestoringHistory(): boolean {
  return isRestoring;
}

/**
 * Set the restoring flag (for external use during loading)
 */
export function setRestoring(value: boolean): void {
  isRestoring = value;
}

/**
 * Save a snapshot to history
 */
export function saveSnapshot(): void {
  if (isRestoring || isBatching || !_graph) return;

  // Remove future states if we're not at the end
  if (historyIndex < history.length - 1) {
    history.splice(historyIndex + 1);
  }

  // Capture current state
  const snapshot = litegraphToAnode(_graph);
  history.push(snapshot);

  // Limit history size
  if (history.length > MAX_HISTORY) {
    history.shift();
  } else {
    historyIndex++;
  }
}

/**
 * Execute multiple operations as a single undo step
 */
export function batch(fn: () => void): void {
  isBatching = true;
  try {
    fn();
  } finally {
    isBatching = false;
    saveSnapshot();
  }
}

/**
 * Undo the last action
 */
export function undo(): void {
  if (historyIndex <= 0 || !_graph || !_graphCanvas) {
    console.log('Nothing to undo');
    return;
  }

  historyIndex--;
  restoreSnapshot(history[historyIndex]);
  console.log('Undo to state', historyIndex);
}

/**
 * Redo the last undone action
 */
export function redo(): void {
  if (historyIndex >= history.length - 1 || !_graph || !_graphCanvas) {
    console.log('Nothing to redo');
    return;
  }

  historyIndex++;
  restoreSnapshot(history[historyIndex]);
  console.log('Redo to state', historyIndex);
}

/**
 * Restore a snapshot
 */
function restoreSnapshot(snapshot: AnodeGraph): void {
  if (!_graph || !_graphCanvas) return;

  isRestoring = true;
  anodeToLitegraph(snapshot, _graph);
  updateInputColors(_graph, _graphCanvas);
  _graphCanvas.setDirty(true, true);
  isRestoring = false;
}

/**
 * Reset history (called after loading a graph)
 */
export function resetHistory(): void {
  history = [];
  historyIndex = -1;
  saveSnapshot();
}

/**
 * Setup hooks on the graph for automatic history tracking
 */
function setupHistoryHooks(): void {
  if (!_graph || !_graphCanvas) return;

  const graph = _graph;
  const graphCanvas = _graphCanvas;

  // Hook for node added
  const origAdd = graph.add.bind(graph);
  (graph as any).add = function (node: any, skip_compute_order?: boolean) {
    const result = origAdd(node, skip_compute_order);
    saveSnapshot();
    return result;
  };

  // Hook for node removed
  const origRemove = graph.remove.bind(graph);
  (graph as any).remove = function (node: any) {
    const result = origRemove(node);
    saveSnapshot();
    return result;
  };

  // Hook for connections
  (graph as any).onConnectionChange = function () {
    updateInputColors(graph, graphCanvas);
    saveSnapshot();
  };

  // Hook for node moved (debounced)
  (graphCanvas as any).onNodeMoved = function (_node: any) {
    if (moveTimeout) clearTimeout(moveTimeout);
    moveTimeout = setTimeout(() => saveSnapshot(), 300);
  };
}
