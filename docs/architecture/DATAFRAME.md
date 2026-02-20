# DataFrame Architecture

## Overview

The DataFrame library is a high-performance, columnar data structure optimized for analytical queries. It follows the Single Responsibility Principle (SRP) with specialized classes for each operation type.

## File Structure

```
src/dataframe/
├── DataFrame.hpp/cpp           # Main container class
├── DataFrameFilter.hpp/cpp     # Filtering operations
├── DataFrameSorter.hpp/cpp     # Sorting operations
├── DataFrameAggregator.hpp/cpp # GroupBy and aggregations
├── DataFrameJoiner.hpp/cpp     # Join operations
├── DataFrameSerializer.hpp/cpp # JSON/String output
├── DataFrameIO.hpp/cpp         # CSV I/O
├── Column.hpp                  # Column type definitions
└── StringPool.hpp              # String interning
```

## Class Diagram

```
                    ┌─────────────────────┐
                    │     DataFrame       │
                    │─────────────────────│
                    │ - m_columns         │
                    │ - m_columnOrder     │
                    │ - m_string_pool     │
                    │─────────────────────│
                    │ + filter()          │──────► DataFrameFilter
                    │ + orderBy()         │──────► DataFrameSorter
                    │ + groupBy()         │──────► DataFrameAggregator
                    │ + innerJoin()       │──────► DataFrameJoiner
                    │ + toString()        │──────► DataFrameSerializer
                    │ + toJson()          │──────► DataFrameSerializer
                    └─────────┬───────────┘
                              │
                              │ contains
                              ▼
              ┌───────────────────────────────────┐
              │           IColumn                 │
              │───────────────────────────────────│
              │ + getName()                       │
              │ + getType()                       │
              │ + size()                          │
              │ + filterEqual/LessThan/...()     │
              │ + filterByIndices()              │
              └───────────────┬───────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
    ┌───────────┐       ┌───────────┐       ┌───────────┐
    │ IntColumn │       │DoubleCol. │       │StringCol. │
    │───────────│       │───────────│       │───────────│
    │vector<int>│       │vector<dbl>│       │vector<ID> │
    └───────────┘       └───────────┘       └─────┬─────┘
                                                  │
                                                  ▼
                                            ┌───────────┐
                                            │StringPool │
                                            │───────────│
                                            │ intern()  │
                                            │getString()│
                                            └───────────┘
```

## Column Types

### IntColumn
- Storage: `std::vector<int>`
- Best for: Integer IDs, counts, indices
- Comparisons: Direct integer comparison

### DoubleColumn
- Storage: `std::vector<double>`
- Best for: Floating point values, prices, measurements
- Comparisons: Direct double comparison

### StringColumn
- Storage: `std::vector<uint32_t>` (StringPool IDs)
- Best for: Text data, categories, names
- Optimization: String interning via StringPool

## String Interning (StringPool)

The `StringPool` provides O(1) string equality comparison by assigning unique IDs to strings:

```cpp
// Without StringPool: O(n) comparison
if (str1 == str2) { ... }  // Compares each character

// With StringPool: O(1) comparison
if (id1 == id2) { ... }    // Compares two uint32_t
```

### Benefits:
- Memory efficiency: Each unique string stored once
- Fast equality: Integer comparison instead of string comparison
- Fast hashing: ID-based hashing for groupBy operations

## Operations

### Filter (DataFrameFilter)
Applies filter conditions and returns matching row indices.

**Supported operators:**
- `==`, `!=` - Equality
- `<`, `<=`, `>`, `>=` - Comparison
- `contains` - Substring match (strings only)

**Implementation:**
```cpp
// Intersection of filter results (AND logic)
for each filter:
    matchingIndices = column->filterEqual/LessThan/...(value)
    result = set_intersection(result, matchingIndices)
```

### Sort (DataFrameSorter)
Creates sorted indices using specialized comparators per column type.

**Optimizations:**
- Branchless comparators: `(a > b) - (a < b)`
- Type-specialized lambdas captured at setup time
- `std::stable_sort` for consistent ordering

### GroupBy (DataFrameAggregator)
Groups rows and computes aggregations.

**Supported aggregations:**
- `count` - Row count per group
- `sum` - Sum of values
- `avg` - Average value
- `min`, `max` - Extremes
- `first` - First value of the group (useful for strings)
- `blank` - Returns null (default for unspecified columns)

**Implementation:**
```cpp
// Hash-based grouping using uint64_t keys
GroupKey = { extractValue(col1, row), extractValue(col2, row), ... }
groups[GroupKey].push_back(rowIndex)

// Compute aggregations per group
for each group:
    for each aggregation:
        compute(function, column, rowIndices)
```

### GroupByTree (DataFrameAggregator)
Hierarchical groupBy that preserves child rows for tree visualization (Tabulator).

Returns columnar format with `_children` containing original rows:
```json
{
  "columns": ["col1", "col2", ...],
  "data": [
    [groupVal1, aggVal1, [[child1...], [child2...]]],
    [groupVal2, aggVal2, [[child3...]]]
  ]
}
```

### Pivot (DataFrameAggregator)
Transposes values from a column into multiple columns.

**Example:**
```
Input:                          Output:
question_id, task_id, value     task_id, value_1, value_2, value_3
1, 1, 10                   =>   1, 10, 11, 12
2, 1, 11                        2, 13, 14, 15
3, 1, 12
1, 2, 13
...
```

**Parameters:**
- `pivotColumn` - Column whose values become new column names
- `valueColumn` - Column whose values fill the new columns
- `indexColumns` - Columns that identify each row (optional, auto-detected)
- `prefix` - Prefix for new columns (default: `{valueColumn}_`)

### Inner Join (DataFrameJoiner)
Joins two DataFrames on N key columns with O(n+m) hash-based algorithm.

**Example:**
```
Left DataFrame:              Right DataFrame:
id, name, dept_id            dept_id, dept_name
1, Alice, 10           +     10, Engineering
2, Bob, 20                   20, Sales
3, Carol, 10                 30, Marketing

Result (innerJoin on dept_id):
id, name, dept_id, dept_name
1, Alice, 10, Engineering
3, Carol, 10, Engineering
2, Bob, 20, Sales
```

**JSON Format:**
```json
{
  "keys": [
    {"left": "dept_id", "right": "dept_id"},
    {"left": "region", "right": "region_code"}
  ]
}
```

**Features:**
- Multiple key columns support (composite keys)
- Keys can have different names between left and right DataFrames
- Column name collision handling: adds `_right` suffix to duplicate columns
- Hash-based algorithm: O(n+m) time complexity
- Builds hash table from smaller DataFrame for memory efficiency

**Type constraints:**
- Key columns must have matching types (INT-INT, DOUBLE-DOUBLE, STRING-STRING)
- Type mismatch throws `std::invalid_argument`

### Serialization (DataFrameSerializer)
Converts DataFrame to columnar JSON format for efficient transfer:

```json
{
  "columns": ["col1", "col2", "col3"],
  "data": [
    [1, "a", 10.5],
    [2, "b", 20.3]
  ]
}
```

This format avoids repeating column names for each row, reducing payload size significantly (10-20x smaller than row-based JSON).

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Filter (equality) | O(n) | Single pass |
| Filter (string ==) | O(n) | ID comparison, very fast |
| Sort | O(n log n) | std::stable_sort |
| GroupBy | O(n) | Hash-based grouping |
| CSV Read | O(n) | Type detection + parsing |

## Memory Layout

```
DataFrame:
┌─────────────────────────────────────────┐
│ m_columns: unordered_map<string, IColumnPtr>
│ m_columnOrder: vector<string>
│ m_string_pool: shared_ptr<StringPool>
└─────────────────────────────────────────┘

IntColumn:              StringColumn:
┌─────────────────┐     ┌─────────────────┐
│ [1][2][3][4]... │     │ [0][1][0][2]... │ ← StringPool IDs
└─────────────────┘     └─────────────────┘
  Contiguous memory       Contiguous memory
  Cache-friendly          Cache-friendly
```

## Usage Example

```cpp
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameIO.hpp"

// Load CSV
auto df = DataFrameIO::readCSV("data.csv");

// Filter
json filters = json::array({
    {{"column", "age"}, {"operator", ">="}, {"value", 18}}
});
auto filtered = df->filter(filters);

// Sort
json orders = json::array({
    {{"column", "name"}, {"order", "asc"}}
});
auto sorted = filtered->orderBy(orders);

// GroupBy
json groupBy = {
    {"groupBy", {"country"}},
    {"aggregations", json::array({
        {{"column", "salary"}, {"function", "avg"}, {"alias", "avg_salary"}}
    })}
};
auto grouped = df->groupBy(groupBy);

// Inner Join
auto employees = DataFrameIO::readCSV("employees.csv");
auto departments = DataFrameIO::readCSV("departments.csv");

json joinSpec = {
    {"keys", json::array({
        {{"left", "dept_id"}, {"right", "id"}}
    })}
};
auto joined = employees->innerJoin(departments, joinSpec);
```
