/**
 * Modal dialogs for the graph editor
 */

import type { AnodeClient, GraphMetadata } from '../api/AnodeClient';

/**
 * Show save modal
 */
export function showSaveModal(): void {
  const modal = document.getElementById('save-modal');
  if (modal) modal.classList.add('active');
}

/**
 * Close save modal
 */
export function closeSaveModal(): void {
  const modal = document.getElementById('save-modal');
  if (modal) modal.classList.remove('active');
}

/**
 * Sanitize a string into a URL-safe slug
 */
function slugify(value: string): string {
  return value
    .trim()
    .toLowerCase()
    .replace(/[\s_]+/g, '-')
    .replace(/[^a-z0-9-]/g, '')
    .replace(/-{2,}/g, '-')
    .replace(/^-|-$/g, '');
}

/**
 * Get save form values
 */
export function getSaveFormValues(): { slug: string; name: string; description: string } {
  const rawSlug = (document.getElementById('save-slug') as HTMLInputElement)?.value || '';
  const slug = slugify(rawSlug);
  const name = (document.getElementById('save-name') as HTMLInputElement)?.value.trim() || '';
  const description = (document.getElementById('save-description') as HTMLInputElement)?.value.trim() || '';
  return { slug, name, description };
}

/**
 * Show load modal
 */
export function showLoadModal(): void {
  const modal = document.getElementById('load-modal');
  if (modal) modal.classList.add('active');
}

/**
 * Close load modal
 */
export function closeLoadModal(): void {
  const modal = document.getElementById('load-modal');
  if (modal) modal.classList.remove('active');
}

/**
 * Load graph list into the load modal
 */
export async function loadGraphList(
  client: AnodeClient,
  onSelect: (slug: string) => void
): Promise<void> {
  const list = document.getElementById('graph-list');
  if (!list) return;

  list.innerHTML = '<li>Loading...</li>';

  try {
    const response = await client.listGraphs();
    if (response.graphs.length === 0) {
      list.innerHTML = '<li>No saved graphs</li>';
      return;
    }

    list.innerHTML = '';
    for (const g of response.graphs) {
      const li = document.createElement('li');
      li.innerHTML = `
        <div class="name">${escapeHtml(g.name)}</div>
        <div class="meta">${escapeHtml(g.slug)} - ${escapeHtml(g.author || 'Unknown')}</div>
      `;
      li.onclick = () => onSelect(g.slug);
      list.appendChild(li);
    }
  } catch (error) {
    list.innerHTML = `<li>Error: ${escapeHtml(error instanceof Error ? error.message : 'Unknown error')}</li>`;
  }
}

/**
 * Escape HTML to prevent XSS
 */
function escapeHtml(text: string): string {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}
