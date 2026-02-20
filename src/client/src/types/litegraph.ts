/**
 * Extended type definitions for @comfyorg/litegraph
 */

import type {
  LGraph,
  LGraphCanvas,
  LGraphNode,
  INodeInputSlot,
  INodeOutputSlot,
  Point,
  Size,
  NodeId,
} from '@comfyorg/litegraph';

// Re-export commonly used types
export type {
  LGraph,
  LGraphCanvas,
  LGraphNode,
  INodeInputSlot,
  INodeOutputSlot,
  Point,
  Size,
  NodeId,
};

// NodeProperty from litegraph
export type NodeProperty = string | number | boolean | object;

// Extended LGraphNode with our custom properties
export interface AnodeLGraphNode extends LGraphNode {
  _anodeId?: string;
  // group node
  _visibleFieldCount?: number;
  restoreFieldInputs?: (count: number) => void;
  // select_by_name node
  _visibleColumnCount?: number;
  restoreColumnInputs?: (count: number) => void;
  // select_by_pos node
  _visibleColCount?: number;
  restoreColInputs?: (count: number) => void;
  // pivot node
  _visibleIndexColumnCount?: number;
  restoreIndexColumnInputs?: (count: number) => void;
  // scalars_to_csv node
  _visiblePairCount?: number;
  restorePairInputs?: (count: number) => void;
  // remap_by_name node
  _visibleRemapCount?: number;
  restoreRemapInputs?: (count: number) => void;
  // concat / concat_prefix nodes
  _visibleConcatCount?: number;
  restoreConcatInputs?: (count: number) => void;
  // string_as_fields node
  _visibleValueCount?: number;
  restoreValueInputs?: (count: number) => void;
}

// Link structure from the graph
export interface LGraphLink {
  id: number;
  origin_id: number;
  origin_slot: number;
  target_id: number;
  target_slot: number;
  type?: string;
}

// Extended input slot with color
export interface ColoredInputSlot extends INodeInputSlot {
  color_on?: string;
  color_off?: string;
}

// Extended output slot with color
export interface ColoredOutputSlot extends INodeOutputSlot {
  color_on?: string;
  color_off?: string;
}
