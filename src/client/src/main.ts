/**
 * Graph Editor - Main entry point
 */

import { LGraph, LGraphCanvas, LiteGraph } from '@comfyorg/litegraph';
import '@comfyorg/litegraph/style.css';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import 'ag-grid-enterprise';

// Configure LiteGraph settings
LiteGraph.alt_drag_do_clone_nodes = true;           // Alt+drag to clone nodes
LiteGraph.release_link_on_empty_shows_menu = true;  // Show menu when releasing link on empty space
LiteGraph.shift_click_do_break_link_from = true;    // Shift+click to break links

import { AnodeClient, type CsvMetadata } from './api/AnodeClient';
import { registerNodeTypes, setGraphCanvas, setupSubgraphSupport, setSubgraphChangeCallback } from './graph/nodeTypes';
import { anodeToLitegraph, litegraphToAnode } from './graph/conversion';
import { updateInputColors } from './graph/colors';
import { initHistory, resetHistory, setRestoring, isRestoringHistory } from './graph/history';
import { setupKeyboardShortcuts, exitSubgraph, getSubgraphPath, isInSubgraph } from './graph/shortcuts';
import {
  initDataFrame,
  openDataFrameView,
  setSession,
  closeSplitPanel,
  closeDataFrameModal,
  dfPrevPage,
  dfNextPage,
  handleWindowResize,
  getSessionId,
} from './ui/dataframe';
import {
  showSaveModal,
  closeSaveModal,
  getSaveFormValues,
  showLoadModal,
  closeLoadModal,
  loadGraphList,
} from './ui/modals';
import { setStatus, updateGraphName, updateGraphLinks, showResults } from './ui/toolbar';
import { initExecutionHistory, showExecutionHistory } from './ui/executionHistory';
import { initCsvEditor } from './ui/csvEditor';
import type { AnodeLGraphNode } from './types/litegraph';
import { connectSync, applyDelta, type SyncMessage, type GraphEvent } from './sync';
import {
  executeGraphWithStream,
  resetExecutionState,
  setNodeStatus,
  onExecutionStateChange,
  type CsvOutputMetadata,
} from './execution';

// State
let client: AnodeClient;
let graph: LGraph;
let graphCanvas: LGraphCanvas;
let currentSlug: string | null = null;
let currentName = 'New Graph';
let currentSessionId: string | null = null;
let currentCsvMetadata: Record<string, Record<string, CsvMetadata>> | null = null;

// Make functions available globally for HTML onclick handlers
declare global {
  interface Window {
    newGraph: () => void;
    openLoadModal: () => void;
    saveGraph: () => void;
    executeGraph: () => void;
    showHistory: () => void;
    closeSaveModal: () => void;
    closeLoadModal: () => void;
    confirmSave: () => Promise<void>;
    closeDataFrameModal: () => void;
    closeSplitPanel: () => void;
    dfPrevPage: () => void;
    dfNextPage: () => void;
    navigateToRoot: () => void;
    navigateBack: () => void;
    navigateToBreadcrumb: (index: number) => void;
    downloadLocalBackup: () => void;
  }
}

/**
 * Initialize the application
 */
async function init(): Promise<void> {
  client = new AnodeClient();

  // Init LiteGraph
  graph = new LGraph();
  const canvas = document.getElementById('graph-canvas') as HTMLCanvasElement;
  graphCanvas = new LGraphCanvas(canvas, graph);

  // Set canvas reference for nodeTypes
  setGraphCanvas(graphCanvas);

  // Setup subgraph support
  setupSubgraphSupport(graph);
  setSubgraphChangeCallback(updateBreadcrumb);

  // Resize canvas
  resizeCanvas();
  window.addEventListener('resize', handleResize);

  // Init modules
  initDataFrame(client);
  initCsvEditor();
  initHistory(graph, graphCanvas);
  setupKeyboardShortcuts(graph, graphCanvas);
  initExecutionHistory(client, handleExecutionRestore);

  // Setup global functions for HTML handlers
  setupGlobalFunctions();

  try {
    // Fetch node catalog and register types
    const catalog = await client.getNodes();
    registerNodeTypes(catalog);

    setStatus('connected', `Connected - ${catalog.nodes.length} node types loaded`);

    // Connect to MCP WebSocket for real-time sync
    connectSync(handleSync, handleDelta);

    // Check for URL parameters and auto-load
    const urlParams = new URLSearchParams(window.location.search);
    const graphSlug = urlParams.get('graph');
    const executionParam = urlParams.get('execution');

    if (graphSlug) {
      await loadGraph(graphSlug);

      // Load execution if parameter is present
      if (executionParam) {
        await loadExecutionFromParam(executionParam);
      }
    }
  } catch (error) {
    setStatus('error', `Connection error: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

/**
 * Load an execution from URL parameter
 * @param param - 'latest' or execution ID
 */
async function loadExecutionFromParam(param: string): Promise<void> {
  if (!currentSlug) return;

  try {
    let executionId: number;

    if (param === 'latest') {
      const response = await client.listExecutions(currentSlug);
      if (response.executions.length === 0) {
        showResults('No past executions found for this graph.', true);
        return;
      }
      executionId = response.executions[0].id;
    } else {
      executionId = parseInt(param, 10);
      if (isNaN(executionId)) {
        showResults(`Invalid execution parameter: ${param}`, true);
        return;
      }
    }

    setStatus('loading', 'Loading execution...');
    const response = await client.restoreExecution(executionId);

    if (response.status === 'ok') {
      restoreExecutionState(response.csv_metadata);
      handleExecutionRestore(response.session_id, response.csv_metadata);
      setStatus('connected', 'Execution loaded');
    } else {
      showResults('Failed to load execution', true);
    }
  } catch (error) {
    setStatus('error', `Failed to load execution: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

/**
 * Restore execution state for visual indicators
 */
function restoreExecutionState(csvMetadata: Record<string, Record<string, CsvMetadata>>): void {
  resetExecutionState();
  for (const nodeId of Object.keys(csvMetadata)) {
    setNodeStatus(nodeId, { status: 'success', csvMetadata: csvMetadata[nodeId] });
  }
  graphCanvas.setDirty(true, true);
}

/**
 * Handle full graph sync from MCP
 */
function handleSync(msg: SyncMessage): void {
  console.log('[Main] Full sync received:', msg.slug);

  setRestoring(true);
  anodeToLitegraph(msg.graph, graph);
  updateInputColors(graph, graphCanvas);
  setRestoring(false);

  currentSlug = msg.slug;
  currentName = msg.name || 'New Graph';
  updateGraphName(currentName);

  resetHistory();
  graphCanvas.setDirty(true, true);

  setStatus('connected', msg.slug ? `Synced: ${currentName}` : 'Synced with MCP');
}

/**
 * Handle delta events from MCP
 */
function handleDelta(event: GraphEvent): void {
  // Don't apply deltas while user is actively restoring (e.g., undo/redo)
  if (isRestoringHistory()) {
    console.log('[Main] Ignoring delta during restore:', event.type);
    return;
  }

  // For graph_loaded and graph_cleared, do a full sync
  if (event.type === 'graph_loaded') {
    handleSync({
      type: 'sync',
      graph: event.graph,
      slug: event.slug,
      name: event.name,
    });
    return;
  }

  if (event.type === 'graph_cleared') {
    setRestoring(true);
    graph.clear();
    setRestoring(false);
    currentSlug = null;
    currentName = 'New Graph';
    updateGraphName(currentName);
    resetHistory();
    graphCanvas.setDirty(true, true);
    return;
  }

  // Apply delta
  setRestoring(true);
  applyDelta(graph, graphCanvas, event);
  setRestoring(false);
}

/**
 * Setup global functions for HTML onclick handlers
 */
function setupGlobalFunctions(): void {
  window.newGraph = newGraph;
  window.openLoadModal = openLoadModalFn;
  window.saveGraph = saveGraph;
  window.executeGraph = executeGraph;
  window.showHistory = showHistory;
  window.closeSaveModal = closeSaveModal;
  window.closeLoadModal = closeLoadModal;
  window.confirmSave = confirmSave;
  window.closeDataFrameModal = closeDataFrameModal;
  window.closeSplitPanel = closeSplitPanel;
  window.dfPrevPage = dfPrevPage;
  window.dfNextPage = dfNextPage;
  window.navigateToRoot = navigateToRoot;
  window.navigateBack = navigateBack;
  window.navigateToBreadcrumb = navigateToBreadcrumb;
  window.downloadLocalBackup = downloadLocalBackup;
}

/**
 * Show execution history modal
 */
function showHistory(): void {
  if (!currentSlug) {
    showResults('Please save or load a graph first.', true);
    return;
  }
  showExecutionHistory(currentSlug);
}

/**
 * Handle execution restore callback
 */
function handleExecutionRestore(
  sessionId: string,
  csvMetadata: Record<string, Record<string, CsvMetadata>>
): void {
  currentSessionId = sessionId;
  currentCsvMetadata = csvMetadata;
  setSession(sessionId, csvMetadata);

  // Add visualization widgets
  addVisualizationWidgets();

  setStatus('connected', 'Execution restored');
  showResults('Past execution restored. Click "View" buttons to see data.');
}

/**
 * Resize canvas to fit container
 */
function resizeCanvas(): void {
  const container = document.querySelector('.canvas-container') as HTMLElement;
  const canvas = document.getElementById('graph-canvas') as HTMLCanvasElement;
  if (container && canvas) {
    canvas.width = container.clientWidth;
    canvas.height = container.clientHeight;
    if (graphCanvas) graphCanvas.resize();
  }
}

/**
 * Handle window resize
 */
function handleResize(): void {
  resizeCanvas();
  handleWindowResize((nodeId, portName, metadata) => {
    openDataFrameView(nodeId, portName, metadata);
  });
}

/**
 * Create a new empty graph
 */
function newGraph(): void {
  graph.clear();
  currentSlug = null;
  currentName = 'New Graph';
  updateGraphName(currentName);
  updateGraphLinks({ outgoing: [], incoming: [] });
  showResults('Graph cleared. Create your nodes!');
  resetHistory();
}

/**
 * Open load modal and fetch graph list
 */
function openLoadModalFn(): void {
  showLoadModal();
  loadGraphList(client, loadGraph);
}

/**
 * Load a graph by slug
 */
async function loadGraph(slug: string): Promise<void> {
  closeLoadModal();
  setStatus('loading', 'Loading graph...');

  try {
    const response = await client.getGraph(slug);
    console.log('[LOAD] Server response:', JSON.stringify(response.graph, null, 2));

    setRestoring(true);
    anodeToLitegraph(response.graph, graph);
    updateInputColors(graph, graphCanvas);
    setRestoring(false);

    currentSlug = slug;
    currentName = response.metadata.name;
    updateGraphName(currentName);
    updateGraphLinks(response.links || { outgoing: [], incoming: [] });

    resetHistory();

    setStatus('connected', `Loaded: ${currentName}`);
    showResults(`Graph "${currentName}" loaded successfully.`);
  } catch (error) {
    setRestoring(false);
    setStatus('error', `Load error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    showResults(`Error loading graph: ${error instanceof Error ? error.message : 'Unknown error'}`, true);
  }
}

/**
 * Save graph
 */
function saveGraph(): void {
  if (currentSlug) {
    confirmUpdate();
  } else {
    showSaveModal();
  }
}

/**
 * Confirm save (new graph)
 */
async function confirmSave(): Promise<void> {
  const { slug, name, description } = getSaveFormValues();

  if (!slug || !name) {
    alert('Slug and Name are required');
    return;
  }

  closeSaveModal();
  setStatus('loading', 'Saving...');

  try {
    const anodeGraph = litegraphToAnode(graph);
    await client.createGraph({
      slug,
      name,
      description: description || undefined,
      graph: anodeGraph,
    });

    currentSlug = slug;
    currentName = name;
    updateGraphName(currentName);

    // Fetch links for the newly created graph
    try {
      const graphData = await client.getGraph(slug);
      updateGraphLinks(graphData.links || { outgoing: [], incoming: [] });
    } catch {
      // Non-critical, just skip link badges
    }

    setStatus('connected', `Saved: ${name}`);
    showResults(`Graph "${name}" saved successfully.`);
  } catch (error) {
    setStatus('error', `Save error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    showResults(`Error saving graph: ${error instanceof Error ? error.message : 'Unknown error'}`, true);
  }
}

/**
 * Update existing graph
 */
async function confirmUpdate(): Promise<void> {
  if (!currentSlug) return;

  setStatus('loading', 'Saving...');

  try {
    const anodeGraph = litegraphToAnode(graph);
    const response = await client.updateGraph(currentSlug, {
      graph: anodeGraph,
    });

    updateGraphLinks(response.links || { outgoing: [], incoming: [] });

    setStatus('connected', `Updated: ${currentName}`);
    showResults(`Graph "${currentName}" updated successfully.`);
  } catch (error) {
    setStatus('error', `Update error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    showResults(`Error updating graph: ${error instanceof Error ? error.message : 'Unknown error'}`, true);
  }
}

/**
 * Execute graph with real-time streaming feedback
 */
function executeGraph(): void {
  if (!currentSlug) {
    showResults('Please save the graph before executing.', true);
    return;
  }

  setStatus('loading', 'Executing...');

  // Reset execution state and clear existing visualization widgets
  resetExecutionState();
  clearVisualizationWidgets();

  // Initialize metadata storage
  currentCsvMetadata = {};

  executeGraphWithStream('', currentSlug, {
    onStart: (sessionId, nodeCount) => {
      currentSessionId = sessionId;
      setSession(sessionId, {});
      setStatus('loading', `Executing ${nodeCount} nodes...`);
      showResults(`Starting execution of ${nodeCount} nodes...`);
    },

    onNodeStarted: (nodeId) => {
      // Redraw to show yellow indicator
      graphCanvas.setDirty(true, true);
    },

    onNodeCompleted: (nodeId, durationMs, csvMeta) => {
      // Add "View CSV" button immediately for this node
      if (csvMeta && Object.keys(csvMeta).length > 0) {
        currentCsvMetadata![nodeId] = csvMeta as Record<string, CsvMetadata>;
        addVisualizationWidgetForNode(nodeId, csvMeta as Record<string, CsvMetadata>);
      }
      // Redraw to show green indicator and time
      graphCanvas.setDirty(true, true);
    },

    onNodeFailed: (nodeId, durationMs, error) => {
      showResults(`Node ${nodeId} failed: ${error}`, true);
      // Redraw to show red indicator
      graphCanvas.setDirty(true, true);
    },

    onComplete: (sessionId, hasErrors) => {
      setSession(sessionId, currentCsvMetadata || {});
      if (hasErrors) {
        setStatus('error', 'Execution completed with errors');
        showResults('Execution completed with errors.', true);
      } else {
        setStatus('connected', 'Execution completed');
        showResults('Execution completed successfully.');
      }
    },

    onError: (error) => {
      setStatus('error', `Execution error: ${error}`);
      showResults(`Execution error: ${error}`, true);
    },
  });
}

/**
 * Add "View CSV" widget to a single node
 */
function addVisualizationWidgetForNode(
  nodeId: string,
  csvMeta: Record<string, CsvMetadata>
): void {
  const nodes = (graph as any)._nodes as AnodeLGraphNode[] | undefined;
  if (!nodes) return;

  const lgNode = nodes.find(n => (n._anodeId || `node_${n.id}`) === nodeId);
  if (!lgNode) return;

  // Remove existing visualization widgets for this node
  const widgets = (lgNode as any).widgets as any[] | undefined;
  if (widgets) {
    (lgNode as any).widgets = widgets.filter((w: any) => !w.name.startsWith('View '));
  }

  // Add a widget for each CSV output
  for (const [portName, meta] of Object.entries(csvMeta)) {
    const rows = meta.rows || 0;
    lgNode.addWidget('button', `View ${portName} (${rows})`, '', () => {
      openDataFrameView(nodeId, portName, meta);
    });
  }

  // Recalculate node size to fit widgets
  lgNode.setSize(lgNode.computeSize());
}

/**
 * Clear all visualization widgets from all nodes
 */
function clearVisualizationWidgets(): void {
  const nodes = (graph as any)._nodes as AnodeLGraphNode[] | undefined;
  if (!nodes) return;

  for (const lgNode of nodes) {
    const widgets = (lgNode as any).widgets as any[] | undefined;
    if (widgets) {
      (lgNode as any).widgets = widgets.filter((w: any) => !w.name.startsWith('View '));
      lgNode.setSize(lgNode.computeSize());
    }
  }

  graphCanvas.setDirty(true, true);
}

/**
 * Add "View CSV" widgets to nodes with CSV outputs
 */
function addVisualizationWidgets(): void {
  if (!currentCsvMetadata || !currentSessionId) return;

  const nodes = (graph as any)._nodes as AnodeLGraphNode[] | undefined;
  if (!nodes) return;

  for (const lgNode of nodes) {
    const anodeId = lgNode._anodeId || `node_${lgNode.id}`;
    const nodeMeta = currentCsvMetadata[anodeId];
    if (!nodeMeta) continue;

    // Remove existing visualization widgets
    const widgets = (lgNode as any).widgets as any[] | undefined;
    if (widgets) {
      (lgNode as any).widgets = widgets.filter((w: any) => !w.name.startsWith('View '));
    }

    // Add a widget for each CSV output
    for (const [portName, meta] of Object.entries(nodeMeta)) {
      const rows = meta.rows || 0;
      lgNode.addWidget('button', `View ${portName} (${rows})`, '', () => {
        openDataFrameView(anodeId, portName, meta);
      });
    }

    // Recalculate node size to fit widgets
    lgNode.setSize(lgNode.computeSize());
  }

  // Redraw canvas
  graphCanvas.setDirty(true, true);
}

/**
 * Update the breadcrumb UI for subgraph navigation
 */
function updateBreadcrumb(): void {
  const breadcrumbEl = document.getElementById('breadcrumb');
  if (!breadcrumbEl) return;

  const path = getSubgraphPath(graphCanvas);

  // Hide breadcrumb if at root level
  if (path.length <= 1) {
    breadcrumbEl.style.display = 'none';
    return;
  }

  breadcrumbEl.style.display = 'flex';

  // Build breadcrumb HTML
  let html = '<button class="btn-back" onclick="navigateBack()">&larr; Back</button>';

  path.forEach((name, index) => {
    if (index > 0) {
      html += '<span class="breadcrumb-separator">/</span>';
    }

    if (index < path.length - 1) {
      // Clickable item (not the last one)
      html += `<span class="breadcrumb-item" onclick="navigateToBreadcrumb(${index})">${name}</span>`;
    } else {
      // Current location (last item)
      html += `<span class="breadcrumb-item">${name}</span>`;
    }
  });

  breadcrumbEl.innerHTML = html;
}

/**
 * Navigate to root graph
 */
function navigateToRoot(): void {
  // Exit all subgraphs until we're at root
  let currentGraph = (graphCanvas as any).graph;
  while (currentGraph && currentGraph.parentGraph) {
    exitSubgraph(graphCanvas);
    currentGraph = (graphCanvas as any).graph;
  }
  updateBreadcrumb();
}

/**
 * Navigate back one level
 */
function navigateBack(): void {
  exitSubgraph(graphCanvas);
  updateBreadcrumb();
}

/**
 * Navigate to a specific breadcrumb level
 * index 0 = root, higher = nested subgraphs
 */
function navigateToBreadcrumb(index: number): void {
  if (index === 0) {
    // Go to root
    navigateToRoot();
  }
  // For now, we only support one level of nesting, so index 0 is the only clickable one
  // Future: support multi-level navigation
}

/**
 * Download the current graph as a local JSON backup
 */
function downloadLocalBackup(): void {
  const anodeGraph = litegraphToAnode(graph);
  const payload = {
    slug: currentSlug,
    name: currentName,
    graph: anodeGraph,
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `${currentSlug || 'graph'}-backup.json`;
  a.click();
  URL.revokeObjectURL(url);
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', init);
