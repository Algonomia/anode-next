/**
 * Type definitions for MCP server
 * Re-exports from AnodeClient with MCP-specific additions
 */

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
