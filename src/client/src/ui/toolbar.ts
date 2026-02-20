/**
 * Toolbar and status management
 */

/**
 * Set status indicator
 */
export function setStatus(type: 'connected' | 'error' | 'loading', message: string): void {
  const status = document.getElementById('status');
  if (status) {
    status.className = `status ${type}`;
    status.textContent = message;
  }
}

/**
 * Update the displayed graph name
 */
export function updateGraphName(name: string): void {
  const nameEl = document.getElementById('current-graph-name');
  if (nameEl) {
    nameEl.textContent = name;
  }
}

/**
 * Update graph link badges in the toolbar
 */
export function updateGraphLinks(links: { outgoing: string[]; incoming: string[] }): void {
  const el = document.getElementById('graph-links');
  if (!el) return;
  const badges = [
    ...links.outgoing.map(
      (s) =>
        `<a href="/?graph=${encodeURIComponent(s)}" class="link-badge outgoing" title="Event target">\u2192 ${s}</a>`
    ),
    ...links.incoming.map(
      (s) =>
        `<a href="/?graph=${encodeURIComponent(s)}" class="link-badge incoming" title="Called by">\u2190 ${s}</a>`
    ),
  ];
  el.innerHTML = badges.join('');
}

/**
 * Show results in the results panel
 */
export function showResults(content: string, isError = false): void {
  const el = document.getElementById('results-content');
  if (el) {
    el.textContent = content;
    el.className = isError ? 'results-content error' : 'results-content';
  }
}
