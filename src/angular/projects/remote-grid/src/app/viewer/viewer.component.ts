import {
  Component,
  Input,
  OnChanges,
  OnInit,
  SimpleChanges,
  inject,
  signal,
  computed,
} from '@angular/core';
import { CommonModule } from '@angular/common';
import { AnodeApiService, type OutputInfo } from '../services/anode-api.service';
import { NON_GRID_TYPES, type DrilldownEvent, type StatusEvent } from '../shared/models';
import { DataGridComponent } from '../components/data-grid/data-grid.component';
import { ListViewerComponent } from '../components/list-viewer/list-viewer.component';
import { ButtonViewerComponent } from '../components/button-viewer/button-viewer.component';

@Component({
  selector: 'app-viewer',
  standalone: true,
  imports: [CommonModule, DataGridComponent, ListViewerComponent, ButtonViewerComponent],
  templateUrl: './viewer.component.html',
  styleUrls: ['./viewer.component.scss'],
})
export class ViewerComponent implements OnInit, OnChanges {
  private api = inject(AnodeApiService);

  @Input() graph?: string;
  @Input() execute?: boolean;
  @Input() graphInputs?: Record<string, unknown>;

  slug = signal<string>('');
  outputs = signal<OutputInfo[]>([]);
  statusMessage = signal<string>('');
  statusType = signal<string>('');

  gridOutputs = computed(() =>
    this.outputs().filter((o) => !NON_GRID_TYPES.includes(o.type ?? '')),
  );
  listOutputs = computed(() =>
    this.outputs().filter((o) => o.type === 'list'),
  );

  buttonOutputs = computed(() =>
    this.outputs().filter((o) => o.type === 'button'),
  );

  selectedGrid = signal<OutputInfo | null>(null);
  selectedList = signal<OutputInfo | null>(null);
  selectedButton = signal<OutputInfo | null>(null);

  // Drilldown state
  drilldownSlug = signal<string | null>(null);
  drilldownOutputs = signal<OutputInfo[]>([]);
  drilldownGridOutputs = computed(() =>
    this.drilldownOutputs().filter((o) => !NON_GRID_TYPES.includes(o.type ?? '')),
  );
  drilldownListOutputs = computed(() =>
    this.drilldownOutputs().filter((o) => o.type === 'list'),
  );
  drilldownButtonOutputs = computed(() =>
    this.drilldownOutputs().filter((o) => o.type === 'button'),
  );
  selectedDrilldownGrid = signal<OutputInfo | null>(null);
  selectedDrilldownList = signal<OutputInfo | null>(null);
  selectedDrilldownButton = signal<OutputInfo | null>(null);

  private static readonly RESERVED_PARAMS = new Set(['graph', 'execute']);

  ngOnInit(): void {
    // Only use URL params if no @Input was provided
    if (!this.graph) {
      this._initFromUrl();
    }
  }

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['graph'] && this.graph) {
      this._initFromGraph(this.graph, this.execute ?? false, this.graphInputs ?? {});
    }
  }

  private _initFromUrl(): void {
    const params = new URLSearchParams(window.location.search);
    const slugParam = params.get('graph');
    if (slugParam) {
      this.slug.set(slugParam);
      if (params.has('execute')) {
        const inputs = this.parseUrlInputs(params);
        this.executeAndLoad(inputs);
      } else {
        this.loadOutputs();
      }
    } else {
      this.statusMessage.set('No graph parameter in URL. Add ?graph=your-graph-slug');
      this.statusType.set('error');
    }
  }

  private _initFromGraph(graph: string, execute: boolean, inputs: Record<string, unknown>): void {
    this.slug.set(graph);
    if (execute) {
      this.executeAndLoad(inputs);
    } else {
      this.loadOutputs();
    }
  }

  private parseUrlInputs(params: URLSearchParams): Record<string, unknown> {
    const inputs: Record<string, unknown> = {};
    params.forEach((value, key) => {
      if (ViewerComponent.RESERVED_PARAMS.has(key)) return;
      if (value === 'true') { inputs[key] = true; return; }
      if (value === 'false') { inputs[key] = false; return; }
      const num = Number(value);
      if (value !== '' && !isNaN(num)) { inputs[key] = num; return; }
      inputs[key] = value;
    });
    return inputs;
  }

  private executeAndLoad(inputs: Record<string, unknown> = {}): void {
    const hasInputs = Object.keys(inputs).length > 0;
    this.statusMessage.set('Executing graph...');
    this.statusType.set('loading');

    this.api.executeGraph(this.slug(), hasInputs ? {
      inputs,
      skip_unknown_inputs: true,
    } : undefined).subscribe({
      next: (result) => {
        if (result.status === 'ok') {
          this.statusMessage.set(`Executed in ${result.duration_ms}ms. Loading outputs...`);
          this.loadOutputs();
        } else {
          this.statusMessage.set(`Execute error: ${result.message || 'Unknown'}`);
          this.statusType.set('error');
        }
      },
      error: (err) => {
        this.statusMessage.set(`Execute error: ${err.message}`);
        this.statusType.set('error');
      },
    });
  }

  loadOutputs(): void {
    this.statusMessage.set('Loading outputs...');
    this.statusType.set('loading');

    this.api.getOutputs(this.slug()).subscribe({
      next: (result) => {
        if (result.status === 'ok') {
          this.outputs.set(result.outputs || []);

          const grids = this.gridOutputs();
          const lists = this.listOutputs();
          const buttons = this.buttonOutputs();

          if (this.outputs().length === 0) {
            this.statusMessage.set('No outputs found. Execute the graph first.');
            this.statusType.set('info');
          } else {
            const parts: string[] = [];
            if (lists.length > 0) parts.push(`${lists.length} list(s)`);
            if (buttons.length > 0) parts.push(`${buttons.length} button(s)`);
            if (grids.length > 0) parts.push(`${grids.length} grid(s)`);
            this.statusMessage.set(`Found ${parts.join(', ')}.`);
            this.statusType.set('connected');

            if (lists.length > 0) {
              this.selectedList.set(lists[0]);
            }
            if (buttons.length > 0) {
              this.selectedButton.set(buttons[0]);
            }
            if (grids.length > 0) {
              this.selectedGrid.set(grids[0]);
            }
          }
        } else {
          this.statusMessage.set(`Error: ${result.message || 'Unknown error'}`);
          this.statusType.set('error');
        }
      },
      error: (err) => {
        this.statusMessage.set(`Error: ${err.message}`);
        this.statusType.set('error');
      },
    });
  }

  selectGrid(output: OutputInfo): void {
    this.selectedGrid.set(output);
  }

  selectList(output: OutputInfo): void {
    this.selectedList.set(output);
  }

  onChildStatus(event: StatusEvent): void {
    this.statusMessage.set(event.message);
    this.statusType.set(event.type);
  }

  onListDrilldown(event: DrilldownEvent): void {
    const list = this.selectedList();
    if (list) this.executeDrilldown(list, event);
  }

  onDrilldownListDrilldown(event: DrilldownEvent): void {
    const list = this.selectedDrilldownList();
    if (list) this.executeDrilldown(list, event);
  }

  onButtonDrilldown(event: DrilldownEvent): void {
    const btn = this.selectedButton();
    if (btn) this.executeDrilldown(btn, event);
  }

  onDrilldownButtonDrilldown(event: DrilldownEvent): void {
    const btn = this.selectedDrilldownButton();
    if (btn) this.executeDrilldown(btn, event);
  }

  private executeDrilldown(output: OutputInfo, event: DrilldownEvent): void {
    if (!output.metadata?.['event']) return;

    const eventField = output.metadata['event'] as string;
    const isField = !!output.metadata['event_is_field'];
    const targetSlug = isField
      ? String(event.rowData[eventField] ?? '')
      : eventField;

    if (!targetSlug) return;

    this.clearDrilldown();
    this.statusMessage.set(`Executing drilldown graph "${targetSlug}"...`);
    this.statusType.set('loading');

    this.api
      .executeGraph(targetSlug, {
        inputs: event.rowData,
        skip_unknown_inputs: true,
      })
      .subscribe({
        next: () => {
          this.api.getOutputs(targetSlug).subscribe({
            next: (result) => {
              if (result.status === 'ok') {
                this.drilldownSlug.set(targetSlug);
                this.drilldownOutputs.set(result.outputs || []);

                const grids = this.drilldownGridOutputs();
                const lists = this.drilldownListOutputs();
                const buttons = this.drilldownButtonOutputs();
                if (lists.length > 0) this.selectedDrilldownList.set(lists[0]);
                if (buttons.length > 0) this.selectedDrilldownButton.set(buttons[0]);
                if (grids.length > 0) this.selectedDrilldownGrid.set(grids[0]);

                this.statusMessage.set(`Drilldown "${targetSlug}" loaded.`);
                this.statusType.set('connected');
              } else {
                this.statusMessage.set(`Drilldown error: ${result.message || 'Unknown'}`);
                this.statusType.set('error');
              }
            },
            error: (err) => {
              this.statusMessage.set(`Drilldown error: ${err.message}`);
              this.statusType.set('error');
            },
          });
        },
        error: (err) => {
          this.statusMessage.set(`Execute error: ${err.message}`);
          this.statusType.set('error');
        },
      });
  }

  clearDrilldown(): void {
    this.drilldownSlug.set(null);
    this.drilldownOutputs.set([]);
    this.selectedDrilldownGrid.set(null);
    this.selectedDrilldownList.set(null);
    this.selectedDrilldownButton.set(null);
  }

  selectButton(output: OutputInfo): void {
    this.selectedButton.set(output);
  }

  selectDrilldownGrid(output: OutputInfo): void {
    this.selectedDrilldownGrid.set(output);
  }

  selectDrilldownList(output: OutputInfo): void {
    this.selectedDrilldownList.set(output);
  }

  selectDrilldownButton(output: OutputInfo): void {
    this.selectedDrilldownButton.set(output);
  }
}

// Backward-compat alias for Module Federation.
// The host imports m.GridViewerComponent from './GridViewer'.
export { ViewerComponent as GridViewerComponent };
