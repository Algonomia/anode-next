#pragma once

namespace nodes {

/**
 * Register string transformation nodes:
 * - add_column: Add/set a column in a CSV with a given value
 * - json_extract: Extract a value from a JSON string by key
 * - trim: Remove leading/trailing whitespace
 * - to_lower: Convert to lowercase
 * - to_upper: Convert to uppercase
 * - replace: Replace first occurrence of a string
 * - to_integer: Convert string to integer
 * - substring: Extract substring between positions
 * - split: Split by delimiter and extract element at position
 * - unidecode: Remove accents and normalize special characters
 * - trim_integer: Trim whitespace and remove leading zeros
 * - concat: Concatenate suffixes to string
 * - concat_prefix: Concatenate prefixes to string
 *
 * All share the dual-mode signature:
 *   Inputs:
 *     - csv (Csv, optional): CSV for vector operations
 *     - src (String|Field): source value or column
 *     - dest (Field, optional): destination column name (default: src)
 *   Outputs:
 *     - csv (Csv): result CSV (vector mode)
 *     - result (String): scalar result (first row or scalar value)
 */
void registerStringNodes();

void registerAddColumnNode();
void registerJsonExtractNode();
void registerTrimNode();
void registerToLowerNode();
void registerToUpperNode();
void registerReplaceNode();
void registerToIntegerNode();
void registerSubstringNode();
void registerSplitNode();
void registerUnidecodeNode();
void registerTrimIntegerNode();
void registerConcatNode();
void registerConcatPrefixNode();

} // namespace nodes
