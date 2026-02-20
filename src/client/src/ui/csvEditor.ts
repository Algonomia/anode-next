/**
 * CSV Editor modal using AG Grid
 * Allows editing CSV data directly in the graph editor
 */

import { createGrid, type GridApi, type GridOptions, type ColDef, type IHeaderComp, type IHeaderParams } from 'ag-grid-community';

/**
 * Custom header component that allows renaming columns on double-click
 */
class EditableHeader implements IHeaderComp {
  private eGui!: HTMLDivElement;
  private params!: IHeaderParams;

  init(params: IHeaderParams): void {
    this.params = params;

    this.eGui = document.createElement('div');
    this.eGui.classList.add('csv-editable-header');
    this.eGui.textContent = params.displayName;
    this.eGui.title = 'Double-click to rename';

    this.eGui.addEventListener('dblclick', (e) => {
      e.stopPropagation();
      this.startEditing();
    });
  }

  getGui(): HTMLElement {
    return this.eGui;
  }

  refresh(): boolean {
    return false;
  }

  destroy(): void {}

  private startEditing(): void {
    const oldName = this.params.displayName;

    const input = document.createElement('input');
    input.type = 'text';
    input.value = oldName;
    input.classList.add('csv-header-input');

    this.eGui.textContent = '';
    this.eGui.appendChild(input);
    input.focus();
    input.select();

    const finish = () => {
      const newName = input.value.trim();
      if (newName && newName !== oldName) {
        renameColumn(oldName, newName);
      } else {
        this.eGui.textContent = oldName;
      }
    };

    input.addEventListener('blur', finish);
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') { input.blur(); }
      if (e.key === 'Escape') { input.value = oldName; input.blur(); }
    });
  }
}

/** Serialized CSV data format (matches DataFrameSerializer JSON) */
export interface CsvData {
  columns: string[];
  schema: { name: string; type: string }[];
  data: (string | number | null)[][];
}

// State
let gridApi: GridApi | null = null;
let currentColumns: string[] = [];
let currentData: Record<string, unknown>[] = [];
let onSaveCallback: ((data: CsvData) => void) | null = null;

/**
 * Open the CSV editor modal with optional existing data
 */
export function openCsvEditor(
  existingData: CsvData | null | undefined,
  onSave: (data: CsvData) => void
): void {
  onSaveCallback = onSave;

  if (existingData && existingData.columns && existingData.data) {
    currentColumns = [...existingData.columns];
    currentData = existingData.data.map(row => {
      const obj: Record<string, unknown> = {};
      existingData.columns.forEach((col, i) => {
        obj[col] = row[i] ?? '';
      });
      return obj;
    });
  } else {
    // Start with a default 3x3 grid
    currentColumns = ['col_1', 'col_2', 'col_3'];
    currentData = [
      { col_1: '', col_2: '', col_3: '' },
    ];
  }

  renderGrid();
  showModal();
}

/**
 * Render AG Grid in the editor container
 */
function renderGrid(): void {
  if (gridApi) {
    gridApi.destroy();
    gridApi = null;
  }

  const container = document.getElementById('csv-editor-grid');
  if (!container) return;
  container.innerHTML = '';

  const columnDefs: ColDef[] = currentColumns.map(col => ({
    field: col,
    headerName: col,
    headerComponent: EditableHeader,
    editable: true,
    sortable: false,
    filter: false,
    resizable: true,
    flex: 1,
    minWidth: 80,
  }));

  const gridOptions: GridOptions = {
    columnDefs,
    rowData: currentData,
    defaultColDef: {
      editable: true,
      resizable: true,
    },
    singleClickEdit: true,
    stopEditingWhenCellsLoseFocus: true,
    rowSelection: 'single',
  };

  gridApi = createGrid(container, gridOptions);
  updateInfo();
}

/**
 * Rename a column: update currentColumns and all row data, then re-render
 */
function renameColumn(oldName: string, newName: string): void {
  if (!gridApi) return;
  if (currentColumns.includes(newName)) {
    alert(`Column "${newName}" already exists`);
    return;
  }

  gridApi.stopEditing();

  // Collect current data before modifying
  const rows: Record<string, unknown>[] = [];
  gridApi.forEachNode(node => { rows.push({ ...node.data }); });

  // Rename in columns array
  const idx = currentColumns.indexOf(oldName);
  if (idx === -1) return;
  currentColumns[idx] = newName;

  // Rename in row data
  rows.forEach(row => {
    row[newName] = row[oldName];
    delete row[oldName];
  });
  currentData = rows;

  renderGrid();
}

/**
 * Show the modal
 */
function showModal(): void {
  const modal = document.getElementById('csv-editor-modal');
  if (modal) modal.classList.add('active');
}

/**
 * Close the modal
 */
function closeModal(): void {
  const modal = document.getElementById('csv-editor-modal');
  if (modal) modal.classList.remove('active');

  if (gridApi) {
    gridApi.destroy();
    gridApi = null;
  }
}

/**
 * Collect current grid data and return as CsvData
 */
function collectData(): CsvData {
  const rows: (string | number | null)[][] = [];

  if (gridApi) {
    gridApi.stopEditing();
    gridApi.forEachNode(node => {
      const row: (string | number | null)[] = currentColumns.map(col => {
        const val = node.data[col];
        if (val === null || val === undefined || val === '') return '';
        return val;
      });
      rows.push(row);
    });
  }

  // Infer schema from data
  const schema = currentColumns.map((name, colIdx) => {
    let allInt = true;
    let allNum = true;
    for (const row of rows) {
      const val = row[colIdx];
      if (val === null || val === undefined || val === '') continue;
      const str = String(val);
      if (allInt && !/^-?\d+$/.test(str)) allInt = false;
      if (allNum && isNaN(Number(str))) allNum = false;
      if (!allNum) break;
    }
    return { name, type: allInt ? 'INT' : allNum ? 'DOUBLE' : 'STRING' };
  });

  // Convert values to typed
  const typedData = rows.map(row =>
    row.map((val, colIdx) => {
      const t = schema[colIdx].type;
      const str = String(val ?? '');
      if (str === '') return t === 'STRING' ? '' : 0;
      if (t === 'INT') return parseInt(str, 10) || 0;
      if (t === 'DOUBLE') return parseFloat(str) || 0.0;
      return str;
    })
  );

  return { columns: [...currentColumns], schema, data: typedData };
}

/**
 * Add a new row at the end
 */
function addRow(): void {
  if (!gridApi) return;
  gridApi.stopEditing();

  const newRow: Record<string, unknown> = {};
  currentColumns.forEach(col => { newRow[col] = ''; });
  currentData.push(newRow);

  gridApi.applyTransaction({ add: [newRow] });
  updateInfo();
}

/**
 * Remove the last row
 */
function removeRow(): void {
  if (!gridApi || currentData.length === 0) return;
  gridApi.stopEditing();

  const removed = currentData.pop();
  if (removed) {
    gridApi.applyTransaction({ remove: [removed] });
  }
  updateInfo();
}

/**
 * Add a new column
 */
function addColumn(): void {
  if (!gridApi) return;
  gridApi.stopEditing();

  // Collect current data before modifying columns
  const rows: Record<string, unknown>[] = [];
  gridApi.forEachNode(node => { rows.push({ ...node.data }); });

  // Find next available column name
  let idx = currentColumns.length + 1;
  let name = `col_${idx}`;
  while (currentColumns.includes(name)) {
    idx++;
    name = `col_${idx}`;
  }

  currentColumns.push(name);

  // Add the new column to existing row data
  rows.forEach(row => { row[name] = ''; });
  currentData = rows;

  renderGrid();
}

/**
 * Remove the last column
 */
function removeColumn(): void {
  if (!gridApi || currentColumns.length <= 1) return;
  gridApi.stopEditing();

  // Collect current data
  const rows: Record<string, unknown>[] = [];
  gridApi.forEachNode(node => { rows.push({ ...node.data }); });

  const removed = currentColumns.pop()!;

  // Remove column from row data
  rows.forEach(row => { delete row[removed]; });
  currentData = rows;

  renderGrid();
}

/**
 * Import CSV from file
 */
function importCsv(file: File): void {
  file.text().then(text => {
    const rows = parseCsv(text);
    if (rows.length < 2) {
      alert('CSV must have a header and at least one data row');
      return;
    }

    const header = rows[0];
    const dataRows = rows.slice(1);

    currentColumns = header;
    currentData = dataRows.map(row => {
      const obj: Record<string, unknown> = {};
      header.forEach((col, i) => {
        obj[col] = row[i] ?? '';
      });
      return obj;
    });

    renderGrid();
  });
}

/**
 * Update the info display
 */
function updateInfo(): void {
  const info = document.getElementById('csv-editor-info');
  if (!info) return;

  let rowCount = 0;
  if (gridApi) {
    gridApi.forEachNode(() => { rowCount++; });
  }
  info.textContent = `${rowCount} row${rowCount !== 1 ? 's' : ''} × ${currentColumns.length} col${currentColumns.length !== 1 ? 's' : ''}`;
}

/**
 * Parse CSV text into rows of strings
 */
function parseCsv(text: string): string[][] {
  const rows: string[][] = [];
  let current = '';
  let inQuotes = false;
  let row: string[] = [];

  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (inQuotes) {
      if (ch === '"') {
        if (i + 1 < text.length && text[i + 1] === '"') {
          current += '"';
          i++;
        } else {
          inQuotes = false;
        }
      } else {
        current += ch;
      }
    } else if (ch === '"') {
      inQuotes = true;
    } else if (ch === ',' || ch === ';') {
      row.push(current.trim());
      current = '';
    } else if (ch === '\n' || (ch === '\r' && text[i + 1] === '\n')) {
      row.push(current.trim());
      current = '';
      if (row.length > 0 && !(row.length === 1 && row[0] === '')) rows.push(row);
      row = [];
      if (ch === '\r') i++;
    } else {
      current += ch;
    }
  }
  row.push(current.trim());
  if (row.length > 0 && !(row.length === 1 && row[0] === '')) rows.push(row);
  return rows;
}

/**
 * Initialize event listeners for the CSV editor modal buttons
 * Should be called once at app startup
 */
export function initCsvEditor(): void {
  document.getElementById('csv-add-row')?.addEventListener('click', addRow);
  document.getElementById('csv-remove-row')?.addEventListener('click', removeRow);
  document.getElementById('csv-add-col')?.addEventListener('click', addColumn);
  document.getElementById('csv-remove-col')?.addEventListener('click', removeColumn);

  document.getElementById('csv-import-input')?.addEventListener('change', (e) => {
    const input = e.target as HTMLInputElement;
    const file = input.files?.[0];
    if (file) {
      importCsv(file);
      input.value = ''; // Reset so same file can be re-imported
    }
  });

  document.getElementById('csv-editor-save')?.addEventListener('click', () => {
    if (onSaveCallback) {
      onSaveCallback(collectData());
    }
    closeModal();
  });

  document.getElementById('csv-editor-cancel')?.addEventListener('click', () => {
    closeModal();
  });
}
