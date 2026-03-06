import {
  Component,
  Input,
  Output,
  EventEmitter,
  OnChanges,
  SimpleChanges,
  inject,
  signal,
} from '@angular/core';
import { CommonModule } from '@angular/common';
import { AnodeApiService, type OutputInfo } from '../../services/anode-api.service';
import type { DrilldownEvent, ListItem, StatusEvent } from '../../shared/models';

@Component({
  selector: 'app-list-viewer',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './list-viewer.component.html',
  styleUrls: ['./list-viewer.component.scss'],
})
export class ListViewerComponent implements OnChanges {
  private api = inject(AnodeApiService);

  @Input({ required: true }) slug!: string;
  @Input({ required: true }) output!: OutputInfo;
  @Output() statusChange = new EventEmitter<StatusEvent>();
  @Output() itemClick = new EventEmitter<DrilldownEvent>();

  listItems = signal<ListItem[]>([]);
  listLoading = signal<boolean>(false);
  hasEvent = signal<boolean>(false);

  private rawColumns: string[] = [];
  private rawRows: unknown[][] = [];

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['output'] && this.output) {
      this.hasEvent.set(!!this.output.metadata?.['event']);
      this.loadListData(this.output);
    }
  }

  onItemClick(item: ListItem): void {
    const rowData: Record<string, unknown> = {};
    const raw = this.rawRows[item.index];
    if (raw) {
      this.rawColumns.forEach((col, i) => {
        rowData[col] = raw[i];
      });
    }
    this.itemClick.emit({ rowData });
  }

  private loadListData(output: OutputInfo): void {
    this.listLoading.set(true);
    this.listItems.set([]);

    this.api.getOutputData(this.slug, output.name, { limit: 10000, offset: 0 }).subscribe({
      next: (result) => {
        this.listLoading.set(false);
        if (result.status !== 'ok') return;

        const meta = (output.metadata || {}) as Record<string, string>;
        const columns = result.columns;
        const rows = result.data;

        this.rawColumns = columns;
        this.rawRows = rows;

        const labelIdx = columns.indexOf(meta['label']);
        if (labelIdx < 0) return;

        const valueIdx = meta['value'] ? columns.indexOf(meta['value']) : -1;

        this.listItems.set(
          rows.map((row, idx) => ({
            label: row[labelIdx] != null ? String(row[labelIdx]) : '',
            value: valueIdx >= 0 && row[valueIdx] != null ? String(row[valueIdx]) : '',
            index: idx,
          })),
        );
      },
      error: () => {
        this.listLoading.set(false);
      },
    });
  }
}
