#pragma once

namespace nodes {

/**
 * Register CSV-related nodes:
 * - csv_source: outputs a test DataFrame
 * - field: selects a column from a CSV
 */
void registerCsvNodes();

/**
 * Register csv_source node
 *
 * Outputs:
 *   - csv (Csv): a test DataFrame with columns: id, name, price
 *
 * This is an entry point node (no inputs required).
 */
void registerCsvSourceNode();

/**
 * Register field node
 *
 * Inputs:
 *   - csv (Csv): the source DataFrame
 *
 * Outputs:
 *   - field (Field): reference to the selected column
 *   - csv (Csv): pass-through of the input CSV
 *
 * Properties:
 *   - _column (String): name of the column to select
 */
void registerFieldNode();

/**
 * Register join_flex node
 *
 * Flex join with 3 separate outputs for no-match, single-match, and multiple-match.
 *
 * Inputs:
 *   - left_csv (Csv): Left DataFrame
 *   - right_csv (Csv): Right DataFrame
 *   - left_field (Field|String): Left key column
 *   - right_field (Field|String): Right key column
 *
 * Properties:
 *   - _no_match_keep_jointure: "yes" | "no_but_keep_header" | "no" | "skip"
 *   - _single_match_keep_jointure: "yes" | "no_but_keep_header" | "no" | "skip"
 *   - _double_match_keep_jointure: "yes" | "no_but_keep_header" | "no" | "skip"
 *   - _left_field_0, _right_field_0, etc: Additional key pairs
 *
 * Mode values:
 *   - "yes": Keep all columns (left + right) with data
 *   - "no_but_keep_header": Keep all columns but with empty right values
 *   - "no": Keep only left columns
 *   - "skip": Don't write any rows to this output (empty DataFrame)
 *
 * Outputs:
 *   - csv_no_match (Csv): Left rows with no match
 *   - csv_single_match (Csv): Left rows with exactly 1 match
 *   - csv_multiple_match (Csv): Left rows with multiple matches
 */
void registerJoinFlexNode();

/**
 * Register output node
 *
 * A node that "publishes" a DataFrame with a name for external access.
 * The name can be set via widget or overridden by connecting a string value.
 *
 * Inputs:
 *   - csv (Csv): The DataFrame to publish
 *   - _name (String, widget): Name of the published output - can be connected to override widget value
 *
 * Outputs:
 *   - csv (Csv): Pass-through of the input CSV
 *   - output_name (String): The resolved output name (used for persistence)
 *
 * Usage:
 *   The output name is accessible via API:
 *   - GET /api/graph/:slug/outputs - List all named outputs
 *   - POST /api/graph/:slug/output/:name - Get DataFrame by output name
 */
void registerOutputNode();

} // namespace nodes
