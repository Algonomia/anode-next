#pragma once

#include "nodes/PluginContext.hpp"

namespace common {

void registerNodes();
void init(nodes::PluginContext& ctx);
void shutdown();

} // namespace common
