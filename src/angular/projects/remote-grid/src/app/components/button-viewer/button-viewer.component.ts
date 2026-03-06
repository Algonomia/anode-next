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
import type { ButtonItem, DrilldownEvent, StatusEvent } from '../../shared/models';

@Component({
  selector: 'app-button-viewer',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './button-viewer.component.html',
  styleUrls: ['./button-viewer.component.scss'],
})
export class ButtonViewerComponent implements OnChanges {
  private api = inject(AnodeApiService);

  @Input({ required: true }) slug!: string;
  @Input({ required: true }) output!: OutputInfo;
  @Output() statusChange = new EventEmitter<StatusEvent>();
  @Output() buttonClick = new EventEmitter<DrilldownEvent>();

  buttons = signal<ButtonItem[]>([]);
  loading = signal<boolean>(false);

  private rawColumns: string[] = [];
  private rawRows: unknown[][] = [];

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['output'] && this.output) {
      this.loadButtonData(this.output);
    }
  }

  onButtonClick(button: ButtonItem): void {
    if (!button.hasEvent) return;

    const rowData: Record<string, unknown> = {};
    const raw = this.rawRows[button.index];
    if (raw) {
      this.rawColumns.forEach((col, i) => {
        rowData[col] = raw[i];
      });
    }
    this.buttonClick.emit({ rowData });
  }

  private loadButtonData(output: OutputInfo): void {
    this.loading.set(true);
    this.buttons.set([]);

    this.api.getOutputData(this.slug, output.name, { limit: 10000, offset: 0 }).subscribe({
      next: (result) => {
        this.loading.set(false);
        if (result.status !== 'ok') return;

        const meta = (output.metadata || {}) as Record<string, unknown>;
        const columns = result.columns;
        const rows = result.data;

        this.rawColumns = columns;
        this.rawRows = rows;

        const labelMeta = String(meta['label'] ?? '');
        const labelIsField = meta['label_is_field'] !== false;
        const nameMeta = String(meta['name'] ?? '');
        const nameIsField = meta['name_is_field'] !== false;
        const eventMeta = String(meta['event'] ?? '');
        const eventIsField = !!meta['event_is_field'];
        const hasEvent = !!meta['event'];

        if (labelIsField) {
          const labelIdx = columns.indexOf(labelMeta);
          if (labelIdx < 0) return;

          const nameIdx = nameIsField ? columns.indexOf(nameMeta) : -1;

          this.buttons.set(
            rows.map((row, idx) => ({
              label: row[labelIdx] != null ? String(row[labelIdx]) : '',
              name: nameIsField
                ? (nameIdx >= 0 && row[nameIdx] != null ? String(row[nameIdx]) : '')
                : nameMeta,
              index: idx,
              hasEvent,
            })),
          );
        } else {
          this.buttons.set([
            {
              label: labelMeta,
              name: nameIsField
                ? (rows.length > 0 && columns.indexOf(nameMeta) >= 0
                    ? String(rows[0][columns.indexOf(nameMeta)] ?? '')
                    : '')
                : nameMeta,
              index: 0,
              hasEvent,
            },
          ]);
        }
      },
      error: () => {
        this.loading.set(false);
      },
    });
  }
}
