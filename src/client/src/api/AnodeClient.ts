/**
 * AnodeClient - HTTP client for AnodeServer API
 */

// === Types ===

export interface NodeInputDef {
  name: string;
  types: string[];
  required: boolean;
}

export interface NodeOutputDef {
  name: string;
  types: string[];
}

export interface NodeDef {
  name: string;
  category: string;
  isEntryPoint: boolean;
  inputs: NodeInputDef[];
  outputs: NodeOutputDef[];
}

export interface NodeCatalogResponse {
  status: string;
  nodes: NodeDef[];
  categories: string[];
}

export interface GraphMetadata {
  slug: string;
  name: string;
  description?: string;
  author?: string;
  tags?: string[];
  created_at: string;
  updated_at: string;
  links?: GraphLinks;
}

export interface GraphVersion {
  id: number;
  version_name?: string;
  created_at: string;
}

export interface AnodeNode {
  id: string;
  type: string;
  properties: Record<string, { value: unknown; type: string }>;
  position?: [number, number];
}

export interface AnodeConnection {
  from: string;
  fromPort: string;
  to: string;
  toPort: string;
}

export interface AnodeGroup {
  title: string;
  bounding: [number, number, number, number]; // x, y, width, height
  color?: string;
  font_size?: number;
}

export interface AnodeGraph {
  nodes: AnodeNode[];
  connections: AnodeConnection[];
  groups?: AnodeGroup[];
}

export interface GraphLinks {
  outgoing: string[];
  incoming: string[];
}

export interface GraphResponse {
  status: string;
  metadata: GraphMetadata;
  version: GraphVersion;
  graph: AnodeGraph;
  links?: GraphLinks;
}

export interface GraphListResponse {
  status: string;
  graphs: GraphMetadata[];
}

export interface CreateGraphRequest {
  slug: string;
  name: string;
  description?: string;
  author?: string;
  tags?: string[];
  graph?: AnodeGraph;
}

export interface CreateGraphResponse {
  status: string;
  slug: string;
  version_id: number;
}

export interface UpdateGraphRequest {
  version_name?: string;
  graph: AnodeGraph;
}

export interface UpdateGraphResponse {
  status: string;
  version_id: number;
  links?: GraphLinks;
}

export interface CsvMetadata {
  rows: number;
  columns: string[];
}

export interface ExecuteGraphResponse {
  status: string;
  message?: string;
  session_id: string;
  results: Record<string, Record<string, { type: string; value: unknown }>>;
  csv_metadata: Record<string, Record<string, CsvMetadata>>;
  duration_ms: number;
}

export interface SessionDataFrameRequest {
  limit?: number;
  offset?: number;
}

export interface SessionDataFrameStats {
  total_rows: number;
  offset: number;
  returned_rows: number;
  duration_ms: number;
}

export interface SessionDataFrameResponse {
  status: string;
  message?: string;
  stats: SessionDataFrameStats;
  columns: string[];
  data: unknown[][];
}

// === Execution Persistence Types ===

export interface ExecutionMetadata {
  id: number;
  session_id: string;
  version_id: number;
  created_at: string;
  duration_ms: number;
  node_count: number;
  dataframe_count: number;
}

export interface ListExecutionsResponse {
  status: string;
  slug: string;
  executions: ExecutionMetadata[];
}

export interface GetExecutionResponse {
  status: string;
  execution: ExecutionMetadata & { graph_slug: string };
  csv_metadata: Record<string, Record<string, CsvMetadata>>;
}

export interface RestoreExecutionResponse {
  status: string;
  session_id: string;
  execution_id: number;
  csv_metadata: Record<string, Record<string, CsvMetadata>>;
}

// === Client ===

export class AnodeClient {
  private baseUrl: string;

  constructor(baseUrl: string = '') {
    this.baseUrl = baseUrl.replace(/\/$/, '');
  }

  async getNodes(): Promise<NodeCatalogResponse> {
    const r = await fetch(`${this.baseUrl}/api/nodes`);
    if (!r.ok) throw new Error(`Failed to get nodes: ${r.statusText}`);
    return r.json();
  }

  async listGraphs(): Promise<GraphListResponse> {
    const r = await fetch(`${this.baseUrl}/api/graphs`);
    if (!r.ok) throw new Error(`Failed to list graphs: ${r.statusText}`);
    return r.json();
  }

  async getGraph(slug: string): Promise<GraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`);
    if (!r.ok) throw new Error(`Failed to get graph: ${r.statusText}`);
    return r.json();
  }

  async createGraph(data: CreateGraphRequest): Promise<CreateGraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    });
    if (!r.ok) throw new Error(`Failed to create graph: ${r.statusText}`);
    return r.json();
  }

  async updateGraph(slug: string, data: UpdateGraphRequest): Promise<UpdateGraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    });
    if (!r.ok) throw new Error(`Failed to update graph: ${r.statusText}`);
    return r.json();
  }

  async executeGraph(slug: string): Promise<ExecuteGraphResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}/execute`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    // Always return JSON response (even for errors, server returns JSON with message)
    return r.json();
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
    return r.json();
  }

  // === Execution Persistence API ===

  async listExecutions(slug: string): Promise<ListExecutionsResponse> {
    const r = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}/executions`);
    if (!r.ok) throw new Error(`Failed to list executions: ${r.statusText}`);
    return r.json();
  }

  async getExecution(executionId: number): Promise<GetExecutionResponse> {
    const r = await fetch(`${this.baseUrl}/api/execution/${executionId}`);
    if (!r.ok) throw new Error(`Failed to get execution: ${r.statusText}`);
    return r.json();
  }

  async restoreExecution(executionId: number): Promise<RestoreExecutionResponse> {
    const r = await fetch(`${this.baseUrl}/api/execution/${executionId}/restore`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    if (!r.ok) throw new Error(`Failed to restore execution: ${r.statusText}`);
    return r.json();
  }
}
