/**
 * Port colors by type for the graph editor
 */

import type { LGraph, LGraphCanvas } from '@comfyorg/litegraph';
import type { ColoredInputSlot, ColoredOutputSlot, LGraphLink } from '../types/litegraph';

// Port colors by type
export const TYPE_COLORS: Record<string, string> = {
  // Scalars - green
  int: '#4CAF50',
  double: '#4CAF50',
  string: '#4CAF50',
  bool: '#4CAF50',
  // CSV - orange
  csv: '#FF9800',
  // Field - violet
  field: '#9C27B0',
};

/**
 * Get the slot color based on its types
 * @param types Array of type names like ['int', 'double'] or ['csv']
 */
export function getSlotColor(types: string[]): string | null {
  if (!types || types.length === 0) return null;

  // If single type, use its color
  if (types.length === 1) {
    return TYPE_COLORS[types[0].toLowerCase()] ?? null;
  }

  // If multi-type, check if all are scalars
  const allScalars = types.every((t) =>
    ['int', 'double', 'string', 'bool'].includes(t.toLowerCase())
  );
  if (allScalars) return TYPE_COLORS['int']; // green for scalars

  // If mix includes field, use field color
  if (types.some((t) => t.toLowerCase() === 'field')) {
    return TYPE_COLORS['field'];
  }

  // Default: return first type's color
  return TYPE_COLORS[types[0].toLowerCase()] ?? null;
}

/**
 * Update input colors based on connected output colors
 * Inputs are gray by default and take the color of their connected output
 */
export function updateInputColors(graph: LGraph, graphCanvas: LGraphCanvas | null): void {
  if (!graph) return;

  const nodes = (graph as any)._nodes as any[] | undefined;
  if (!nodes) return;

  // Reset all input colors to null (gray)
  for (const node of nodes) {
    if (node.inputs) {
      for (const input of node.inputs as ColoredInputSlot[]) {
        input.color_on = undefined;
        input.color_off = undefined;
      }
    }
  }

  // For each link, set input color from output color
  const links = (graph as any).links as Map<number, LGraphLink> | Record<number, LGraphLink> | undefined;
  if (!links) return;

  // Handle both Map and object formats
  const linkValues = links instanceof Map ? links.values() : Object.values(links);

  for (const link of linkValues) {
    if (!link) continue;

    const originNode = graph.getNodeById(link.origin_id);
    const targetNode = graph.getNodeById(link.target_id);
    if (!originNode || !targetNode) continue;

    const outputSlot = (originNode.outputs as ColoredOutputSlot[] | undefined)?.[link.origin_slot];
    const inputSlot = (targetNode.inputs as ColoredInputSlot[] | undefined)?.[link.target_slot];
    if (!outputSlot || !inputSlot) continue;

    // Copy color from output to input
    if (outputSlot.color_on) {
      inputSlot.color_on = outputSlot.color_on;
      inputSlot.color_off = outputSlot.color_on;
    }
  }

  // Redraw
  if (graphCanvas) {
    graphCanvas.setDirty(true, true);
  }
}
