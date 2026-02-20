/**
 * AnodeClient wrapper for MCP server
 */

import type {
  NodeCatalogResponse,
  GraphListResponse,
  GraphResponse,
  CreateGraphRequest,
  CreateGraphResponse,
  UpdateGraphRequest,
  UpdateGraphResponse,
  ExecuteGraphResponse,
  SessionDataFrameRequest,
  SessionDataFrameResponse,
} from './types.js';

export class AnodeClient {
  private baseUrl: string;

  constructor(baseUrl: string = 'http://localhost:8080') {
    this.baseUrl = baseUrl.replace(/\/$/, '');
  }

  async getNodes(): Promise<NodeCatalogResponse> {
    const r = await fetch(`${this.baseUrl}/api/nodes`);
    if (!r.ok) throw new Error(`Failed to get nodes: ${r.statusText}`);
    return r.json() as Promise<NodeCatalogResponse>;
  }

  async listGraphs(): Promise<GraphListResponse> {
    const r = await fetch(`${this.baseUrl}/api/graphs`);
    if (!r.ok) throw new Error(`Failed to list graphs: ${r.statusText}`);
    return r.json() as Promise<GraphListResponse>;
  }

  async getGraph(slug: string): Promise<GraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`);
    if (!r.ok) throw new Error(`Failed to get graph: ${r.statusText}`);
    return r.json() as Promise<GraphResponse>;
  }

  async createGraph(data: CreateGraphRequest): Promise<CreateGraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    });
    if (!r.ok) throw new Error(`Failed to create graph: ${r.statusText}`);
    return r.json() as Promise<CreateGraphResponse>;
  }

  async updateGraph(slug: string, data: UpdateGraphRequest): Promise<UpdateGraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    });
    if (!r.ok) throw new Error(`Failed to update graph: ${r.statusText}`);
    return r.json() as Promise<UpdateGraphResponse>;
  }

  async deleteGraph(slug: string): Promise<{ status: string }> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`, {
      method: 'DELETE',
    });
    if (!r.ok) throw new Error(`Failed to delete graph: ${r.statusText}`);
    return r.json() as Promise<{ status: string }>;
  }

  async executeGraph(slug: string): Promise<ExecuteGraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}/execute`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    return r.json() as Promise<ExecuteGraphResponse>;
  }

  async querySessionDataFrame(
    sessionId: string,
    nodeId: string,
    portName: string,
    request: SessionDataFrameRequest = {}
  ): Promise<SessionDataFrameResponse> {
    const url = `${this.baseUrl}/api/session/${encodeURIComponent(sessionId)}/dataframe/${encodeURIComponent(nodeId)}/${encodeURIComponent(portName)}`;
    const r = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(request),
    });
    if (!r.ok) throw new Error(`Failed to query session dataframe: ${r.statusText}`);
    return r.json() as Promise<SessionDataFrameResponse>;
  }
}
