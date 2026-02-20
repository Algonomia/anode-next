#pragma once

namespace nodes {

/**
 * Register dynamic marker nodes
 * - dynamic_begin: marks the start of a dynamic injection zone
 * - dynamic_end: marks the end of a dynamic injection zone
 *
 * Both are pass-through nodes (input csv -> output csv) with a _name widget
 * that identifies the zone for dynamic node injection via API.
 */
void registerDynamicNodes();
void registerDynamicBeginNode();
void registerDynamicEndNode();

} // namespace nodes
