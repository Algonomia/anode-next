#pragma once

namespace nodes {

/**
 * Register visualization nodes:
 * - timeline_output: publishes a DataFrame with timeline visualization metadata
 */
void registerVizNodes();

/**
 * Register timeline_output node
 *
 * A node that "publishes" a DataFrame with timeline visualization metadata.
 * Similar to data/output but adds output_type="timeline" and output_metadata
 * containing field mappings for vis-timeline rendering.
 *
 * Inputs:
 *   - csv (Csv, required): The DataFrame to publish
 *   - start_date (Field, required): Column for event start dates
 *   - name (Field, required): Column for event labels (vis-timeline "content")
 *   - end_date (Field, optional): Column for event end dates (point if absent, range if present)
 *   - parent (Field, optional): Column for grouping/hierarchy (vis-timeline "group")
 *   - color (Field|String, optional): Color column or fixed color value (vis-timeline "style")
 *
 * Properties:
 *   - _timeline_name (String, widget): Name for the timeline tab
 *
 * Outputs:
 *   - csv (Csv): Pass-through of the input CSV
 *   - output_name (String): The resolved timeline name
 *   - output_type (String): Literal "timeline"
 *   - output_metadata (String): JSON with field mappings
 */
void registerTimelineOutputNode();

/**
 * Register diff_output node
 *
 * A node that computes a side-by-side diff between two CSVs and publishes
 * an annotated DataFrame for visualization.
 *
 * Inputs:
 *   - left (Csv, required): The "before" DataFrame
 *   - right (Csv, required): The "after" DataFrame
 *   - key (Field, optional): Column to match rows by key (positional if absent)
 *
 * Properties:
 *   - _diff_name (String, widget): Name for the diff tab
 *
 * Outputs:
 *   - csv (Csv): Annotated DataFrame with __diff__, __old_*, __changed_* columns
 *   - output_name (String): The resolved diff name
 *   - output_type (String): Literal "diff"
 *   - output_metadata (String): JSON with column info and diff stats
 */
void registerDiffOutputNode();

/**
 * Register bar_chart_output node
 *
 * A node that publishes a DataFrame with bar chart visualization metadata.
 * Uses amCharts 5 for rendering in the viewer.
 *
 * Inputs:
 *   - csv (Csv, required): The DataFrame to publish
 *   - category (Field, optional): Column for the X axis (categories). Auto-detected in tree mode.
 *   - value (Field, required): Column for the Y axis (values)
 *   - color (Field|String, optional): Color column or fixed hex color
 *   - event (Field|String, optional): Target graph slug for drilldown
 *
 * Tree mode: When the input CSV contains __tree_path (from tree_group), the chart
 * enables hierarchical drilldown: clicking a bar navigates to children in the tree.
 *
 * Properties:
 *   - _chart_name (String, widget): Name for the chart tab
 *
 * Outputs:
 *   - csv (Csv): Pass-through of the input CSV
 *   - output_name (String): The resolved chart name
 *   - output_type (String): Literal "chart"
 *   - output_metadata (String): JSON with field mappings and chart_type
 */
void registerBarChartOutputNode();

} // namespace nodes
