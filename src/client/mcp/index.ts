/**
 * AnodeServer MCP Server
 *
 * Provides tools for Claude to interact with the AnodeServer graph editor:
 * - List and inspect node types
 * - Create, load, save, and delete graphs
 * - Add nodes, connect them, set properties
 * - Execute graphs and query results
 *
 * Usage:
 *   node dist/mcp/index.js
 *
 * Environment variables:
 *   ANODE_SERVER_URL - Base URL for AnodeServer (default: http://localhost:8080)
 *   MCP_WS_DISABLED - Set to "true" to disable WebSocket server
 */

import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { createServer } from './server.js';
import { startWebSocketServer } from './ws-server.js';

const baseUrl = process.env.ANODE_SERVER_URL || 'http://localhost:8080';
const wsDisabled = process.env.MCP_WS_DISABLED === 'true';

// Start WebSocket server for real-time sync with frontend
if (!wsDisabled) {
  startWebSocketServer();
}

const server = createServer(baseUrl);
const transport = new StdioServerTransport();

server.connect(transport).catch((error) => {
  console.error('Failed to start MCP server:', error);
  process.exit(1);
});
