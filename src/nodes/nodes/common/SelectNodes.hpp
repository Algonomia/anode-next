#pragma once

namespace nodes {

/**
 * Register all select nodes
 */
void registerSelectNodes();

/**
 * select_by_name
 *
 * Keeps only the columns whose names are specified via field inputs.
 *
 * Inputs:
 *   - csv: Input CSV (required)
 *   - column: First column to keep (Field)
 *   - column_1..column_99: Additional columns to keep (Field, optional)
 *
 * Outputs:
 *   - csv: Result DataFrame with only selected columns
 */
void registerSelectByNameNode();

/**
 * select_by_pos
 *
 * Keeps or removes columns based on boolean inputs at each position.
 *
 * Inputs:
 *   - csv: Input CSV (required)
 *   - col_0..col_99: Boolean for each column position (optional)
 *
 * Properties:
 *   - _default: Default behavior when no input connected ("true" or "false")
 *
 * Outputs:
 *   - csv: Result DataFrame with selected columns
 */
void registerSelectByPosNode();

/**
 * reorder_columns
 *
 * Reorders columns in a CSV. Specified columns come first in the given order,
 * then remaining columns follow in their original order.
 *
 * Inputs:
 *   - csv: Input CSV (required)
 *   - column: First column to place at front (Field)
 *   - column_1..column_99: Additional columns to reorder (Field, optional)
 *
 * Outputs:
 *   - csv: Result DataFrame with reordered columns
 */
void registerReorderColumnsNode();

/**
 * clean_tmp_columns
 *
 * Removes all columns whose names start with "_tmp_" from the CSV.
 * Useful for cleaning up temporary columns created by the dynamic equation system.
 *
 * Inputs:
 *   - csv: Input CSV (required)
 *
 * Outputs:
 *   - csv: Result DataFrame without _tmp_* columns
 */
void registerCleanTmpColumnsNode();

/**
 * remap_by_name
 *
 * Renames columns in a CSV using col/dest pairs.
 * Dynamic inputs: col (old name) + dest (new name), with +/- buttons.
 *
 * Inputs:
 *   - csv: Input CSV (required)
 *   - col, dest: First rename pair (Field, required)
 *   - col_1/dest_1..col_99/dest_99: Additional rename pairs (Field, optional)
 *
 * Properties:
 *   - _unmapped: "keep" (default) or "remove" unmapped columns
 *
 * Outputs:
 *   - csv: Result DataFrame with renamed columns
 */
void registerRemapByNameNode();

/**
 * remap_by_csv
 *
 * Renames columns in a CSV using a mapping CSV.
 *
 * Inputs:
 *   - csv: Input CSV (required)
 *   - mapping: Mapping CSV with old/new column names (required)
 *   - col: Field pointing to the old-name column in mapping CSV (required)
 *   - dest: Field pointing to the new-name column in mapping CSV (required)
 *
 * Properties:
 *   - _unmapped: "keep" (default) or "remove" unmapped columns
 *
 * Outputs:
 *   - csv: Result DataFrame with renamed columns
 */
void registerRemapByCsvNode();

} // namespace nodes
