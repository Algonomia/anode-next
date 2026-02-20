/**
 * AnodeClient - Client library for AnodeServer DataFrame API
 */

export interface FilterCondition {
  column: string;
  operator: '==' | '!=' | '<' | '<=' | '>' | '>=' | 'contains';
  value: string | number;
}

export interface OrderByCondition {
  column: string;
  order: 'asc' | 'desc';
}

export interface Aggregation {
  column: string;
  function: 'count' | 'sum' | 'avg' | 'min' | 'max';
  alias: string;
}

export interface GroupByParams {
  groupBy: string[];
  aggregations: Aggregation[];
}

export interface Operation {
  type: 'filter' | 'orderby' | 'groupby' | 'select';
  params: FilterCondition[] | OrderByCondition[] | GroupByParams | string[];
}

export interface QueryRequest {
  operations: Operation[];
  limit?: number;
  offset?: number;
}

export interface ColumnInfo {
  name: string;
  type: 'int' | 'double' | 'string';
}

export interface DatasetInfo {
  status: string;
  path: string;
  rows: number;
  columns: ColumnInfo[];
}

export interface QueryStats {
  input_rows: number;
  output_rows: number;
  offset: number;
  returned_rows: number;
  duration_ms: number;
}

export interface QueryResponse<T = Record<string, unknown>> {
  status: string;
  stats: QueryStats;
  data: T[];
}

export interface HealthResponse {
  status: string;
  service: string;
  version: string;
  dataset_loaded: boolean;
}

// === Node Catalog ===

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

// === Graph ===

export interface GraphMetadata {
  slug: string;
  name: string;
  description?: string;
  author?: string;
  tags?: string[];
  created_at: string;
  updated_at: string;
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

export interface AnodeGraph {
  nodes: AnodeNode[];
  connections: AnodeConnection[];
}

export interface GraphResponse {
  status: string;
  metadata: GraphMetadata;
  version: GraphVersion;
  graph: AnodeGraph;
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
}

export interface CsvMetadata {
  rows: number;
  columns: string[];
}

export interface ExecuteGraphResponse {
  status: string;
  session_id: string;
  results: Record<string, Record<string, { type: string; value: unknown }>>;
  csv_metadata: Record<string, Record<string, CsvMetadata>>;
  duration_ms: number;
}

export interface SessionDataFrameRequest {
  operations?: Operation[];
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
  stats: SessionDataFrameStats;
  columns: string[];
  data: unknown[][];
}

export class AnodeClient {
  private baseUrl: string;

  constructor(baseUrl: string = 'http://localhost:8080') {
    this.baseUrl = baseUrl.replace(/\/$/, '');
  }

  /**
   * Check server health
   */
  async health(): Promise<HealthResponse> {
    const response = await fetch(`${this.baseUrl}/api/health`);
    if (!response.ok) {
      throw new Error(`Health check failed: ${response.statusText}`);
    }
    return response.json();
  }

  /**
   * Get dataset information
   */
  async datasetInfo(): Promise<DatasetInfo> {
    const response = await fetch(`${this.baseUrl}/api/dataset/info`);
    if (!response.ok) {
      throw new Error(`Failed to get dataset info: ${response.statusText}`);
    }
    return response.json();
  }

  /**
   * Execute a query on the dataset
   */
  async query<T = Record<string, unknown>>(request: QueryRequest): Promise<QueryResponse<T>> {
    const response = await fetch(`${this.baseUrl}/api/dataset/query`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(request),
    });

    if (!response.ok) {
      throw new Error(`Query failed: ${response.statusText}`);
    }

    return response.json();
  }

  // === Node Catalog ===

  /**
   * Get all registered node definitions
   */
  async getNodes(): Promise<NodeCatalogResponse> {
    const response = await fetch(`${this.baseUrl}/api/nodes`);
    if (!response.ok) {
      throw new Error(`Failed to get nodes: ${response.statusText}`);
    }
    return response.json();
  }

  // === Graph CRUD ===

  /**
   * List all saved graphs
   */
  async listGraphs(): Promise<GraphListResponse> {
    const response = await fetch(`${this.baseUrl}/api/graphs`);
    if (!response.ok) {
      throw new Error(`Failed to list graphs: ${response.statusText}`);
    }
    return response.json();
  }

  /**
   * Get a graph by slug
   */
  async getGraph(slug: string): Promise<GraphResponse> {
    const response = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`);
    if (!response.ok) {
      throw new Error(`Failed to get graph: ${response.statusText}`);
    }
    return response.json();
  }

  /**
   * Create a new graph
   */
  async createGraph(data: CreateGraphRequest): Promise<CreateGraphResponse> {
    const response = await fetch(`${this.baseUrl}/api/graph`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    });
    if (!response.ok) {
      throw new Error(`Failed to create graph: ${response.statusText}`);
    }
    return response.json();
  }

  /**
   * Update a graph (save new version)
   */
  async updateGraph(slug: string, data: UpdateGraphRequest): Promise<UpdateGraphResponse> {
    const response = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    });
    if (!response.ok) {
      throw new Error(`Failed to update graph: ${response.statusText}`);
    }
    return response.json();
  }

  /**
   * Delete a graph
   */
  async deleteGraph(slug: string): Promise<void> {
    const response = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}`, {
      method: 'DELETE',
    });
    if (!response.ok) {
      throw new Error(`Failed to delete graph: ${response.statusText}`);
    }
  }

  /**
   * Execute a graph
   */
  async executeGraph(slug: string, versionId?: number): Promise<ExecuteGraphResponse> {
    const response = await fetch(`${this.baseUrl}/api/graph/${encodeURIComponent(slug)}/execute`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(versionId !== undefined ? { version_id: versionId } : {}),
    });
    if (!response.ok) {
      throw new Error(`Failed to execute graph: ${response.statusText}`);
    }
    return response.json();
  }

  // === Session DataFrame API ===

  /**
   * Query a DataFrame from an execution session
   * @param sessionId - The session ID returned from executeGraph
   * @param nodeId - The node ID (e.g., "node_1")
   * @param portName - The output port name (e.g., "csv")
   * @param request - Optional query parameters (operations, limit, offset)
   */
  async querySessionDataFrame(
    sessionId: string,
    nodeId: string,
    portName: string,
    request: SessionDataFrameRequest = {}
  ): Promise<SessionDataFrameResponse> {
    const url = `${this.baseUrl}/api/session/${encodeURIComponent(sessionId)}/dataframe/${encodeURIComponent(nodeId)}/${encodeURIComponent(portName)}`;
    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(request),
    });
    if (!response.ok) {
      throw new Error(`Failed to query session dataframe: ${response.statusText}`);
    }
    return response.json();
  }
}

/**
 * QueryBuilder - Fluent API for building queries
 */
export class QueryBuilder {
  private operations: Operation[] = [];
  private queryLimit?: number;
  private queryOffset?: number;

  /**
   * Add a filter operation
   */
  filter(conditions: FilterCondition[]): this {
    this.operations.push({
      type: 'filter',
      params: conditions,
    });
    return this;
  }

  /**
   * Add a single filter condition
   */
  where(column: string, operator: FilterCondition['operator'], value: string | number): this {
    return this.filter([{ column, operator, value }]);
  }

  /**
   * Add an order by operation
   */
  orderBy(conditions: OrderByCondition[]): this {
    this.operations.push({
      type: 'orderby',
      params: conditions,
    });
    return this;
  }

  /**
   * Add a single order by condition
   */
  sortBy(column: string, order: 'asc' | 'desc' = 'asc'): this {
    return this.orderBy([{ column, order }]);
  }

  /**
   * Add a group by operation with aggregations
   */
  groupBy(columns: string[], aggregations: Aggregation[]): this {
    this.operations.push({
      type: 'groupby',
      params: {
        groupBy: columns,
        aggregations,
      },
    });
    return this;
  }

  /**
   * Add a select operation
   */
  select(columns: string[]): this {
    this.operations.push({
      type: 'select',
      params: columns,
    });
    return this;
  }

  /**
   * Set the result limit
   */
  limit(count: number): this {
    this.queryLimit = count;
    return this;
  }

  /**
   * Set the offset for pagination
   */
  offset(count: number): this {
    this.queryOffset = count;
    return this;
  }

  /**
   * Build the query request
   */
  build(): QueryRequest {
    return {
      operations: this.operations,
      limit: this.queryLimit,
      offset: this.queryOffset,
    };
  }

  /**
   * Execute the query using the provided client
   */
  async execute<T = Record<string, unknown>>(client: AnodeClient): Promise<QueryResponse<T>> {
    return client.query<T>(this.build());
  }
}

// Factory function for creating a new QueryBuilder
export function createQuery(): QueryBuilder {
  return new QueryBuilder();
}

// Default export
export default AnodeClient;
