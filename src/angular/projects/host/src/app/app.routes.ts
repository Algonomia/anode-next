import { Routes } from '@angular/router';
import { loadRemoteModule } from '@angular-architects/native-federation';

export const routes: Routes = [
  {
    path: '',
    redirectTo: 'viewer',
    pathMatch: 'full',
  },
  {
    path: 'viewer',
    loadComponent: () =>
      loadRemoteModule('remote-grid', './GridViewer').then(
        (m) => m.GridViewerComponent
      ),
  },
];