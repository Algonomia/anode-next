/**
 * Keyboard shortcuts for the graph editor
 * Uses native copy/paste from @comfyorg/litegraph
 */

import { LGraphGroup, type LGraph, type LGraphCanvas, type LGraphNode, Subgraph } from '@comfyorg/litegraph';
import { undo, redo } from './history';

/**
 * Setup keyboard shortcuts for the graph editor
 */
export function setupKeyboardShortcuts(graph: LGraph, graphCanvas: LGraphCanvas): void {
  document.addEventListener('keydown', (e) => {
    // Ignore if typing in an input field
    if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) {
      return;
    }

    // Ctrl+Z: Undo
    if (e.ctrlKey && e.key === 'z' && !e.shiftKey) {
      undo();
      e.preventDefault();
      return;
    }

    // Ctrl+Y or Ctrl+Shift+Z: Redo
    if ((e.ctrlKey && e.key === 'y') || (e.ctrlKey && e.shiftKey && e.key === 'Z')) {
      redo();
      e.preventDefault();
      return;
    }

    // Ctrl+C: Copy selected nodes (native)
    if (e.ctrlKey && e.key === 'c') {
      copySelectedNodes(graphCanvas);
      e.preventDefault();
      return;
    }

    // Ctrl+V: Paste nodes (native)
    if (e.ctrlKey && e.key === 'v') {
      pasteNodes(graphCanvas);
      e.preventDefault();
      return;
    }

    // Ctrl+X: Cut selected nodes
    if (e.ctrlKey && e.key === 'x') {
      copySelectedNodes(graphCanvas);
      deleteSelectedNodes(graph, graphCanvas);
      e.preventDefault();
      return;
    }

    // Delete or Backspace: Delete selected nodes
    if (e.key === 'Delete' || e.key === 'Backspace') {
      deleteSelectedNodes(graph, graphCanvas);
      e.preventDefault();
      return;
    }

    // Ctrl+A: Select all nodes
    if (e.ctrlKey && e.key === 'a') {
      selectAllNodes(graph, graphCanvas);
      e.preventDefault();
      return;
    }

    // Ctrl+D: Duplicate selected nodes
    if (e.ctrlKey && e.key === 'd') {
      copySelectedNodes(graphCanvas);
      pasteNodes(graphCanvas);
      e.preventDefault();
      return;
    }

    // Ctrl+G: Group selected nodes
    if (e.ctrlKey && !e.shiftKey && e.key === 'g') {
      groupSelectedNodes(graph, graphCanvas);
      e.preventDefault();
      return;
    }

    // Ctrl+Shift+G: Convert selected nodes to subgraph
    if (e.ctrlKey && e.shiftKey && (e.key === 'g' || e.key === 'G')) {
      convertToSubgraph(graph, graphCanvas);
      e.preventDefault();
      return;
    }

    // Backspace when no nodes selected: Go back from subgraph
    if (e.key === 'Backspace') {
      const selectedNodes = (graphCanvas as any).selected_nodes as Record<string, LGraphNode> | undefined;
      if (!selectedNodes || Object.keys(selectedNodes).length === 0) {
        exitSubgraph(graphCanvas);
        e.preventDefault();
        return;
      }
    }

    // Escape: Deselect all
    if (e.key === 'Escape') {
      graphCanvas.deselectAllNodes();
      graphCanvas.setDirty(true, true);
    }
  });
}

/**
 * Copy selected nodes using native clipboard
 */
function copySelectedNodes(graphCanvas: LGraphCanvas): void {
  const selectedItems = getSelectedItems(graphCanvas);
  if (selectedItems.length === 0) {
    console.log('No items selected for copy');
    return;
  }

  graphCanvas.copyToClipboard(selectedItems);
  console.log('Copied', selectedItems.length, 'items to clipboard');
}

/**
 * Paste nodes using native clipboard
 */
function pasteNodes(graphCanvas: LGraphCanvas): void {
  graphCanvas.pasteFromClipboard();
  console.log('Pasted from clipboard');
  graphCanvas.setDirty(true, true);
}

/**
 * Delete selected nodes and groups
 */
function deleteSelectedNodes(graph: LGraph, graphCanvas: LGraphCanvas): void {
  const selectedNodes = (graphCanvas as any).selected_nodes as Record<string, LGraphNode> | undefined;
  const selectedGroups = (graphCanvas as any).selected_groups as LGraphGroup[] | undefined;

  if ((!selectedNodes || Object.keys(selectedNodes).length === 0) &&
      (!selectedGroups || selectedGroups.length === 0)) {
    return;
  }

  // Delete selected groups
  if (selectedGroups && selectedGroups.length > 0) {
    for (const group of [...selectedGroups]) {
      (graph as any).remove(group);
    }
  }

  // Delete selected nodes
  if (selectedNodes) {
    const nodesToDelete = Object.values(selectedNodes);
    for (const node of nodesToDelete) {
      if (node) {
        graph.remove(node);
      }
    }
  }

  graphCanvas.deselectAllNodes();
  graphCanvas.setDirty(true, true);
}

/**
 * Select all nodes
 */
function selectAllNodes(graph: LGraph, graphCanvas: LGraphCanvas): void {
  graphCanvas.deselectAllNodes();
  const nodes = (graph as any)._nodes as LGraphNode[] | undefined;
  if (nodes) {
    for (const node of nodes) {
      graphCanvas.selectNode(node, true);
    }
  }
  graphCanvas.setDirty(true, true);
}

/**
 * Group selected nodes
 */
function groupSelectedNodes(graph: LGraph, graphCanvas: LGraphCanvas): void {
  const selectedNodes = (graphCanvas as any).selected_nodes as Record<string, LGraphNode> | undefined;
  if (!selectedNodes || Object.keys(selectedNodes).length === 0) {
    console.log('No nodes selected for grouping');
    return;
  }

  const nodes = Object.values(selectedNodes);
  if (nodes.length === 0) return;

  // Create a new group
  const group = new LGraphGroup('Group');

  // Add to graph first
  (graph as any).add(group);

  // Resize to fit nodes
  group.addNodes(nodes, 20);

  console.log('Created group with', nodes.length, 'nodes');
  graphCanvas.setDirty(true, true);
}

/**
 * Get all selected items (nodes + groups)
 */
function getSelectedItems(graphCanvas: LGraphCanvas): any[] {
  const items: any[] = [];

  const selectedNodes = (graphCanvas as any).selected_nodes as Record<string, LGraphNode> | undefined;
  if (selectedNodes) {
    items.push(...Object.values(selectedNodes));
  }

  const selectedGroups = (graphCanvas as any).selected_groups as LGraphGroup[] | undefined;
  if (selectedGroups) {
    items.push(...selectedGroups);
  }

  return items;
}

/**
 * Convert selected nodes to a subgraph
 */
function convertToSubgraph(graph: LGraph, graphCanvas: LGraphCanvas): void {
  const selectedItems = getSelectedItems(graphCanvas);
  if (selectedItems.length === 0) {
    console.log('No items selected for subgraph');
    return;
  }

  try {
    // Convert items to Set as required by litegraph
    const itemsSet = new Set(selectedItems);

    // Use litegraph's native conversion
    const result = (graph as any).convertToSubgraph(itemsSet);

    if (result && result.subgraph) {
      console.log('Created subgraph:', result.subgraph.name, 'with node:', result.node);

      // Deselect and refresh
      graphCanvas.deselectAllNodes();
      graphCanvas.setDirty(true, true);
    }
  } catch (error) {
    console.error('Failed to create subgraph:', error);
  }
}

/**
 * Enter a subgraph for editing
 */
export function enterSubgraph(graphCanvas: LGraphCanvas, subgraph: Subgraph): void {
  try {
    (graphCanvas as any).openSubgraph(subgraph);
    graphCanvas.setDirty(true, true);
    console.log('Entered subgraph:', subgraph.name);
  } catch (error) {
    console.error('Failed to enter subgraph:', error);
  }
}

/**
 * Exit current subgraph (go back to root graph)
 */
export function exitSubgraph(graphCanvas: LGraphCanvas): void {
  try {
    const currentSubgraph = (graphCanvas as any).subgraph as Subgraph | undefined;

    // Check if we're in a subgraph
    if (currentSubgraph) {
      // Go back to root graph
      const rootGraph = currentSubgraph.rootGraph;
      (graphCanvas as any).setGraph(rootGraph, false);
      (graphCanvas as any).subgraph = undefined;
      graphCanvas.setDirty(true, true);
      console.log('Exited subgraph');
    }
  } catch (error) {
    console.error('Failed to exit subgraph:', error);
  }
}

/**
 * Check if currently inside a subgraph
 */
export function isInSubgraph(graphCanvas: LGraphCanvas): boolean {
  return !!(graphCanvas as any).subgraph;
}

/**
 * Get current subgraph name (or null if at root)
 */
export function getCurrentSubgraphName(graphCanvas: LGraphCanvas): string | null {
  const subgraph = (graphCanvas as any).subgraph as Subgraph | undefined;
  return subgraph ? (subgraph.name || 'Subgraph') : null;
}

/**
 * Get current subgraph path for breadcrumb
 */
export function getSubgraphPath(graphCanvas: LGraphCanvas): string[] {
  const path: string[] = ['Root'];

  const subgraph = (graphCanvas as any).subgraph as Subgraph | undefined;
  if (subgraph) {
    path.push(subgraph.name || 'Subgraph');
  }

  return path;
}
