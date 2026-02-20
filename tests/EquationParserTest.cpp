#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "nodes/EquationParser.hpp"

using namespace nodes;

// =============================================================================
// Tokenizer Tests
// =============================================================================

TEST_CASE("Tokenizer basic tokens", "[EquationParser]") {
    Tokenizer tok("A=1+2");

    auto t1 = tok.next();
    REQUIRE(t1.type == TokenType::IDENT);
    REQUIRE(t1.stringValue == "A");

    auto t2 = tok.next();
    REQUIRE(t2.type == TokenType::EQ);

    auto t3 = tok.next();
    REQUIRE(t3.type == TokenType::NUMBER);
    REQUIRE(t3.numericValue == 1.0);

    auto t4 = tok.next();
    REQUIRE(t4.type == TokenType::PLUS);

    auto t5 = tok.next();
    REQUIRE(t5.type == TokenType::NUMBER);
    REQUIRE(t5.numericValue == 2.0);

    auto t6 = tok.next();
    REQUIRE(t6.type == TokenType::END);
}

TEST_CASE("Tokenizer field references", "[EquationParser]") {
    Tokenizer tok("$A+$field_name");

    auto t1 = tok.next();
    REQUIRE(t1.type == TokenType::FIELD);
    REQUIRE(t1.stringValue == "A");

    auto t2 = tok.next();
    REQUIRE(t2.type == TokenType::PLUS);

    auto t3 = tok.next();
    REQUIRE(t3.type == TokenType::FIELD);
    REQUIRE(t3.stringValue == "field_name");
}

TEST_CASE("Tokenizer operators and parentheses", "[EquationParser]") {
    Tokenizer tok("($A*$B)-$C/$D");

    auto t1 = tok.next();
    REQUIRE(t1.type == TokenType::LPAREN);

    auto t2 = tok.next();
    REQUIRE(t2.type == TokenType::FIELD);
    REQUIRE(t2.stringValue == "A");

    auto t3 = tok.next();
    REQUIRE(t3.type == TokenType::STAR);

    auto t4 = tok.next();
    REQUIRE(t4.type == TokenType::FIELD);
    REQUIRE(t4.stringValue == "B");

    auto t5 = tok.next();
    REQUIRE(t5.type == TokenType::RPAREN);

    auto t6 = tok.next();
    REQUIRE(t6.type == TokenType::MINUS);

    auto t7 = tok.next();
    REQUIRE(t7.type == TokenType::FIELD);
    REQUIRE(t7.stringValue == "C");

    auto t8 = tok.next();
    REQUIRE(t8.type == TokenType::SLASH);

    auto t9 = tok.next();
    REQUIRE(t9.type == TokenType::FIELD);
    REQUIRE(t9.stringValue == "D");
}

TEST_CASE("Tokenizer decimal numbers", "[EquationParser]") {
    Tokenizer tok("3.14");

    auto t = tok.next();
    REQUIRE(t.type == TokenType::NUMBER);
    REQUIRE(t.numericValue == Catch::Approx(3.14));
}

// =============================================================================
// Parser Tests - Simple Equations
// =============================================================================

TEST_CASE("Parse simple addition", "[EquationParser]") {
    auto ops = parseEquation("C=$A+$B");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].src == "A");
    REQUIRE(ops[0].srcIsField == true);
    REQUIRE(ops[0].operand == "B");
    REQUIRE(ops[0].operandIsField == true);
    REQUIRE(ops[0].dest == "C");
}

TEST_CASE("Parse simple subtraction", "[EquationParser]") {
    auto ops = parseEquation("D=$A-$B");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "subtract");
    REQUIRE(ops[0].dest == "D");
}

TEST_CASE("Parse simple multiplication", "[EquationParser]") {
    auto ops = parseEquation("E=$A*$B");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "multiply");
}

TEST_CASE("Parse simple division", "[EquationParser]") {
    auto ops = parseEquation("F=$A/$B");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "divide");
}

// =============================================================================
// Parser Tests - Operator Precedence
// =============================================================================

TEST_CASE("Parse multiplication before addition", "[EquationParser]") {
    // A + B * C should be A + (B * C)
    auto ops = parseEquation("D=$A+$B*$C");

    REQUIRE(ops.size() == 2);
    // First: B * C -> tmp
    REQUIRE(ops[0].op == "multiply");
    REQUIRE(ops[0].src == "B");
    REQUIRE(ops[0].operand == "C");
    // Second: A + tmp -> D
    REQUIRE(ops[1].op == "add");
    REQUIRE(ops[1].src == "A");
    REQUIRE(ops[1].dest == "D");
}

TEST_CASE("Parse division before subtraction", "[EquationParser]") {
    // A - B / C should be A - (B / C)
    auto ops = parseEquation("D=$A-$B/$C");

    REQUIRE(ops.size() == 2);
    REQUIRE(ops[0].op == "divide");
    REQUIRE(ops[1].op == "subtract");
    REQUIRE(ops[1].dest == "D");
}

// =============================================================================
// Parser Tests - Parentheses
// =============================================================================

TEST_CASE("Parse parentheses override precedence", "[EquationParser]") {
    // (A + B) * C
    auto ops = parseEquation("D=($A+$B)*$C");

    REQUIRE(ops.size() == 2);
    // First: A + B -> tmp
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].src == "A");
    REQUIRE(ops[0].operand == "B");
    // Second: tmp * C -> D
    REQUIRE(ops[1].op == "multiply");
    REQUIRE(ops[1].operand == "C");
    REQUIRE(ops[1].dest == "D");
}

TEST_CASE("Parse nested parentheses", "[EquationParser]") {
    // ((A + B) * C) - D
    auto ops = parseEquation("E=(($A+$B)*$C)-$D");

    REQUIRE(ops.size() == 3);
    REQUIRE(ops[0].op == "add");      // A + B
    REQUIRE(ops[1].op == "multiply"); // (A+B) * C
    REQUIRE(ops[2].op == "subtract"); // ((A+B)*C) - D
    REQUIRE(ops[2].dest == "E");
}

// =============================================================================
// Parser Tests - Scalars
// =============================================================================

TEST_CASE("Parse field times scalar", "[EquationParser]") {
    auto ops = parseEquation("B=$A*42");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "multiply");
    REQUIRE(ops[0].src == "A");
    REQUIRE(ops[0].srcIsField == true);
    REQUIRE(ops[0].operandIsField == false);
    REQUIRE(ops[0].operandValue == 42.0);
    REQUIRE(ops[0].dest == "B");
}

TEST_CASE("Parse scalar plus field", "[EquationParser]") {
    auto ops = parseEquation("B=100+$A");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].srcIsField == false);
    REQUIRE(ops[0].operand == "A");
    REQUIRE(ops[0].operandIsField == true);
}

TEST_CASE("Parse decimal scalar", "[EquationParser]") {
    auto ops = parseEquation("B=$A*3.14");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].operandValue == Catch::Approx(3.14));
}

// =============================================================================
// Parser Tests - Complex Equations
// =============================================================================

TEST_CASE("Parse complex equation from plan example", "[EquationParser]") {
    // D=($A+$B)*$C generates:
    //   1. add(src=$A, operand=$B, dest=_tmp_0)
    //   2. multiply(src=_tmp_0, operand=$C, dest=D)
    auto ops = parseEquation("D=($A+$B)*$C");

    REQUIRE(ops.size() == 2);

    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].src == "A");
    REQUIRE(ops[0].srcIsField == true);
    REQUIRE(ops[0].operand == "B");
    REQUIRE(ops[0].operandIsField == true);
    REQUIRE(ops[0].dest == "_tmp_0");

    REQUIRE(ops[1].op == "multiply");
    REQUIRE(ops[1].src == "_tmp_0");
    REQUIRE(ops[1].srcIsField == false);
    REQUIRE(ops[1].operand == "C");
    REQUIRE(ops[1].operandIsField == true);
    REQUIRE(ops[1].dest == "D");
}

TEST_CASE("Parse chained multiplications", "[EquationParser]") {
    // A * B * C (left associative)
    auto ops = parseEquation("D=$A*$B*$C");

    REQUIRE(ops.size() == 2);
    // First: A * B -> tmp
    REQUIRE(ops[0].op == "multiply");
    REQUIRE(ops[0].src == "A");
    REQUIRE(ops[0].operand == "B");
    // Second: tmp * C -> D
    REQUIRE(ops[1].op == "multiply");
    REQUIRE(ops[1].operand == "C");
    REQUIRE(ops[1].dest == "D");
}

TEST_CASE("Parse multiple additions", "[EquationParser]") {
    auto ops = parseEquation("D=$A+$B+$C");

    REQUIRE(ops.size() == 2);
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[1].op == "add");
    REQUIRE(ops[1].dest == "D");
}

// =============================================================================
// Parser Tests - Spaces in Names
// =============================================================================

TEST_CASE("Parse field with spaces", "[EquationParser]") {
    auto ops = parseEquation("D=$Integration rate*2");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "multiply");
    REQUIRE(ops[0].src == "Integration rate");
    REQUIRE(ops[0].srcIsField == true);
    REQUIRE(ops[0].operandValue == 2.0);
    REQUIRE(ops[0].dest == "D");
}

TEST_CASE("Parse destination with spaces", "[EquationParser]") {
    auto ops = parseEquation("Do do=$A+$B");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].src == "A");
    REQUIRE(ops[0].operand == "B");
    REQUIRE(ops[0].dest == "Do do");
}

TEST_CASE("Parse both destination and field with spaces", "[EquationParser]") {
    auto ops = parseEquation("My Result = $First Value + $Second Value");

    REQUIRE(ops.size() == 1);
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].src == "First Value");
    REQUIRE(ops[0].srcIsField == true);
    REQUIRE(ops[0].operand == "Second Value");
    REQUIRE(ops[0].operandIsField == true);
    REQUIRE(ops[0].dest == "My Result");
}

TEST_CASE("Parse complex equation with spaces", "[EquationParser]") {
    auto ops = parseEquation("Total Price = ($Unit Price + $Tax Amount) * $Quantity");

    REQUIRE(ops.size() == 2);
    // First: Unit Price + Tax Amount -> tmp
    REQUIRE(ops[0].op == "add");
    REQUIRE(ops[0].src == "Unit Price");
    REQUIRE(ops[0].operand == "Tax Amount");
    // Second: tmp * Quantity -> Total Price
    REQUIRE(ops[1].op == "multiply");
    REQUIRE(ops[1].operand == "Quantity");
    REQUIRE(ops[1].dest == "Total Price");
}

// =============================================================================
// Reconstruction Tests
// =============================================================================

TEST_CASE("Reconstruct simple addition", "[EquationParser]") {
    std::vector<MathOp> ops = {
        {"add", "A", true, "B", true, 0.0, "C"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "C = $A + $B");
}

TEST_CASE("Reconstruct simple multiplication", "[EquationParser]") {
    std::vector<MathOp> ops = {
        {"multiply", "A", true, "B", true, 0.0, "C"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "C = $A * $B");
}

TEST_CASE("Reconstruct with precedence - no parens needed", "[EquationParser]") {
    // A + B * C -> ops in order: B*C->tmp, A+tmp->D
    std::vector<MathOp> ops = {
        {"multiply", "B", true, "C", true, 0.0, "_tmp_0"},
        {"add", "A", true, "_tmp_0", false, 0.0, "D"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "D = $A + $B * $C");
}

TEST_CASE("Reconstruct with precedence - parens needed", "[EquationParser]") {
    // (A + B) * C -> ops in order: A+B->tmp, tmp*C->D
    std::vector<MathOp> ops = {
        {"add", "A", true, "B", true, 0.0, "_tmp_0"},
        {"multiply", "_tmp_0", false, "C", true, 0.0, "D"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "D = ($A + $B) * $C");
}

TEST_CASE("Reconstruct nested parentheses", "[EquationParser]") {
    // ((A + B) * C) - D
    std::vector<MathOp> ops = {
        {"add", "A", true, "B", true, 0.0, "_tmp_0"},
        {"multiply", "_tmp_0", false, "C", true, 0.0, "_tmp_1"},
        {"subtract", "_tmp_1", false, "D", true, 0.0, "E"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "E = ($A + $B) * $C - $D");
}

TEST_CASE("Reconstruct with scalars", "[EquationParser]") {
    std::vector<MathOp> ops = {
        {"multiply", "A", true, "42", false, 42.0, "B"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "B = $A * 42");
}

TEST_CASE("Reconstruct field names with spaces", "[EquationParser]") {
    std::vector<MathOp> ops = {
        {"add", "First Value", true, "Second Value", true, 0.0, "My Result"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 1);
    REQUIRE(equations[0] == "My Result = $First Value + $Second Value");
}

TEST_CASE("Reconstruct multiple final destinations", "[EquationParser]") {
    // Two independent equations: C = A + B and F = D * E
    std::vector<MathOp> ops = {
        {"add", "A", true, "B", true, 0.0, "C"},
        {"multiply", "D", true, "E", true, 0.0, "F"}
    };

    auto equations = reconstructEquations(ops);
    REQUIRE(equations.size() == 2);
    // Order may vary, check both exist
    bool foundC = false, foundF = false;
    for (const auto& eq : equations) {
        if (eq == "C = $A + $B") foundC = true;
        if (eq == "F = $D * $E") foundF = true;
    }
    REQUIRE(foundC);
    REQUIRE(foundF);
}

TEST_CASE("Roundtrip: parse and reconstruct simple", "[EquationParser]") {
    std::string original = "C = $A + $B";
    auto ops = parseEquation(original);
    auto reconstructed = reconstructEquations(ops);
    REQUIRE(reconstructed.size() == 1);
    REQUIRE(reconstructed[0] == original);
}

TEST_CASE("Roundtrip: parse and reconstruct with parens", "[EquationParser]") {
    std::string original = "D = ($A + $B) * $C";
    auto ops = parseEquation(original);
    auto reconstructed = reconstructEquations(ops);
    REQUIRE(reconstructed.size() == 1);
    REQUIRE(reconstructed[0] == original);
}

TEST_CASE("Roundtrip: parse and reconstruct complex", "[EquationParser]") {
    std::string original = "E = ($A + $B) * $C - $D";
    auto ops = parseEquation(original);
    auto reconstructed = reconstructEquations(ops);
    REQUIRE(reconstructed.size() == 1);
    REQUIRE(reconstructed[0] == original);
}

TEST_CASE("Roundtrip: parse and reconstruct with scalar", "[EquationParser]") {
    std::string original = "B = $A * 42";
    auto ops = parseEquation(original);
    auto reconstructed = reconstructEquations(ops);
    REQUIRE(reconstructed.size() == 1);
    REQUIRE(reconstructed[0] == original);
}

TEST_CASE("Roundtrip: parse and reconstruct with spaces in names", "[EquationParser]") {
    std::string original = "My Result = $First Value + $Second Value";
    auto ops = parseEquation(original);
    auto reconstructed = reconstructEquations(ops);
    REQUIRE(reconstructed.size() == 1);
    REQUIRE(reconstructed[0] == original);
}

TEST_CASE("Shared temp counter across multiple equations", "[EquationParser]") {
    int tmpCounter = 0;

    // First equation: D = ($A + $B) * 2
    // Parser creates: _tmp_0 for (A+B), then _tmp_1 for the multiply (renamed to D)
    auto ops1 = parseEquation("D = ($A + $B) * 2", &tmpCounter);
    REQUIRE(ops1.size() == 2);
    REQUIRE(ops1[0].dest == "_tmp_0");  // A + B -> _tmp_0
    REQUIRE(ops1[1].dest == "D");       // _tmp_0 * 2 -> D (internally was _tmp_1)
    REQUIRE(tmpCounter == 2);           // Counter is 2 (both temps were allocated)

    // Second equation: E = ($D * $C) + 10
    // Parser creates: _tmp_2 for (D*C), then _tmp_3 for the add (renamed to E)
    auto ops2 = parseEquation("E = ($D * $C) + 10", &tmpCounter);
    REQUIRE(ops2.size() == 2);
    REQUIRE(ops2[0].dest == "_tmp_2");  // D * C -> _tmp_2 (not _tmp_0!)
    REQUIRE(ops2[1].dest == "E");       // _tmp_2 + 10 -> E
    REQUIRE(tmpCounter == 4);           // Counter is 4

    // Third equation: F = ($E + $E) * 2
    // Parser creates: _tmp_4 for (E+E), then _tmp_5 for the multiply (renamed to F)
    auto ops3 = parseEquation("F = ($E + $E) * 2", &tmpCounter);
    REQUIRE(ops3.size() == 2);
    REQUIRE(ops3[0].dest == "_tmp_4");  // E + E -> _tmp_4 (not _tmp_0!)
    REQUIRE(ops3[1].dest == "F");       // _tmp_4 * 2 -> F
    REQUIRE(tmpCounter == 6);           // Counter is 6

    // Combine all ops and reconstruct - each equation should reconstruct correctly
    std::vector<MathOp> allOps;
    allOps.insert(allOps.end(), ops1.begin(), ops1.end());
    allOps.insert(allOps.end(), ops2.begin(), ops2.end());
    allOps.insert(allOps.end(), ops3.begin(), ops3.end());

    auto equations = reconstructEquations(allOps);
    REQUIRE(equations.size() == 3);

    // Check that all three equations are reconstructed
    // Note: redundant parentheses are simplified in reconstruction
    bool foundD = false, foundE = false, foundF = false;
    for (const auto& eq : equations) {
        if (eq == "D = ($A + $B) * 2") foundD = true;
        if (eq == "E = $D * $C + 10") foundE = true;  // No parens needed (mul before add)
        if (eq == "F = ($E + $E) * 2") foundF = true;
    }
    REQUIRE(foundD);
    REQUIRE(foundE);
    REQUIRE(foundF);
}
