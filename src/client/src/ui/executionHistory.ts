/**
 * Execution History Modal
 * Shows past executions and allows restoring them
 */

import type {
  AnodeClient,
  ExecutionMetadata,
  CsvMetadata,
} from '../api/AnodeClient';

let modal: HTMLDivElement | null = null;
let client: AnodeClient | null = null;
let currentSlug: string = '';
let onRestoreCallback: ((sessionId: string, csvMetadata: Record<string, Record<string, CsvMetadata>>) => void) | null = null;

export function initExecutionHistory(
  apiClient: AnodeClient,
  restoreCallback: (sessionId: string, csvMetadata: Record<string, Record<string, CsvMetadata>>) => void
): void {
  client = apiClient;
  onRestoreCallback = restoreCallback;
}

export async function showExecutionHistory(slug: string): Promise<void> {
  currentSlug = slug;

  if (!client) {
    console.error('ExecutionHistory: client not initialized');
    return;
  }

  // Create modal if it doesn't exist
  if (!modal) {
    createModal();
  }

  // Show modal
  modal!.style.display = 'flex';

  // Load executions
  await loadExecutions();
}

export function hideExecutionHistory(): void {
  if (modal) {
    modal.style.display = 'none';
  }
}

function createModal(): void {
  modal = document.createElement('div');
  modal.className = 'execution-history-modal';
  modal.innerHTML = `
    <div class="execution-history-content">
      <div class="execution-history-header">
        <h2>Execution History</h2>
        <button class="close-btn">&times;</button>
      </div>
      <div class="execution-history-list"></div>
    </div>
  `;

  // Style
  const style = document.createElement('style');
  style.textContent = `
    .execution-history-modal {
      display: none;
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0, 0, 0, 0.6);
      justify-content: center;
      align-items: center;
      z-index: 10000;
    }

    .execution-history-content {
      background: #1e1e1e;
      border: 1px solid #444;
      border-radius: 8px;
      width: 600px;
      max-width: 90%;
      max-height: 80%;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }

    .execution-history-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 16px 20px;
      border-bottom: 1px solid #444;
      background: #252525;
    }

    .execution-history-header h2 {
      margin: 0;
      font-size: 18px;
      color: #e0e0e0;
    }

    .execution-history-header .close-btn {
      background: none;
      border: none;
      color: #888;
      font-size: 24px;
      cursor: pointer;
      padding: 0;
      line-height: 1;
    }

    .execution-history-header .close-btn:hover {
      color: #fff;
    }

    .execution-history-list {
      padding: 16px;
      overflow-y: auto;
      flex: 1;
    }

    .execution-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 16px;
      background: #2a2a2a;
      border: 1px solid #333;
      border-radius: 6px;
      margin-bottom: 8px;
      transition: background 0.2s;
    }

    .execution-item:hover {
      background: #333;
    }

    .execution-info {
      flex: 1;
    }

    .execution-date {
      font-size: 14px;
      color: #e0e0e0;
      margin-bottom: 4px;
    }

    .execution-details {
      font-size: 12px;
      color: #888;
    }

    .execution-restore-btn {
      background: #4a9eff;
      color: white;
      border: none;
      padding: 8px 16px;
      border-radius: 4px;
      cursor: pointer;
      font-size: 13px;
      transition: background 0.2s;
    }

    .execution-restore-btn:hover {
      background: #3a8eef;
    }

    .execution-restore-btn:disabled {
      background: #555;
      cursor: not-allowed;
    }

    .no-executions {
      text-align: center;
      color: #888;
      padding: 40px 20px;
    }

    .loading-executions {
      text-align: center;
      color: #888;
      padding: 40px 20px;
    }
  `;
  document.head.appendChild(style);

  // Close button handler
  modal.querySelector('.close-btn')?.addEventListener('click', hideExecutionHistory);

  // Click outside to close
  modal.addEventListener('click', (e) => {
    if (e.target === modal) {
      hideExecutionHistory();
    }
  });

  // ESC key to close
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && modal?.style.display === 'flex') {
      hideExecutionHistory();
    }
  });

  document.body.appendChild(modal);
}

async function loadExecutions(): Promise<void> {
  const listContainer = modal?.querySelector('.execution-history-list');
  if (!listContainer || !client) return;

  // Show loading
  listContainer.innerHTML = '<div class="loading-executions">Loading executions...</div>';

  try {
    const response = await client.listExecutions(currentSlug);

    if (response.executions.length === 0) {
      listContainer.innerHTML = '<div class="no-executions">No past executions found</div>';
      return;
    }

    // Render executions
    listContainer.innerHTML = response.executions
      .map((exec) => renderExecutionItem(exec))
      .join('');

    // Add click handlers for restore buttons
    listContainer.querySelectorAll('.execution-restore-btn').forEach((btn) => {
      btn.addEventListener('click', async (e) => {
        const executionId = parseInt((e.target as HTMLElement).dataset.id || '0', 10);
        if (executionId > 0) {
          await restoreExecution(executionId, e.target as HTMLButtonElement);
        }
      });
    });
  } catch (error) {
    console.error('Failed to load executions:', error);
    listContainer.innerHTML = '<div class="no-executions">Failed to load executions</div>';
  }
}

function renderExecutionItem(exec: ExecutionMetadata): string {
  const date = formatDate(exec.created_at);
  const duration = exec.duration_ms;
  const nodeCount = exec.node_count;
  const dfCount = exec.dataframe_count;

  return `
    <div class="execution-item">
      <div class="execution-info">
        <div class="execution-date">${date}</div>
        <div class="execution-details">
          ${nodeCount} nodes &bull; ${dfCount} DataFrames &bull; ${duration}ms
        </div>
      </div>
      <button class="execution-restore-btn" data-id="${exec.id}">Restore</button>
    </div>
  `;
}

function formatDate(isoString: string): string {
  try {
    const date = new Date(isoString);
    return date.toLocaleString(undefined, {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
    });
  } catch {
    return isoString;
  }
}

async function restoreExecution(executionId: number, button: HTMLButtonElement): Promise<void> {
  if (!client || !onRestoreCallback) return;

  // Disable button
  button.disabled = true;
  button.textContent = 'Restoring...';

  try {
    const response = await client.restoreExecution(executionId);

    if (response.status === 'ok') {
      // Close modal
      hideExecutionHistory();

      // Call callback to update UI
      onRestoreCallback(response.session_id, response.csv_metadata);
    } else {
      throw new Error('Failed to restore execution');
    }
  } catch (error) {
    console.error('Failed to restore execution:', error);
    button.disabled = false;
    button.textContent = 'Restore';
    alert('Failed to restore execution. Please try again.');
  }
}
