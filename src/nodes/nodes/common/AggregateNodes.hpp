#pragma once

namespace nodes {

/**
 * Register aggregate nodes:
 * - group: groups rows by field columns with selectable aggregation
 */
void registerAggregateNodes();

/**
 * Register group node
 *
 * Inputs:
 *   - csv (Csv): input DataFrame
 *   - field (Field): first groupBy column (required)
 *   - field2 (Field, optional): second groupBy column
 *   - field3 (Field, optional): third groupBy column
 *
 * Outputs:
 *   - csv (Csv): grouped DataFrame
 *
 * Properties:
 *   - _aggregation: aggregation function (sum, avg, min, max, first, count)
 */
void registerGroupNode();

/**
 * Register pivot node
 *
 * Inputs:
 *   - csv (Csv): input DataFrame
 *   - pivot_column (Field): column whose unique values become column names
 *   - value_column (Field): column whose values fill the pivoted columns
 *   - index_column (Field, optional): first index column
 *   - index_column_1 to index_column_99 (Field, optional): additional index columns
 *
 * Outputs:
 *   - csv (Csv): pivoted DataFrame
 *
 * Properties:
 *   - _prefix (optional): prefix for pivoted column names
 */
void registerPivotNode();

/**
 * Register tree_group node
 *
 * Enriches a DataFrame with __tree_path and __tree_agg columns for
 * AG Grid Enterprise Tree Data display. The hierarchy is defined by
 * ordered field inputs (root to leaf).
 *
 * Inputs:
 *   - csv (Csv): input DataFrame
 *   - field (Field): first hierarchy column (required, root level)
 *   - field_1 to field_99 (Field, optional): additional hierarchy levels
 *
 * Outputs:
 *   - csv (Csv): enriched DataFrame with __tree_path and __tree_agg columns
 *
 * Properties:
 *   - _aggregation: aggregation function (sum, avg, min, max, first, count)
 */
void registerTreeGroupNode();

} // namespace nodes
