import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

export interface OutputInfo {
  name: string;
  node_id: string;
  rows: number;
  columns: string[];
  execution_id: number;
  created_at: string;
  type?: string;
  metadata?: Record<string, unknown>;
}

export interface OutputsResponse {
  status: string;
  message?: string;
  outputs: OutputInfo[];
}

export interface FilterCondition {
  column: string;
  operator: '==' | '!=' | '<' | '<=' | '>' | '>=' | 'contains';
  value: string | number;
}

export interface OrderByCondition {
  column: string;
  order: 'asc' | 'desc';
}

export interface Operation {
  type: 'filter' | 'orderby' | 'groupby' | 'select';
  params: FilterCondition[] | OrderByCondition[] | unknown;
}

export interface OutputDataRequest {
  limit?: number;
  offset?: number;
  operations?: Operation[];
}

export interface OutputDataStats {
  total_rows: number;
  offset: number;
  returned_rows: number;
  duration_ms: number;
}

export interface ExecuteGraphResponse {
  status: string;
  session_id?: string;
  execution_id?: number;
  duration_ms: number;
  message?: string;
}

export interface OutputDataResponse {
  status: string;
  message?: string;
  columns: string[];
  data: unknown[][];
  stats: OutputDataStats;
  output?: {
    name: string;
    node_id: string;
    execution_id: number;
    created_at: string;
  };
}

@Injectable({ providedIn: 'root' })
export class AnodeApiService {
  private http = inject(HttpClient);

  getOutputs(slug: string): Observable<OutputsResponse> {
    return this.http.get<OutputsResponse>(
      `/api/graph/${encodeURIComponent(slug)}/outputs`
    );
  }

  getOutputData(
    slug: string,
    outputName: string,
    request: OutputDataRequest
  ): Observable<OutputDataResponse> {
    return this.http.post<OutputDataResponse>(
      `/api/graph/${encodeURIComponent(slug)}/output/${encodeURIComponent(outputName)}`,
      request
    );
  }

  executeGraph(
    slug: string,
    options?: {
      inputs?: Record<string, unknown>;
      skip_unknown_inputs?: boolean;
    }
  ): Observable<ExecuteGraphResponse> {
    const body: Record<string, unknown> = { apply_overrides: true };
    if (options?.inputs) {
      body['inputs'] = options.inputs;
    }
    if (options?.skip_unknown_inputs) {
      body['skip_unknown_inputs'] = true;
    }
    return this.http.post<ExecuteGraphResponse>(
      `/api/graph/${encodeURIComponent(slug)}/execute`,
      body
    );
  }
}