#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>

namespace nodes {

// Forward declaration
class NodeGraph;

/**
 * Represents a single math operation to be translated into a node
 */
struct MathOp {
    std::string op;           // "add", "subtract", "multiply", "divide"
    std::string src;          // field name or "_tmp_N"
    bool srcIsField;          // true if src is a field (column), false if temporary
    std::string operand;      // field name, "_tmp_N", or string representation of value
    bool operandIsField;      // true if operand is a field (column), false if scalar/temporary
    double operandValue;      // numeric value if operandIsField is false and operand is not a tmp
    std::string dest;         // destination field name
};

/**
 * Token types for the equation lexer
 */
enum class TokenType {
    IDENT,      // Variable name (destination)
    FIELD,      // $X (field reference)
    NUMBER,     // Integer or double
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    LPAREN,     // (
    RPAREN,     // )
    EQ,         // =
    END         // End of input
};

/**
 * Token with value
 */
struct Token {
    TokenType type;
    std::string stringValue;
    double numericValue = 0.0;
};

/**
 * AST node types for expression parsing
 */
struct ASTNode;

using ASTNodePtr = std::shared_ptr<ASTNode>;

struct ASTNode {
    enum class Type {
        NUMBER,     // Numeric literal
        FIELD,      // Field reference ($X)
        BINOP       // Binary operation
    };

    Type type;
    double numericValue = 0.0;      // For NUMBER
    std::string fieldName;          // For FIELD
    std::string op;                 // For BINOP: "+", "-", "*", "/"
    ASTNodePtr left;                // For BINOP
    ASTNodePtr right;               // For BINOP
};

/**
 * Parse an equation string into a list of MathOp operations
 *
 * Equation format: DEST=EXPRESSION
 * Expression supports:
 *   - Fields: $A, $field_name
 *   - Numbers: 42, 3.14
 *   - Operators: +, -, *, / (with standard precedence)
 *   - Parentheses: (expression)
 *
 * Example: D=($A+$B)*$C
 * Returns operations in execution order (post-order traversal):
 *   1. add(src=$A, operand=$B, dest=_tmp_0)
 *   2. multiply(src=_tmp_0, operand=$C, dest=D)
 *
 * @param equation The equation string to parse
 * @param tmpCounter Optional pointer to temp counter (for parsing multiple equations)
 *                   If provided, will be updated with the next available temp index
 */
std::vector<MathOp> parseEquation(const std::string& equation, int* tmpCounter = nullptr);

/**
 * Extract MathOps from a node graph between two dynamic markers
 *
 * Traverses the graph from beginNodeId to endNodeId, extracting all
 * math operation nodes and their inputs/outputs to reconstruct MathOp structures.
 *
 * @param graph The node graph to traverse
 * @param beginNodeId ID of the dynamic_begin node
 * @param endNodeId ID of the dynamic_end node
 * @return Vector of MathOps in execution order
 */
std::vector<MathOp> extractMathOps(
    const NodeGraph& graph,
    const std::string& beginNodeId,
    const std::string& endNodeId);

/**
 * Reconstruct compact equation strings from MathOps
 *
 * Takes a list of MathOps and generates human-readable equation strings
 * with proper parentheses based on operator precedence.
 *
 * Example: Given ops for D = ($A + $B) * $C
 *   - add(src=A, operand=B, dest=_tmp_0)
 *   - multiply(src=_tmp_0, operand=C, dest=D)
 * Returns: ["D = ($A + $B) * $C"]
 *
 * @param ops Vector of MathOps in execution order
 * @return Vector of equation strings (one per final destination)
 */
std::vector<std::string> reconstructEquations(const std::vector<MathOp>& ops);

/**
 * Tokenizer class for equation parsing
 */
class Tokenizer {
public:
    explicit Tokenizer(const std::string& input);
    Token next();
    Token peek();

private:
    std::string m_input;
    size_t m_pos = 0;

    void skipWhitespace();
    Token readNumber();
    Token readIdentifier();
    Token readField();
};

/**
 * Recursive descent parser for equations
 */
class EquationParser {
public:
    explicit EquationParser(const std::string& equation, int startTmpCounter = 0);

    /**
     * Parse the equation and return the list of operations
     */
    std::vector<MathOp> parse();

    /**
     * Get the current temp counter value (for chaining multiple equations)
     */
    int getTmpCounter() const { return m_tmpCounter; }

private:
    Tokenizer m_tokenizer;
    Token m_currentToken;
    int m_tmpCounter = 0;
    std::vector<MathOp> m_operations;

    void advance();
    void expect(TokenType type);

    // Grammar productions
    std::string parseEquation();  // IDENT '=' expression -> returns dest name
    std::string parseExpression(); // term (('+' | '-') term)*
    std::string parseTerm();       // factor (('*' | '/') factor)*
    std::string parseFactor();     // primary | '-' factor
    std::string parsePrimary();    // FIELD | NUMBER | '(' expression ')'

    std::string newTemp();

    // Returns either a field name (prefixed with $), a tmp name, or a numeric string
    struct Operand {
        std::string name;
        bool isField;
        double numericValue;
    };

    Operand parseOperand();
};

} // namespace nodes
