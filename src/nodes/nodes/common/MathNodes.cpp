#include "MathNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "dataframe/DataFrame.hpp"
#include "dataframe/Column.hpp"
#include <functional>
#include <cmath>

namespace nodes {

void registerMathNodes() {
    registerAddNode();
    registerSubtractNode();
    registerMultiplyNode();
    registerDivideNode();
    registerModulusNode();
}

// Helper to create math nodes with shared logic
using MathOp = std::function<double(double, double)>;

static void registerMathNode(const std::string& name, MathOp op) {
    NodeBuilder(name, "math")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::Int, Type::Double, Type::Field})
        .inputOptional("dest", Type::Field)
        .input("operand", {Type::Int, Type::Double, Type::Field})
        .output("csv", Type::Csv)
        .output("result", Type::Double)
        .onCompile([op, name](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto operand = ctx.getInputWorkload("operand");
            auto dest = ctx.getInputWorkload("dest");

            // Check required inputs
            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }
            if (operand.isNull()) {
                ctx.setError("Input 'operand' is not connected");
                return;
            }

            // Check if either input is a field (vector mode)
            bool hasField = (src.getType() == Type::Field || operand.getType() == Type::Field);

            if (!hasField) {
                // Pure scalar mode - simple operation
                double result = op(src.getDouble(), operand.getDouble());
                ctx.setOutput("result", result);
                return;
            }

            // Vector mode - need CSV for field lookups
            auto csv = ctx.getActiveCsv();
            if (!csv) {
                auto csvInput = ctx.getInputWorkload("csv");
                if (!csvInput.isNull()) {
                    csv = csvInput.getCsv();
                }
            }

            if (!csv) {
                ctx.setError("Field inputs require a CSV connection");
                return;
            }

            auto header = csv->getColumnNames();
            size_t rowCount = csv->rowCount();

            // Determine destination column name
            std::string destColName;
            if (!dest.isNull()) {
                // Use explicit dest
                destColName = dest.getString();
            } else if (src.getType() == Type::Field) {
                // Overwrite source column (like original JS behavior)
                destColName = src.getString();
            } else {
                // Scalar src with no dest - generate name
                destColName = "_" + name + "_result";
            }

            // Create result column
            auto resultCol = std::make_shared<dataframe::DoubleColumn>(destColName);
            resultCol->reserve(rowCount);

            // Compute for each row (with broadcasting for scalars)
            for (size_t i = 0; i < rowCount; ++i) {
                double va = src.getDoubleAtRow(i, header, csv);
                double vb = operand.getDoubleAtRow(i, header, csv);
                resultCol->push_back(op(va, vb));
            }

            // Create output CSV: clone original + set result column
            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            // Copy original columns (except dest if it exists, we'll replace it)
            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }

            // Add/replace result column
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            // Also output scalar result (first row)
            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            } else {
                ctx.setOutput("result", 0.0);
            }
        })
        .buildAndRegister();
}

void registerAddNode() {
    registerMathNode("add", [](double a, double b) { return a + b; });
}

void registerSubtractNode() {
    registerMathNode("subtract", [](double a, double b) { return a - b; });
}

void registerMultiplyNode() {
    registerMathNode("multiply", [](double a, double b) { return a * b; });
}

void registerDivideNode() {
    registerMathNode("divide", [](double a, double b) { return a / b; });
}

void registerModulusNode() {
    registerMathNode("modulus", [](double a, double b) { return std::fmod(a, b); });
}

} // namespace nodes
