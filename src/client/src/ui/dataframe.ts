/**
 * DataFrame visualization using AG Grid
 */

import { createGrid, type GridApi, type GridOptions } from 'ag-grid-community';
import type { AnodeClient, CsvMetadata, SessionDataFrameResponse } from '../api/AnodeClient';

const PAGE_SIZE = 100;

// State
let currentSessionId: string | null = null;
let currentNodeId: string | null = null;
let currentPortName: string | null = null;
let currentMetadata: CsvMetadata | null = null;
let currentPage = 0;
let totalRows = 0;
let gridApi: GridApi | null = null;
let currentTreeMode = false;
let panelIsOpen = false;
let isResizing = false;
let client: AnodeClient | null = null;

const PANEL_WIDTH_KEY = 'df-panel-width';

/**
 * Initialize the dataframe viewer
 */
export function initDataFrame(anodeClient: AnodeClient): void {
  client = anodeClient;
  setupPanelResizer();
  restorePanelWidth();
}

/**
 * Set the current session for dataframe queries
 */
export function setSession(sessionId: string, csvMetadata: Record<string, Record<string, CsvMetadata>>): void {
  currentSessionId = sessionId;
}

/**
 * Get current session ID
 */
export function getSessionId(): string | null {
  return currentSessionId;
}

/**
 * Get current CSV metadata
 */
export function getCsvMetadata(): Record<string, Record<string, CsvMetadata>> | null {
  return null; // Handled externally
}

/**
 * Check if screen is ultra-wide (21:9 or wider)
 */
function isUltraWide(): boolean {
  const ratio = window.innerWidth / window.innerHeight;
  return ratio >= 2.33;
}

/**
 * Open dataframe view for a node output
 */
export function openDataFrameView(nodeId: string, portName: string, metadata: CsvMetadata): void {
  currentNodeId = nodeId;
  currentPortName = portName;
  currentMetadata = metadata;
  currentPage = 0;
  totalRows = metadata.rows || 0;
  currentTreeMode = (metadata.columns ?? []).includes('__tree_path');

  if (isUltraWide()) {
    showSplitPanel();
  } else {
    showFullscreenModal();
  }
}

/**
 * Show split panel (for ultra-wide screens)
 */
function showSplitPanel(): void {
  const panel = document.getElementById('dataframe-panel');
  const resizer = document.getElementById('panel-resizer');

  if (panel) panel.classList.add('open');
  if (resizer) resizer.style.display = 'block';
  panelIsOpen = true;

  // Resize canvas
  window.dispatchEvent(new Event('resize'));

  // Load data
  loadDataFrameToPanel();
}

/**
 * Close split panel
 */
export function closeSplitPanel(): void {
  const panel = document.getElementById('dataframe-panel');
  const resizer = document.getElementById('panel-resizer');

  if (panel) panel.classList.remove('open');
  if (resizer) resizer.style.display = 'none';
  panelIsOpen = false;

  if (gridApi) {
    gridApi.destroy();
    gridApi = null;
  }

  // Resize canvas
  setTimeout(() => {
    window.dispatchEvent(new Event('resize'));
  }, 10);
}

/**
 * Show fullscreen modal (for standard screens)
 */
function showFullscreenModal(): void {
  const modal = document.getElementById('dataframe-modal');
  if (modal) modal.classList.add('active', 'fullscreen');
  loadDataFramePage();
}

/**
 * Close dataframe modal
 */
export function closeDataFrameModal(): void {
  const modal = document.getElementById('dataframe-modal');
  if (modal) modal.classList.remove('active', 'fullscreen');

  if (gridApi) {
    gridApi.destroy();
    gridApi = null;
  }
}

/**
 * Load dataframe data to split panel
 */
async function loadDataFrameToPanel(): Promise<void> {
  if (!client || !currentSessionId || !currentNodeId || !currentPortName) return;

  const gridContainer = document.getElementById('panel-grid');
  if (!gridContainer) return;

  try {
    const response = await client.querySessionDataFrame(
      currentSessionId,
      currentNodeId,
      currentPortName,
      {
        limit: currentTreeMode ? totalRows : PAGE_SIZE,
        offset: currentTreeMode ? 0 : currentPage * PAGE_SIZE,
      }
    );

    if (response.status !== 'ok') {
      throw new Error(response.message || 'Failed to load data');
    }

    renderGridInContainer(gridContainer, response);
    updatePanelPageInfo(response.stats.total_rows);
  } catch (error) {
    console.error('Error loading DataFrame:', error);
    gridContainer.innerHTML = `<div style="padding: 20px; color: #ff6b6b;">Error: ${error instanceof Error ? error.message : 'Unknown error'}</div>`;
  }
}

/**
 * Load dataframe page for modal
 */
async function loadDataFramePage(): Promise<void> {
  if (!client || !currentSessionId || !currentNodeId || !currentPortName) return;

  const gridContainer = document.getElementById('dataframe-grid');
  if (!gridContainer) return;

  try {
    const response = await client.querySessionDataFrame(
      currentSessionId,
      currentNodeId,
      currentPortName,
      {
        limit: currentTreeMode ? totalRows : PAGE_SIZE,
        offset: currentTreeMode ? 0 : currentPage * PAGE_SIZE,
      }
    );

    if (response.status !== 'ok') {
      throw new Error(response.message || 'Failed to load data');
    }

    renderGridInContainer(gridContainer, response);
    updateModalPageInfo(response.stats.total_rows);
  } catch (error) {
    console.error('Error loading DataFrame:', error);
    gridContainer.innerHTML = `<div style="padding: 20px; color: #ff6b6b;">Error: ${error instanceof Error ? error.message : 'Unknown error'}</div>`;
  }
}

/**
 * Render AG Grid in a container
 */
function renderGridInContainer(container: HTMLElement, response: SessionDataFrameResponse): void {
  if (gridApi) {
    gridApi.destroy();
    gridApi = null;
  }
  container.innerHTML = '';

  const isTreeMode = response.columns.includes('__tree_path');
  const metaColumns = ['__tree_path', '__tree_agg'];

  // Extract aggFunc from __tree_agg column (first row)
  let treeAggFunc = 'sum';
  if (isTreeMode && response.data.length > 0) {
    const aggIdx = response.columns.indexOf('__tree_agg');
    if (aggIdx !== -1) treeAggFunc = String(response.data[0][aggIdx]);
  }

  // Create column definitions (exclude meta columns in tree mode)
  const visibleColumns = isTreeMode
    ? response.columns.filter((c) => !metaColumns.includes(c))
    : response.columns;

  const columnDefs = visibleColumns.map((col) => ({
    field: col,
    headerName: col,
    sortable: true,
    filter: true,
    resizable: true,
    ...(isTreeMode ? { aggFunc: treeAggFunc } : {}),
  }));

  // Convert columnar data to row objects (include ALL columns for getDataPath)
  const rowData = response.data.map((row) => {
    const obj: Record<string, unknown> = {};
    response.columns.forEach((col, i) => {
      obj[col] = row[i];
    });
    return obj;
  });

  // Create grid
  const gridOptions: GridOptions = {
    columnDefs,
    rowData,
    defaultColDef: {
      flex: 1,
      minWidth: 100,
      sortable: true,
      filter: true,
      resizable: true,
    },
    animateRows: true,
    rowSelection: 'single',
  };

  // AG Grid Enterprise Tree Data configuration
  if (isTreeMode) {
    Object.assign(gridOptions, {
      treeData: true,
      getDataPath: (data: any) => JSON.parse(data.__tree_path),
      autoGroupColumnDef: {
        headerName: 'Hierarchy',
        minWidth: 250,
        flex: 2,
        cellRendererParams: {
          suppressCount: false,
        },
      },
      groupDefaultExpanded: 1,
      suppressAggFuncInHeader: true,
    });
  }

  gridApi = createGrid(container, gridOptions);
}

/**
 * Update panel page info
 */
function updatePanelPageInfo(rows: number): void {
  totalRows = rows;
  const pageInfo = document.getElementById('panel-page-info');
  if (pageInfo) {
    if (currentTreeMode) {
      pageInfo.textContent = `${totalRows} rows (tree)`;
    } else {
      const totalPages = Math.max(1, Math.ceil(totalRows / PAGE_SIZE));
      pageInfo.textContent = `Page ${currentPage + 1} of ${totalPages} (${totalRows} rows)`;
    }
  }
  const prevBtn = document.getElementById('panel-prev-btn');
  const nextBtn = document.getElementById('panel-next-btn');
  if (prevBtn) prevBtn.style.display = currentTreeMode ? 'none' : '';
  if (nextBtn) nextBtn.style.display = currentTreeMode ? 'none' : '';
}

/**
 * Update modal page info
 */
function updateModalPageInfo(rows: number): void {
  totalRows = rows;
  const pageInfo = document.getElementById('page-info');
  if (pageInfo) {
    if (currentTreeMode) {
      pageInfo.textContent = `${totalRows} rows (tree)`;
    } else {
      const totalPages = Math.max(1, Math.ceil(totalRows / PAGE_SIZE));
      pageInfo.textContent = `Page ${currentPage + 1} of ${totalPages} (${totalRows} rows)`;
    }
  }
  const prevBtn = document.getElementById('prev-btn');
  const nextBtn = document.getElementById('next-btn');
  if (prevBtn) prevBtn.style.display = currentTreeMode ? 'none' : '';
  if (nextBtn) nextBtn.style.display = currentTreeMode ? 'none' : '';
}

/**
 * Go to previous page
 */
export function dfPrevPage(): void {
  if (currentPage > 0) {
    currentPage--;
    reloadCurrentView();
  }
}

/**
 * Go to next page
 */
export function dfNextPage(): void {
  const totalPages = Math.ceil(totalRows / PAGE_SIZE);
  if (currentPage < totalPages - 1) {
    currentPage++;
    reloadCurrentView();
  }
}

/**
 * Reload current view
 */
function reloadCurrentView(): void {
  if (panelIsOpen) {
    loadDataFrameToPanel();
  } else {
    loadDataFramePage();
  }
}

/**
 * Setup panel resizer
 */
function setupPanelResizer(): void {
  const resizer = document.getElementById('panel-resizer');
  const panel = document.getElementById('dataframe-panel');

  if (!resizer || !panel) return;

  resizer.addEventListener('mousedown', (e) => {
    isResizing = true;
    resizer.classList.add('active');
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
    e.preventDefault();
  });

  document.addEventListener('mousemove', (e) => {
    if (!isResizing) return;

    const containerWidth = window.innerWidth;
    const newWidth = containerWidth - e.clientX;
    const minWidth = 300;
    const maxWidth = containerWidth * 0.6;
    const width = Math.max(minWidth, Math.min(maxWidth, newWidth));

    panel.style.width = width + 'px';

    // Resize canvas
    window.dispatchEvent(new Event('resize'));
  });

  document.addEventListener('mouseup', () => {
    if (isResizing) {
      isResizing = false;
      resizer.classList.remove('active');
      document.body.style.cursor = '';
      document.body.style.userSelect = '';

      // Save width to localStorage
      localStorage.setItem(PANEL_WIDTH_KEY, panel.offsetWidth.toString());
    }
  });
}

/**
 * Restore panel width from localStorage
 */
function restorePanelWidth(): void {
  const savedWidth = localStorage.getItem(PANEL_WIDTH_KEY);
  if (savedWidth) {
    const panel = document.getElementById('dataframe-panel');
    if (panel) {
      panel.style.width = savedWidth + 'px';
    }
  }
}

/**
 * Check if split panel is open
 */
export function isPanelOpen(): boolean {
  return panelIsOpen;
}

/**
 * Handle window resize (switch between panel and modal)
 */
export function handleWindowResize(
  onSwitchToModal: (nodeId: string, portName: string, metadata: CsvMetadata) => void
): void {
  if (panelIsOpen && !isUltraWide() && currentNodeId && currentPortName && currentMetadata) {
    closeSplitPanel();
    onSwitchToModal(currentNodeId, currentPortName, currentMetadata);
  }
}
