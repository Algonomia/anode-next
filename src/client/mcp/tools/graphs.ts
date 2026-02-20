/**
 * Graph management tools - list_graphs, get_graph, create_graph, save_graph, delete_graph
 */

import { z } from 'zod';
import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { AnodeClient } from '../client.js';
import {
  getCurrentGraph,
  getGraphState,
  loadGraph,
  newGraph,
  setSlug,
  markClean,
} from '../state.js';

export function registerGraphTools(server: McpServer, client: AnodeClient): void {
  server.tool(
    'list_graphs',
    'List all saved graphs in the database.',
    {},
    async () => {
      try {
        const response = await client.listGraphs();

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                graphs: response.graphs.map(g => ({
                  slug: g.slug,
                  name: g.name,
                  description: g.description,
                  author: g.author,
                  tags: g.tags,
                  created_at: g.created_at,
                  updated_at: g.updated_at,
                })),
                count: response.graphs.length,
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
    'get_graph',
    'Load a graph from the database by its slug. This replaces the current in-memory graph.',
    {
      slug: z.string().describe('The unique identifier (slug) of the graph to load'),
    },
    async ({ slug }) => {
      try {
        const response = await client.getGraph(slug);
        loadGraph(response.graph, slug, response.metadata.name);

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Graph '${slug}' loaded successfully`,
                metadata: response.metadata,
                version: response.version,
                graph: {
                  nodes_count: response.graph.nodes.length,
                  connections_count: response.graph.connections.length,
                  nodes: response.graph.nodes,
                  connections: response.graph.connections,
                },
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
    'create_graph',
    'Create a new graph in the database. Optionally saves the current in-memory graph or starts fresh.',
    {
      slug: z.string().describe('Unique identifier for the graph (URL-friendly)'),
      name: z.string().describe('Display name of the graph'),
      description: z.string().optional().describe('Description of the graph'),
      author: z.string().optional().describe('Author name'),
      tags: z.array(z.string()).optional().describe('List of tags'),
      use_current: z.boolean().optional().describe('If true, save the current in-memory graph. Otherwise create an empty graph.'),
    },
    async ({ slug, name, description, author, tags, use_current }) => {
      try {
        const graph = use_current ? getCurrentGraph() : { nodes: [], connections: [] };

        const response = await client.createGraph({
          slug,
          name,
          description,
          author,
          tags,
          graph,
        });

        // Update state to point to the new graph
        setSlug(slug, name);
        markClean();

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Graph '${slug}' created successfully`,
                slug: response.slug,
                version_id: response.version_id,
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
    'save_graph',
    'Save the current in-memory graph to the database. Creates a new version of an existing graph.',
    {
      slug: z.string().optional().describe('Graph slug. Uses current graph slug if not provided.'),
      version_name: z.string().optional().describe('Optional name for this version'),
    },
    async ({ slug, version_name }) => {
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

        const graph = getCurrentGraph();
        const response = await client.updateGraph(targetSlug, {
          version_name,
          graph,
        });

        markClean();

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Graph '${targetSlug}' saved successfully`,
                version_id: response.version_id,
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
    'delete_graph',
    'Delete a graph from the database.',
    {
      slug: z.string().describe('The slug of the graph to delete'),
    },
    async ({ slug }) => {
      try {
        await client.deleteGraph(slug);

        // If we deleted the current graph, clear state
        const state = getGraphState();
        if (state.slug === slug) {
          newGraph();
        }

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Graph '${slug}' deleted successfully`,
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
    'new_graph',
    'Clear the current in-memory graph and start fresh. Does not affect saved graphs.',
    {},
    async () => {
      newGraph();

      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify({
              message: 'New empty graph created',
              graph: {
                nodes: [],
                connections: [],
              },
            }, null, 2),
          },
        ],
      };
    }
  );
}
