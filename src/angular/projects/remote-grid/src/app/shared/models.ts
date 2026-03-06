export type VizType = 'grid' | 'list' | 'timeline' | 'diff' | 'chart' | 'button';

export const NON_GRID_TYPES: string[] = ['timeline', 'diff', 'chart', 'list', 'button'];

export interface ListItem {
  label: string;
  value: string;
  index: number;
}

export interface ButtonItem {
  label: string;
  name: string;
  index: number;
  hasEvent: boolean;
}

export interface DrilldownEvent {
  rowData: Record<string, unknown>;
}

export interface StatusEvent {
  message: string;
  type: string;
}
