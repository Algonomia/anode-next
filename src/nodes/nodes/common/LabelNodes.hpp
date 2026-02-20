#pragma once

namespace nodes {

/**
 * Register all label nodes
 */
void registerLabelNodes();

// Define nodes - store a value under a name
void registerLabelDefineCsvNode();
void registerLabelDefineFieldNode();
void registerLabelDefineIntNode();
void registerLabelDefineDoubleNode();
void registerLabelDefineStringNode();

// Ref nodes - retrieve a value by name
void registerLabelRefCsvNode();
void registerLabelRefFieldNode();
void registerLabelRefIntNode();
void registerLabelRefDoubleNode();
void registerLabelRefStringNode();

} // namespace nodes
