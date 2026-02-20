/**
 * Catalog tools - list_nodes, get_node_info
 */

import { z } from 'zod';
import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { AnodeClient } from '../client.js';
import { setNodeCatalog, getNodeCatalog, getNodeDef } from '../state.js';

export function registerCatalogTools(server: McpServer, client: AnodeClient): void {
  server.tool(
    'list_nodes',
    'List all available node types in the catalog. Returns node types grouped by category.',
    {},
    async () => {
      try {
        const response = await client.getNodes();
        setNodeCatalog(response.nodes);

        // Group by category
        const byCategory: Record<string, string[]> = {};
        for (const node of response.nodes) {
          const type = `${node.category}/${node.name}`;
          if (!byCategory[node.category]) {
            byCategory[node.category] = [];
          }
          byCategory[node.category].push(type);
        }

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                categories: response.categories,
                nodes_by_category: byCategory,
                total_nodes: response.nodes.length,
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
    'get_node_info',
    'Get detailed information about a specific node type, including its inputs, outputs, and whether it is an entry point.',
    {
      type: z.string().describe("Node type in format 'category/name' (e.g., 'scalar/int_value', 'csv/csv_source')"),
    },
    async ({ type }) => {
      try {
        // Ensure catalog is loaded
        if (getNodeCatalog().length === 0) {
          const response = await client.getNodes();
          setNodeCatalog(response.nodes);
        }

        const nodeDef = getNodeDef(type);
        if (!nodeDef) {
          return {
            content: [
              {
                type: 'text' as const,
                text: `Error: Node type '${type}' not found in catalog`,
              },
            ],
            isError: true,
          };
        }

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                type,
                name: nodeDef.name,
                category: nodeDef.category,
                is_entry_point: nodeDef.isEntryPoint,
                inputs: nodeDef.inputs.map(i => ({
                  name: i.name,
                  types: i.types,
                  required: i.required,
                })),
                outputs: nodeDef.outputs.map(o => ({
                  name: o.name,
                  types: o.types,
                })),
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
