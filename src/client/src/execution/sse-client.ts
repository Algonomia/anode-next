/**
 * SSE client for real-time graph execution events
 */

import { setNodeStatus, resetExecutionState, type CsvOutputMetadata } from './state';

// Active EventSource connection
let eventSource: EventSource | null = null;

/**
 * Callbacks for execution events
 */
export interface ExecutionCallbacks {
  onStart?: (sessionId: string, nodeCount: number) => void;
  onNodeStarted?: (nodeId: string) => void;
  onNodeCompleted?: (nodeId: string, durationMs: number, csvMeta?: Record<string, CsvOutputMetadata>) => void;
  onNodeFailed?: (nodeId: string, durationMs: number, error: string) => void;
  onComplete?: (sessionId: string, hasErrors: boolean) => void;
  onError?: (error: string) => void;
}

/**
 * Execute a graph with real-time streaming feedback
 */
export function executeGraphWithStream(
  baseUrl: string,
  slug: string,
  callbacks: ExecutionCallbacks
): void {
  // Close any existing connection
  if (eventSource) {
    eventSource.close();
    eventSource = null;
  }

  // Reset execution state
  resetExecutionState();

  // Create SSE connection
  // Note: EventSource only supports GET, so we need to use fetch + ReadableStream for POST
  // Using POST via fetch with streaming response
  const url = `${baseUrl}/api/graph/${encodeURIComponent(slug)}/execute-stream`;

  fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({}),
  })
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error: ${response.status}`);
      }
      if (!response.body) {
        throw new Error('No response body');
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';

      function processChunk(): Promise<void> {
        return reader.read().then(({ done, value }) => {
          if (done) {
            return;
          }

          buffer += decoder.decode(value, { stream: true });

          // Process complete SSE messages
          const lines = buffer.split('\n');
          buffer = lines.pop() || ''; // Keep incomplete line in buffer

          let currentEvent = '';
          let currentData = '';

          for (const line of lines) {
            if (line.startsWith('event: ')) {
              currentEvent = line.substring(7);
            } else if (line.startsWith('data: ')) {
              currentData = line.substring(6);
            } else if (line === '' && currentEvent && currentData) {
              // End of event
              processEvent(currentEvent, currentData, callbacks);
              currentEvent = '';
              currentData = '';
            }
          }

          return processChunk();
        });
      }

      return processChunk();
    })
    .catch(error => {
      callbacks.onError?.(error.message || 'Connection failed');
    });
}

/**
 * Process a single SSE event
 */
function processEvent(eventType: string, data: string, callbacks: ExecutionCallbacks): void {
  try {
    const parsed = JSON.parse(data);

    switch (eventType) {
      case 'execution_start':
        callbacks.onStart?.(parsed.session_id, parsed.node_count);
        break;

      case 'node_started':
        setNodeStatus(parsed.node_id, { status: 'running' });
        callbacks.onNodeStarted?.(parsed.node_id);
        break;

      case 'node_completed':
        setNodeStatus(parsed.node_id, {
          status: 'success',
          durationMs: parsed.duration_ms,
          csvMetadata: parsed.csv_metadata,
        });
        callbacks.onNodeCompleted?.(parsed.node_id, parsed.duration_ms, parsed.csv_metadata);
        break;

      case 'node_failed':
        setNodeStatus(parsed.node_id, {
          status: 'error',
          durationMs: parsed.duration_ms,
          errorMessage: parsed.error_message,
        });
        callbacks.onNodeFailed?.(parsed.node_id, parsed.duration_ms, parsed.error_message);
        break;

      case 'execution_complete':
        callbacks.onComplete?.(parsed.session_id, parsed.has_errors);
        break;

      case 'error':
        callbacks.onError?.(parsed.message);
        break;
    }
  } catch (e) {
    console.error('Failed to parse SSE event:', eventType, data, e);
  }
}

/**
 * Cancel any ongoing execution stream
 */
export function cancelExecutionStream(): void {
  if (eventSource) {
    eventSource.close();
    eventSource = null;
  }
}
