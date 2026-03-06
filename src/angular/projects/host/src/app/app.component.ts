import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet],
  template: `
    <div class="shell">
      <header class="shell-header">
        <h1>Anode Viewer</h1>
      </header>
      <main class="shell-content">
        <router-outlet />
      </main>
    </div>
  `,
  styles: [`
    .shell {
      display: flex;
      flex-direction: column;
      height: 100vh;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    }
    .shell-header {
      display: flex;
      align-items: center;
      padding: 8px 20px;
      background: white;
      border-bottom: 1px solid #e0e0e0;
      flex-shrink: 0;
    }
    .shell-header h1 {
      margin: 0;
      font-size: 18px;
      color: #333;
    }
    .shell-content {
      flex: 1;
      overflow: hidden;
    }
  `],
})
export class AppComponent {}