#include "DynamicNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"

namespace nodes {

void registerDynamicNodes() {
    registerDynamicBeginNode();
    registerDynamicEndNode();
}

void registerDynamicBeginNode() {
    NodeBuilder("dynamic_begin", "dynamic")
        .input("csv", Type::Csv)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto csvWL = ctx.getInputWorkload("csv");
            if (csvWL.isNull()) {
                ctx.setError("No CSV input");
                return;
            }
            // Pass-through the CSV
            ctx.setOutput("csv", csvWL.getCsv());
        })
        .buildAndRegister();
}

void registerDynamicEndNode() {
    NodeBuilder("dynamic_end", "dynamic")
        .input("csv", Type::Csv)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto csvWL = ctx.getInputWorkload("csv");
            if (csvWL.isNull()) {
                ctx.setError("No CSV input");
                return;
            }
            // Pass-through the CSV
            ctx.setOutput("csv", csvWL.getCsv());
        })
        .buildAndRegister();
}

} // namespace nodes
