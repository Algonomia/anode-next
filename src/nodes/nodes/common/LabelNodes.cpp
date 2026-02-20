#include "LabelNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/LabelRegistry.hpp"

namespace nodes {

// =============================================================================
// LABEL DEFINE NODES
// Store a value under a unique identifier
// =============================================================================

void registerLabelDefineCsvNode() {
    NodeBuilder("label_define_csv", "label")
        .input("value", Type::Csv)
        .output("value", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            auto value = ctx.getInputWorkload("value");
            if (value.isNull()) {
                ctx.setError("No value connected to define");
                return;
            }

            LabelRegistry::instance().defineLabel(identifierProp.getString(), value);
            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelDefineFieldNode() {
    NodeBuilder("label_define_field", "label")
        .input("value", Type::Field)
        .output("value", Type::Field)
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            auto value = ctx.getInputWorkload("value");
            if (value.isNull()) {
                ctx.setError("No value connected to define");
                return;
            }

            LabelRegistry::instance().defineLabel(identifierProp.getString(), value);
            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelDefineIntNode() {
    NodeBuilder("label_define_int", "label")
        .input("value", Type::Int)
        .output("value", Type::Int)
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            auto value = ctx.getInputWorkload("value");
            if (value.isNull()) {
                ctx.setError("No value connected to define");
                return;
            }

            LabelRegistry::instance().defineLabel(identifierProp.getString(), value);
            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelDefineDoubleNode() {
    NodeBuilder("label_define_double", "label")
        .input("value", Type::Double)
        .output("value", Type::Double)
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            auto value = ctx.getInputWorkload("value");
            if (value.isNull()) {
                ctx.setError("No value connected to define");
                return;
            }

            LabelRegistry::instance().defineLabel(identifierProp.getString(), value);
            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelDefineStringNode() {
    NodeBuilder("label_define_string", "label")
        .input("value", Type::String)
        .output("value", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            auto value = ctx.getInputWorkload("value");
            if (value.isNull()) {
                ctx.setError("No value connected to define");
                return;
            }

            LabelRegistry::instance().defineLabel(identifierProp.getString(), value);
            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

// =============================================================================
// LABEL REF NODES
// Retrieve a value by its identifier
// =============================================================================

void registerLabelRefCsvNode() {
    NodeBuilder("label_ref_csv", "label")
        .output("value", Type::Csv)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            std::string identifier = identifierProp.getString();
            if (!LabelRegistry::instance().hasLabel(identifier)) {
                ctx.setError("Label not found: " + identifier);
                return;
            }

            auto value = LabelRegistry::instance().getLabel(identifier);
            if (value.getType() != Type::Csv) {
                ctx.setError("Label '" + identifier + "' is not a CSV");
                return;
            }

            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelRefFieldNode() {
    NodeBuilder("label_ref_field", "label")
        .output("value", Type::Field)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            std::string identifier = identifierProp.getString();
            if (!LabelRegistry::instance().hasLabel(identifier)) {
                ctx.setError("Label not found: " + identifier);
                return;
            }

            auto value = LabelRegistry::instance().getLabel(identifier);
            if (value.getType() != Type::Field) {
                ctx.setError("Label '" + identifier + "' is not a Field");
                return;
            }

            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelRefIntNode() {
    NodeBuilder("label_ref_int", "label")
        .output("value", Type::Int)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            std::string identifier = identifierProp.getString();
            if (!LabelRegistry::instance().hasLabel(identifier)) {
                ctx.setError("Label not found: " + identifier);
                return;
            }

            auto value = LabelRegistry::instance().getLabel(identifier);
            if (value.getType() != Type::Int) {
                ctx.setError("Label '" + identifier + "' is not an Int");
                return;
            }

            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelRefDoubleNode() {
    NodeBuilder("label_ref_double", "label")
        .output("value", Type::Double)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            std::string identifier = identifierProp.getString();
            if (!LabelRegistry::instance().hasLabel(identifier)) {
                ctx.setError("Label not found: " + identifier);
                return;
            }

            auto value = LabelRegistry::instance().getLabel(identifier);
            if (value.getType() != Type::Double) {
                ctx.setError("Label '" + identifier + "' is not a Double");
                return;
            }

            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

void registerLabelRefStringNode() {
    NodeBuilder("label_ref_string", "label")
        .output("value", Type::String)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            auto identifierProp = ctx.getInputWorkload("_label");
            if (identifierProp.isNull() || identifierProp.getString().empty()) {
                ctx.setError("Label identifier required (_label property)");
                return;
            }

            std::string identifier = identifierProp.getString();
            if (!LabelRegistry::instance().hasLabel(identifier)) {
                ctx.setError("Label not found: " + identifier);
                return;
            }

            auto value = LabelRegistry::instance().getLabel(identifier);
            if (value.getType() != Type::String) {
                ctx.setError("Label '" + identifier + "' is not a String");
                return;
            }

            ctx.setOutput("value", value);
        })
        .buildAndRegister();
}

// =============================================================================
// REGISTRATION
// =============================================================================

void registerLabelNodes() {
    // Define nodes
    registerLabelDefineCsvNode();
    registerLabelDefineFieldNode();
    registerLabelDefineIntNode();
    registerLabelDefineDoubleNode();
    registerLabelDefineStringNode();

    // Ref nodes
    registerLabelRefCsvNode();
    registerLabelRefFieldNode();
    registerLabelRefIntNode();
    registerLabelRefDoubleNode();
    registerLabelRefStringNode();
}

} // namespace nodes
