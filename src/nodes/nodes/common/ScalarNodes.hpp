#pragma once

namespace nodes {

/**
 * Register all scalar value nodes
 */
void registerScalarNodes();

/** int_value - outputs configurable integer (_value property) */
void registerIntValueNode();

/** double_value - outputs configurable double (_value property) */
void registerDoubleValueNode();

/** string_value - outputs configurable string (_value property) */
void registerStringValueNode();

/** bool_value - outputs configurable boolean (_value property) */
void registerBoolValueNode();

/** null_value - outputs null */
void registerNullValueNode();

/** string_as_field - interprets a string as a field name */
void registerStringAsFieldNode();

/** string_as_fields - interprets a JSON array of strings as multiple field names */
void registerStringAsFieldsNode();

/** date_value - converts date string to timestamp */
void registerDateValueNode();

/** current_date - returns current date with optional year/month/day offsets */
void registerCurrentDateNode();

/** scalars_to_csv - combines field/value pairs into a single-row CSV */
void registerScalarsToCsvNode();

/** csv_value - outputs configurable CSV DataFrame (_value property) */
void registerCsvValueNode();

} // namespace nodes
