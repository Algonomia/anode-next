import {
  Component,
  Input,
  Output,
  EventEmitter,
  OnChanges,
  OnDestroy,
  SimpleChanges,
  inject,
  signal,
} from '@angular/core';
import { CommonModule } from '@angular/common';
import { AgGridAngular } from 'ag-grid-angular';
import {
  AllCommunityModule,
  ModuleRegistry,
  type ColDef,
  type GridApi,
  type GridReadyEvent,
  type IDatasource,
  type IGetRowsParams,
} from 'ag-grid-community';
import { AllEnterpriseModule } from 'ag-grid-enterprise';
import {
  AnodeApiService,
  type OutputInfo,
  type OutputDataRequest,
} from '../../services/anode-api.service';
import { columnarToRows, buildOperations } from '../../shared/utils';
import type { StatusEvent } from '../../shared/models';

ModuleRegistry.registerModules([AllCommunityModule, AllEnterpriseModule]);

const PAGE_SIZE = 100;

@Component({
  selector: 'app-data-grid',
  standalone: true,
  imports: [CommonModule, AgGridAngular],
  templateUrl: './data-grid.component.html',
  styleUrls: ['./data-grid.component.scss'],
})
export class DataGridComponent implements OnChanges, OnDestroy {
  private api = inject(AnodeApiService);
  private gridApi: GridApi | null = null;

  @Input({ required: true }) slug!: string;
  @Input({ required: true }) output!: OutputInfo;
  @Output() statusChange = new EventEmitter<StatusEvent>();

  isTreeMode = signal<boolean>(false);
  columnDefs = signal<ColDef[]>([]);
  rowData = signal<any[]>([]);
  statsText = signal<string>('');

  defaultColDef: ColDef = {
    flex: 1,
    minWidth: 100,
    sortable: true,
    filter: true,
    resizable: true,
  };

  getDataPath = (data: any): string[] => {
    try {
      return JSON.parse(data.__tree_path);
    } catch {
      return [String(data.__tree_path)];
    }
  };

  autoGroupColumnDef: ColDef = {
    headerName: 'Hierarchy',
    minWidth: 250,
    flex: 2,
    cellRendererParams: { suppressCount: false },
  };

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['output'] && this.output) {
      const isTree = (this.output.columns ?? []).includes('__tree_path');
      this.isTreeMode.set(isTree);
      this.initGrid(this.output);
    }
  }

  ngOnDestroy(): void {
    this.gridApi = null;
  }

  onGridReady(event: GridReadyEvent): void {
    this.gridApi = event.api;

    if (!this.isTreeMode() && this.output) {
      this.gridApi.setGridOption('datasource', this.createDatasource());
    }
  }

  onSortChanged(): void {
    if (this.gridApi && !this.isTreeMode()) {
      this.gridApi.purgeInfiniteCache();
    }
  }

  onFilterChanged(): void {
    if (this.gridApi && !this.isTreeMode()) {
      this.gridApi.purgeInfiniteCache();
    }
  }

  private initGrid(output: OutputInfo): void {
    const metaColumns = ['__tree_path', '__tree_agg'];
    const isTree = this.isTreeMode();
    const visibleColumns = isTree
      ? output.columns.filter((c) => !metaColumns.includes(c))
      : output.columns;

    this.columnDefs.set(this.createColumnDefs(visibleColumns));
    this.gridApi = null;
    this.rowData.set([]);

    if (isTree) {
      this.loadTreeData(output);
    }
  }

  private createColumnDefs(columns: string[]): ColDef[] {
    return columns.map((col) => ({
      field: col,
      headerName: col,
      sortable: true,
      filter: 'agTextColumnFilter',
      filterParams: {
        filterOptions: ['contains', 'equals', 'notEqual', 'startsWith', 'endsWith'],
        defaultOption: 'contains',
      },
      resizable: true,
    }));
  }

  private createDatasource(): IDatasource {
    return {
      getRows: (params: IGetRowsParams) => {
        if (!this.output) {
          params.failCallback();
          return;
        }

        const operations = buildOperations(params.sortModel, params.filterModel);
        const request: OutputDataRequest = {
          limit: params.endRow - params.startRow,
          offset: params.startRow,
          operations,
        };

        this.statusChange.emit({ message: 'Loading...', type: 'loading' });

        this.api.getOutputData(this.slug, this.output.name, request).subscribe({
          next: (result) => {
            if (result.status !== 'ok') {
              this.statusChange.emit({ message: `Error: ${result.message}`, type: 'error' });
              params.failCallback();
              return;
            }
            const rows = columnarToRows(result.columns, result.data);
            const totalRows = result.stats.total_rows;
            const currentPage = Math.floor(params.startRow / PAGE_SIZE) + 1;
            const lastPage = Math.ceil(totalRows / PAGE_SIZE);

            this.statsText.set(
              `Total: ${totalRows.toLocaleString()} rows | Page: ${currentPage}/${lastPage} | Query: ${result.stats.duration_ms}ms`,
            );
            this.statusChange.emit({
              message: `Rows ${params.startRow + 1}-${Math.min(params.endRow, totalRows)} of ${totalRows.toLocaleString()}`,
              type: 'connected',
            });

            params.successCallback(rows, totalRows);
          },
          error: (err) => {
            this.statusChange.emit({ message: `Error: ${err.message}`, type: 'error' });
            params.failCallback();
          },
        });
      },
    };
  }

  private loadTreeData(output: OutputInfo): void {
    this.statusChange.emit({ message: 'Loading tree data...', type: 'loading' });

    this.api
      .getOutputData(this.slug, output.name, { limit: 100000, offset: 0 })
      .subscribe({
        next: (result) => {
          if (result.status !== 'ok') {
            this.statusChange.emit({ message: `Error: ${result.message}`, type: 'error' });
            return;
          }

          let aggFunc = 'sum';
          const aggIdx = result.columns.indexOf('__tree_agg');
          if (aggIdx !== -1 && result.data.length > 0) {
            aggFunc = String(result.data[0][aggIdx]);
          }

          this.columnDefs.update((cols) =>
            cols.map((col) => ({ ...col, aggFunc })),
          );

          const rows = columnarToRows(result.columns, result.data);
          this.rowData.set(rows);

          this.statsText.set(
            `Tree mode - ${result.stats.total_rows.toLocaleString()} rows`,
          );
          this.statusChange.emit({
            message: `Tree mode - ${result.stats.total_rows.toLocaleString()} rows`,
            type: 'connected',
          });
        },
        error: (err) => {
          this.statusChange.emit({ message: `Error: ${err.message}`, type: 'error' });
        },
      });
  }
}
