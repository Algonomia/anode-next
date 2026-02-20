#include "register.hpp"
#include "ScalarNodes.hpp"
#include "CsvNodes.hpp"
#include "MathNodes.hpp"
#include "AggregateNodes.hpp"
#include "SelectNodes.hpp"
#include "StringNodes.hpp"
#include "PostgresNodes.hpp"
#include "DynamicNodes.hpp"
#include "LabelNodes.hpp"
#include "VizNodes.hpp"

namespace common {

void registerNodes() {
    nodes::registerScalarNodes();
    nodes::registerCsvNodes();
    nodes::registerMathNodes();
    nodes::registerAggregateNodes();
    nodes::registerSelectNodes();
    nodes::registerStringNodes();
    nodes::registerPostgresNodes();
    nodes::registerDynamicNodes();
    nodes::registerLabelNodes();
    nodes::registerVizNodes();
}

void init(nodes::PluginContext& /*ctx*/) {
    // Common nodes don't need initialization
}

void shutdown() {
    // Nothing to clean up
}

} // namespace common
