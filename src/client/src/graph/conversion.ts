/**
 * Conversion between Anode graph format and LiteGraph format
 */

import { LiteGraph, LGraphGroup, type LGraph, type LGraphNode } from '@comfyorg/litegraph';
import type { AnodeGraph, AnodeNode, AnodeConnection, AnodeGroup } from '../api/AnodeClient';
import type { AnodeLGraphNode, LGraphLink, NodeId, NodeProperty } from '../types/litegraph';
import { updateCsvInfoWidget } from './nodeTypes';

/**
 * Convert an Anode graph to LiteGraph format
 * @returns Map from anode node ID to LiteGraph node ID
 */
export function anodeToLitegraph(anodeGraph: AnodeGraph, lgraph: LGraph): Map<string, NodeId> {
  lgraph.clear();
  const idMap = new Map<string, NodeId>();

  console.log('[LOAD] Loading graph, nodes:', anodeGraph.nodes.length, 'connections:', anodeGraph.connections.length);

  // Create nodes
  for (const anode of anodeGraph.nodes) {
    console.log('[LOAD] Creating node:', anode.id, 'type:', anode.type);
    let lgNode = LiteGraph.createNode(anode.type) as AnodeLGraphNode | null;

    // If not found with full type, try category prefixes
    if (!lgNode && !anode.type.includes('/')) {
      for (const cat of ['scalar', 'data', 'math', 'csv', 'aggregate']) {
        lgNode = LiteGraph.createNode(`${cat}/${anode.type}`) as AnodeLGraphNode | null;
        if (lgNode) break;
      }
    }

    if (!lgNode) {
      console.error(`[LOAD] FAILED to create node: ${anode.id} type: ${anode.type}`);
      continue;
    }

    // Store Anode ID for later mapping
    lgNode._anodeId = anode.id;

    // Copy properties
    for (const [key, prop] of Object.entries(anode.properties || {})) {
      lgNode.properties[key] = prop.value as NodeProperty;
      // Update widget
      const widgets = (lgNode as any).widgets as any[] | undefined;
      if (widgets) {
        const w = widgets.find((w: any) => w.name === key);
        if (w) w.value = prop.value;
      }
    }

    // Restore dynamic inputs for nodes that support them
    // group node
    if (anode.type.includes('group') && anode.properties._visibleFieldCount) {
      if (lgNode.restoreFieldInputs) {
        lgNode.restoreFieldInputs(anode.properties._visibleFieldCount.value as number);
      }
    }
    // select_by_name and reorder_columns nodes (both use _visibleColumnCount)
    if ((anode.type.includes('select_by_name') || anode.type.includes('reorder_columns')) && anode.properties._visibleColumnCount) {
      if (lgNode.restoreColumnInputs) {
        lgNode.restoreColumnInputs(anode.properties._visibleColumnCount.value as number);
      }
    }
    // select_by_pos node
    if (anode.type.includes('select_by_pos') && anode.properties._visibleColCount) {
      if (lgNode.restoreColInputs) {
        lgNode.restoreColInputs(anode.properties._visibleColCount.value as number);
      }
    }
    // pivot node
    if (anode.type.includes('pivot') && anode.properties._visibleIndexColumnCount) {
      if (lgNode.restoreIndexColumnInputs) {
        lgNode.restoreIndexColumnInputs(anode.properties._visibleIndexColumnCount.value as number);
      }
    }
    // scalars_to_csv node
    if (anode.type.includes('scalars_to_csv') && anode.properties._visiblePairCount) {
      if (lgNode.restorePairInputs) {
        lgNode.restorePairInputs(anode.properties._visiblePairCount.value as number);
      }
    }
    // remap_by_name node
    if (anode.type.includes('remap_by_name') && anode.properties._visibleRemapCount) {
      if (lgNode.restoreRemapInputs) {
        lgNode.restoreRemapInputs(anode.properties._visibleRemapCount.value as number);
      }
    }
    // concat / concat_prefix nodes
    if ((anode.type.includes('concat')) && anode.properties._visibleConcatCount) {
      if (lgNode.restoreConcatInputs) {
        lgNode.restoreConcatInputs(anode.properties._visibleConcatCount.value as number);
      }
    }
    // string_as_fields node
    if (anode.type.includes('string_as_fields') && anode.properties._visibleValueCount) {
      if (lgNode.restoreValueInputs) {
        lgNode.restoreValueInputs(anode.properties._visibleValueCount.value as number);
      }
      // Restore individual field values from _value JSON array
      const rawValue = anode.properties._value?.value;
      if (typeof rawValue === 'string') {
        try {
          const arr = JSON.parse(rawValue);
          if (Array.isArray(arr)) {
            arr.forEach((v: string, i: number) => {
              lgNode.properties['_field_' + i] = v;
              const widget = (lgNode as any).widgets?.find((w: any) => w.name === '_field_' + i);
              if (widget) widget.value = v;
            });
          }
        } catch { /* ignore parse errors */ }
      }
    }
    // csv_value node: restore info widget
    if (anode.type.includes('csv_value') && anode.properties._value) {
      updateCsvInfoWidget(lgNode);
    }

    // Set position
    if (anode.position) {
      lgNode.pos = [anode.position[0], anode.position[1]];
    }

    lgraph.add(lgNode);
    idMap.set(anode.id, lgNode.id);
    console.log('[LOAD] Node created:', anode.id, '-> lgId:', lgNode.id);
  }

  // Create connections
  for (const conn of anodeGraph.connections) {
    const fromLgId = idMap.get(conn.from);
    const toLgId = idMap.get(conn.to);
    if (fromLgId === undefined || toLgId === undefined) {
      console.error('[LOAD] Connection skipped - node not in idMap:', conn);
      continue;
    }

    const fromNode = lgraph.getNodeById(fromLgId);
    const toNode = lgraph.getNodeById(toLgId);
    if (!fromNode || !toNode) {
      console.error('[LOAD] Connection skipped - node not found:', conn);
      continue;
    }

    const fromSlot = fromNode.findOutputSlot(conn.fromPort);
    const toSlot = toNode.findInputSlot(conn.toPort);
    if (fromSlot === -1 || toSlot === -1) {
      console.error('[LOAD] Connection skipped - slot not found:', conn, 'fromSlot:', fromSlot, 'toSlot:', toSlot);
      continue;
    }

    fromNode.connect(fromSlot, toNode, toSlot);
  }

  // Restore groups
  if (anodeGraph.groups) {
    for (const aGroup of anodeGraph.groups) {
      const group = new LGraphGroup(aGroup.title);
      (lgraph as any).add(group);
      // Set pos and size directly (LiteGraph uses _pos/_size internally, not _bounding)
      const [x, y, w, h] = aGroup.bounding;
      (group as any)._pos[0] = x;
      (group as any)._pos[1] = y;
      (group as any)._size[0] = w;
      (group as any)._size[1] = h;
      if (aGroup.color) group.color = aGroup.color;
      if (aGroup.font_size) group.font_size = aGroup.font_size;
    }
  }

  console.log('[LOAD] Done. Nodes in graph:', (lgraph as any)._nodes.length);
  return idMap;
}

/**
 * Convert a LiteGraph graph to Anode format
 */
export function litegraphToAnode(lgraph: LGraph): AnodeGraph {
  const nodes: AnodeNode[] = [];
  const connections: AnodeConnection[] = [];
  const usedIds = new Set<string>();

  const lgNodes = (lgraph as any)._nodes as AnodeLGraphNode[] | undefined;
  if (!lgNodes) {
    return { nodes: [], connections: [] };
  }

  console.log('[SAVE] Exporting graph, nodes count:', lgNodes.length);

  // First pass: find max numeric ID from existing anode IDs
  let nextId = 1;
  for (const lgNode of lgNodes) {
    if (lgNode._anodeId) {
      const match = lgNode._anodeId.match(/^node_(\d+)$/);
      if (match) {
        nextId = Math.max(nextId, parseInt(match[1]) + 1);
      }
    }
  }

  // Export nodes, ensuring unique IDs
  for (const lgNode of lgNodes) {
    let anodeId = lgNode._anodeId;

    // If no ID or duplicate, generate a new one
    if (!anodeId || usedIds.has(anodeId)) {
      if (anodeId && usedIds.has(anodeId)) {
        console.warn('[SAVE] Duplicate ID detected:', anodeId, 'for', lgNode.type);
      }
      anodeId = `node_${nextId++}`;
      console.log('[SAVE] Assigned new ID:', anodeId, 'to', lgNode.type);
    }

    usedIds.add(anodeId);
    lgNode._anodeId = anodeId;

    const anodeNode: AnodeNode = {
      id: anodeId,
      type: lgNode.type ?? '',
      properties: {},
      position: [lgNode.pos[0], lgNode.pos[1]],
    };

    for (const [key, value] of Object.entries(lgNode.properties || {})) {
      // Skip display-only properties
      if (key === '_csv_info') continue;
      anodeNode.properties[key] = {
        value: value,
        type: inferType(value),
      };
    }

    // Save dynamic input counters for nodes that support them
    // group node
    if (lgNode._visibleFieldCount !== undefined) {
      anodeNode.properties._visibleFieldCount = {
        value: lgNode._visibleFieldCount,
        type: 'int',
      };
    }
    // select_by_name node
    if (lgNode._visibleColumnCount !== undefined) {
      anodeNode.properties._visibleColumnCount = {
        value: lgNode._visibleColumnCount,
        type: 'int',
      };
    }
    // select_by_pos node
    if (lgNode._visibleColCount !== undefined) {
      anodeNode.properties._visibleColCount = {
        value: lgNode._visibleColCount,
        type: 'int',
      };
    }
    // pivot node
    if (lgNode._visibleIndexColumnCount !== undefined) {
      anodeNode.properties._visibleIndexColumnCount = {
        value: lgNode._visibleIndexColumnCount,
        type: 'int',
      };
    }
    // scalars_to_csv node
    if (lgNode._visiblePairCount !== undefined) {
      anodeNode.properties._visiblePairCount = {
        value: lgNode._visiblePairCount,
        type: 'int',
      };
    }
    // remap_by_name node
    if (lgNode._visibleRemapCount !== undefined) {
      anodeNode.properties._visibleRemapCount = {
        value: lgNode._visibleRemapCount,
        type: 'int',
      };
    }
    // concat / concat_prefix nodes
    if (lgNode._visibleConcatCount !== undefined) {
      anodeNode.properties._visibleConcatCount = {
        value: lgNode._visibleConcatCount,
        type: 'int',
      };
    }
    // string_as_fields node
    if (lgNode._visibleValueCount !== undefined) {
      anodeNode.properties._visibleValueCount = {
        value: lgNode._visibleValueCount,
        type: 'int',
      };
    }

    console.log('[SAVE] Node:', anodeId, 'type:', lgNode.type);
    nodes.push(anodeNode);
  }

  // Export connections
  const links = (lgraph as any).links as Map<number, LGraphLink> | Record<number, LGraphLink> | undefined;
  if (links) {
    const linkValues = links instanceof Map ? links.values() : Object.values(links);

    for (const link of linkValues) {
      if (!link) continue;

      const fromNode = lgraph.getNodeById(link.origin_id) as AnodeLGraphNode | null;
      const toNode = lgraph.getNodeById(link.target_id) as AnodeLGraphNode | null;
      if (!fromNode || !toNode) {
        console.warn('[SAVE] Skipping link - missing node:', link);
        continue;
      }

      const outputs = fromNode.outputs as any[] | undefined;
      const inputs = toNode.inputs as any[] | undefined;
      const fromPort = outputs?.[link.origin_slot]?.name;
      const toPort = inputs?.[link.target_slot]?.name;
      if (!fromPort || !toPort) {
        console.warn('[SAVE] Skipping link - missing port:', { fromPort, toPort, link });
        continue;
      }

      connections.push({
        from: fromNode._anodeId || `node_${link.origin_id}`,
        fromPort,
        to: toNode._anodeId || `node_${link.target_id}`,
        toPort,
      });
    }
  }

  // Export groups
  const groups: AnodeGroup[] = [];
  const lgGroups = (lgraph as any)._groups as LGraphGroup[] | undefined;
  if (lgGroups) {
    for (const group of lgGroups) {
      groups.push({
        title: group.title,
        bounding: Array.from(group._bounding) as [number, number, number, number],
        color: group.color,
        font_size: group.font_size,
      });
      console.log('[SAVE] Group:', group.title);
    }
  }

  console.log('[SAVE] Exported:', nodes.length, 'nodes,', connections.length, 'connections,', groups.length, 'groups');
  return { nodes, connections, groups: groups.length > 0 ? groups : undefined };
}

/**
 * Infer the Anode type from a JavaScript value
 */
function inferType(value: unknown): string {
  if (value === null || value === undefined) return 'null';
  if (typeof value === 'boolean') return 'bool';
  if (typeof value === 'number') return Number.isInteger(value) ? 'int' : 'double';
  if (typeof value === 'string') return 'string';
  if (typeof value === 'object' && value !== null && 'columns' in value && 'data' in value) return 'csv';
  return 'string';
}
