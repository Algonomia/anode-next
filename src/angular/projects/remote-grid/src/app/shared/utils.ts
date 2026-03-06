import type { SortModelItem } from 'ag-grid-community';
import type {
  Operation,
  FilterCondition,
  OrderByCondition,
} from '../services/anode-api.service';

export function columnarToRows(
  columns: string[],
  data: unknown[][],
): Record<string, unknown>[] {
  return data.map((row) => {
    const obj: Record<string, unknown> = {};
    columns.forEach((col, i) => {
      obj[col] = row[i];
    });
    return obj;
  });
}

export function buildOperations(
  sortModel: SortModelItem[],
  filterModel: any,
): Operation[] {
  const operations: Operation[] = [];

  if (filterModel && Object.keys(filterModel).length > 0) {
    const filterParams: FilterCondition[] = [];
    for (const [column, filter] of Object.entries(filterModel) as [
      string,
      any,
    ][]) {
      if (filter.filterType === 'text') {
        const op: FilterCondition['operator'] =
          filter.type === 'contains'
            ? 'contains'
            : filter.type === 'equals'
              ? '=='
              : filter.type === 'notEqual'
                ? '!='
                : filter.type === 'startsWith'
                  ? 'contains'
                  : filter.type === 'endsWith'
                    ? 'contains'
                    : 'contains';
        if (filter.filter) {
          filterParams.push({ column, operator: op, value: filter.filter });
        }
      } else if (filter.filterType === 'number') {
        const opMap: Record<string, FilterCondition['operator']> = {
          equals: '==',
          notEqual: '!=',
          lessThan: '<',
          lessThanOrEqual: '<=',
          greaterThan: '>',
          greaterThanOrEqual: '>=',
        };
        const op = opMap[filter.type] || '==';
        if (filter.filter !== null && filter.filter !== undefined) {
          filterParams.push({ column, operator: op, value: filter.filter });
        }
        if (
          filter.type === 'inRange' &&
          filter.filter !== null &&
          filter.filterTo !== null
        ) {
          filterParams.push({
            column,
            operator: '>=',
            value: filter.filter,
          });
          filterParams.push({
            column,
            operator: '<=',
            value: filter.filterTo,
          });
        }
      }
    }
    if (filterParams.length > 0) {
      operations.push({ type: 'filter', params: filterParams });
    }
  }

  if (sortModel && sortModel.length > 0) {
    const orderByParams: OrderByCondition[] = sortModel.map((sort) => ({
      column: sort.colId,
      order: sort.sort as 'asc' | 'desc',
    }));
    operations.push({ type: 'orderby', params: orderByParams });
  }

  return operations;
}
