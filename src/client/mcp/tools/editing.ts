/**
 * Graph editing tools - add_node, remove_node, connect_nodes, disconnect_nodes, set_property, get_current_graph
 */

import { z } from 'zod';
import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import {
  addNode,
  removeNode,
  getNode,
  setProperty,
  connect,
  disconnect,
  getCurrentGraph,
  getGraphState,
  listConnections,
} from '../state.js';

export function registerEditingTools(server: McpServer): void {
  server.tool(
    'add_node',
    'Add a node to the current in-memory graph.',
    {
      type: z.string().describe("Node type in format 'category/name' (e.g., 'scalar/int_value', 'math/add')"),
      id: z.string().optional().describe('Optional custom node ID. Auto-generated if not provided.'),
      position: z.tuple([z.number(), z.number()]).optional().describe('Optional [x, y] position on canvas'),
      properties: z.record(z.unknown()).optional().describe('Optional initial property values (e.g., { _value: 5 })'),
    },
    async ({ type, id, position, properties }) => {
      try {
        const node = addNode(type, id, position, properties);

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Node '${node.id}' added successfully`,
                node,
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
    'remove_node',
    'Remove a node from the current graph. Also removes all connections to/from this node.',
    {
      id: z.string().describe('The ID of the node to remove'),
    },
    async ({ id }) => {
      try {
        const removed = removeNode(id);

        if (!removed) {
          return {
            content: [
              {
                type: 'text' as const,
                text: `Error: Node '${id}' not found`,
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
                message: `Node '${id}' removed successfully`,
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
    'connect_nodes',
    'Connect two nodes by creating a link from an output port to an input port.',
    {
      from_node: z.string().describe('Source node ID'),
      from_port: z.string().describe("Source output port name (e.g., 'value', 'output')"),
      to_node: z.string().describe('Target node ID'),
      to_port: z.string().describe("Target input port name (e.g., 'a', 'b', 'input')"),
    },
    async ({ from_node, from_port, to_node, to_port }) => {
      try {
        const connection = connect(from_node, from_port, to_node, to_port);

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Connected ${from_node}.${from_port} -> ${to_node}.${to_port}`,
                connection,
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
    'disconnect_nodes',
    'Remove a connection between two nodes.',
    {
      from_node: z.string().describe('Source node ID'),
      from_port: z.string().describe('Source output port name'),
      to_node: z.string().describe('Target node ID'),
      to_port: z.string().describe('Target input port name'),
    },
    async ({ from_node, from_port, to_node, to_port }) => {
      try {
        const disconnected = disconnect(from_node, from_port, to_node, to_port);

        if (!disconnected) {
          return {
            content: [
              {
                type: 'text' as const,
                text: `Error: Connection ${from_node}.${from_port} -> ${to_node}.${to_port} not found`,
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
                message: `Disconnected ${from_node}.${from_port} -> ${to_node}.${to_port}`,
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
    'set_property',
    "Set a property value on a node. Use '_value' for value nodes like int_value, string_value.",
    {
      node_id: z.string().describe('The ID of the node'),
      property: z.string().describe("Property name (e.g., '_value', '_path')"),
      value: z.unknown().describe('The value to set'),
    },
    async ({ node_id, property, value }) => {
      try {
        const success = setProperty(node_id, property, value);

        if (!success) {
          return {
            content: [
              {
                type: 'text' as const,
                text: `Error: Node '${node_id}' not found`,
              },
            ],
            isError: true,
          };
        }

        const node = getNode(node_id);

        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify({
                message: `Property '${property}' set on node '${node_id}'`,
                node,
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
    'get_current_graph',
    'Get the current in-memory graph state, including all nodes and connections.',
    {},
    async () => {
      const state = getGraphState();
      const graph = getCurrentGraph();

      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify({
              slug: state.slug,
              name: state.name,
              dirty: state.dirty,
              last_session_id: state.lastSessionId,
              graph: {
                nodes_count: graph.nodes.length,
                connections_count: graph.connections.length,
                nodes: graph.nodes,
                connections: graph.connections,
              },
            }, null, 2),
          },
        ],
      };
    }
  );

  server.tool(
    'get_node',
    'Get details of a specific node by ID.',
    {
      id: z.string().describe('The ID of the node'),
    },
    async ({ id }) => {
      const node = getNode(id);

      if (!node) {
        return {
          content: [
            {
              type: 'text' as const,
              text: `Error: Node '${id}' not found`,
            },
          ],
          isError: true,
        };
      }

      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify({ node }, null, 2),
          },
        ],
      };
    }
  );

  server.tool(
    'list_connections',
    'List all connections in the current graph.',
    {},
    async () => {
      const connections = listConnections();

      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify({
              connections_count: connections.length,
              connections,
            }, null, 2),
          },
        ],
      };
    }
  );
}
