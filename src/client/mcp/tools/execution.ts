/**
 * Execution tools - execute_graph, query_dataframe
 */

import { z } from 'zod';
import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { AnodeClient } from '../client.js';
import {
  getCurrentGraph,
  getGraphState,
  setLastSessionId,
  getLastSessionId,
  markClean,
} from '../state.js';

export function registerExecutionTools(server: McpServer, client: AnodeClient): void {
  server.tool(
    'execute_graph',
    'Execute the current graph. Saves the graph first if it has a slug, then executes and returns results.',
    {
      slug: z.string().optional().describe('Graph slug to execute. Uses current graph slug if not provided.'),
    },
    async ({ slug }) => {
      try {
        const state = getGraphState();
        const targetSlug = slug || state.slug;

        if (!targetSlug) {
          return {
            content: [
              {
                type: 'text' as const,
                text: 'Error: No slug provided and no current graph loaded. Use create_graph first or provide a slug.',
              },
            ],
            isError: true,
          };
        }

        // Save current graph first if dirty
        if (state.dirty || !slug) {
          const graph = getCurrentGraph();
          await client.updateGraph(targetSlug, { graph });
          markClean();
        }

        // Execute
        const response = await client.executeGraph(targetSlug);

        if (response.status === 'error') {
          return {
            content: [
              {
                type: 'text' as const,
                text: JSON.stringify({
                  status: 'error',
                  message: response.message,
                }, null, 2),
              },
            ],
            isError: true,
          };
        }

        setLastSessionId(response.session_id);

        // Format results for readability
        const formattedResults: Record<string, Record<string, unknown>> = {};
        for (const [nodeId, ports] of Object.entries(response.results)) {
          formattedResults[nodeId] = {};
          for (const [portName, result] of Object.entries(ports)) {
            if (result.type === 'csv') {
              // For CSV results, show metadata instead of raw value
              const meta = response.csv_metadata[nodeId]?.[portName];
              formattedResults[nodeId][portName] = {
                type: 'csv',
                rows: meta?.rows,
                columns: meta?.columns,
                hint: `Use query_dataframe to view data: node_id="${nodeId}", port="${portName}"`,
              };
            } else {
              formattedResults[nodeId][portName] = result;
            }
          }
        }

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                status: response.status,
                session_id: response.session_id,
                duration_ms: response.duration_ms,
                results: formattedResults,
              }, null, 2),
            },
          ],
        };
      } catch (error) {
        return {
          content: [
            {
              type: 'text' as const,
              text: `Error: ${error instanceof Error ? error.message : String(error)}`,
            },
          ],
          isError: true,
        };
      }
    }
  );

  server.tool(
    'query_dataframe',
    'Query a DataFrame result from a graph execution. Use after execute_graph to view CSV/DataFrame outputs.',
    {
      session_id: z.string().optional().describe('Session ID from execute_graph. Uses last session if not provided.'),
      node_id: z.string().describe('The node ID that produced the DataFrame'),
      port: z.string().describe("The output port name (usually 'output' or 'value')"),
      limit: z.number().optional().describe('Maximum number of rows to return (default: 100)'),
      offset: z.number().optional().describe('Number of rows to skip (default: 0)'),
    },
    async ({ session_id, node_id, port, limit, offset }) => {
      try {
        const sessionIdToUse = session_id || getLastSessionId();

        if (!sessionIdToUse) {
          return {
            content: [
              {
                type: 'text' as const,
                text: 'Error: No session_id provided and no previous execution. Run execute_graph first.',
              },
            ],
            isError: true,
          };
        }

        const response = await client.querySessionDataFrame(
          sessionIdToUse,
          node_id,
          port,
          { limit: limit ?? 100, offset: offset ?? 0 }
        );

        if (response.status === 'error') {
          return {
            content: [
              {
                type: 'text' as const,
                text: JSON.stringify({
                  status: 'error',
                  message: response.message,
                }, null, 2),
              },
            ],
            isError: true,
          };
        }

        // Format data as a table for better readability
        const header = response.columns.join(' | ');
        const separator = response.columns.map(() => '---').join(' | ');
        const rows = response.data.map(row => row.map(cell => String(cell ?? '')).join(' | '));
        const table = [header, separator, ...rows].join('\n');

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                stats: response.stats,
                columns: response.columns,
                data_preview: table,
                raw_data: response.data,
              }, null, 2),
            },
          ],
        };
      } catch (error) {
        return {
          content: [
            {
              type: 'text' as const,
              text: `Error: ${error instanceof Error ? error.message : String(error)}`,
            },
          ],
          isError: true,
        };
      }
    }
  );
}
