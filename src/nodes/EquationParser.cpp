#include "nodes/EquationParser.hpp"
#include "nodes/NodeExecutor.hpp"  // For NodeGraph
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>

namespace nodes {

// Helper to trim whitespace from both ends of a string
static std::string trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(str[start])) ++start;
    size_t end = str.size();
    while (end > start && std::isspace(str[end - 1])) --end;
    return str.substr(start, end - start);
}

// Check if character is an operator or delimiter that ends a name
static bool isOperatorOrDelimiter(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' ||
           c == '(' || c == ')' || c == '=' || c == '$';
}

// ============================================================================
// Tokenizer
// ============================================================================

Tokenizer::Tokenizer(const std::string& input) : m_input(input) {}

void Tokenizer::skipWhitespace() {
    while (m_pos < m_input.size() && std::isspace(m_input[m_pos])) {
        ++m_pos;
    }
}

Token Tokenizer::readNumber() {
    size_t start = m_pos;
    bool hasDecimal = false;

    // Handle negative sign
    if (m_pos < m_input.size() && m_input[m_pos] == '-') {
        ++m_pos;
    }

    while (m_pos < m_input.size()) {
        char c = m_input[m_pos];
        if (std::isdigit(c)) {
            ++m_pos;
        } else if (c == '.' && !hasDecimal) {
            hasDecimal = true;
            ++m_pos;
        } else {
            break;
        }
    }

    std::string numStr = m_input.substr(start, m_pos - start);
    Token tok;
    tok.type = TokenType::NUMBER;
    tok.stringValue = numStr;
    tok.numericValue = std::stod(numStr);
    return tok;
}

Token Tokenizer::readIdentifier() {
    // Read until '=' to support spaces in destination names
    // e.g., "Do do = $A + $B" -> identifier is "Do do"
    size_t start = m_pos;
    while (m_pos < m_input.size() && m_input[m_pos] != '=') {
        ++m_pos;
    }
    Token tok;
    tok.type = TokenType::IDENT;
    tok.stringValue = trim(m_input.substr(start, m_pos - start));
    return tok;
}

Token Tokenizer::readField() {
    // Skip the $ sign
    ++m_pos;
    size_t start = m_pos;
    // Read until operator or delimiter to support spaces in field names
    // e.g., "$Integration rate * 2" -> field is "Integration rate"
    while (m_pos < m_input.size() && !isOperatorOrDelimiter(m_input[m_pos])) {
        ++m_pos;
    }
    Token tok;
    tok.type = TokenType::FIELD;
    tok.stringValue = trim(m_input.substr(start, m_pos - start));
    return tok;
}

Token Tokenizer::next() {
    skipWhitespace();

    if (m_pos >= m_input.size()) {
        return Token{TokenType::END, "", 0.0};
    }

    char c = m_input[m_pos];

    // Single character tokens
    switch (c) {
        case '+': ++m_pos; return Token{TokenType::PLUS, "+", 0.0};
        case '-':
            // Check if this is a negative number or the minus operator
            if (m_pos + 1 < m_input.size() && std::isdigit(m_input[m_pos + 1])) {
                // Could be a negative number, but only if previous token allows it
                // For simplicity, treat as operator here; unary minus handled in parseFactor
            }
            ++m_pos;
            return Token{TokenType::MINUS, "-", 0.0};
        case '*': ++m_pos; return Token{TokenType::STAR, "*", 0.0};
        case '/': ++m_pos; return Token{TokenType::SLASH, "/", 0.0};
        case '(': ++m_pos; return Token{TokenType::LPAREN, "(", 0.0};
        case ')': ++m_pos; return Token{TokenType::RPAREN, ")", 0.0};
        case '=': ++m_pos; return Token{TokenType::EQ, "=", 0.0};
        case '$': return readField();
    }

    // Numbers
    if (std::isdigit(c)) {
        return readNumber();
    }

    // Identifiers
    if (std::isalpha(c) || c == '_') {
        return readIdentifier();
    }

    throw std::runtime_error("Unexpected character in equation: " + std::string(1, c));
}

Token Tokenizer::peek() {
    size_t savedPos = m_pos;
    Token tok = next();
    m_pos = savedPos;
    return tok;
}

// ============================================================================
// EquationParser
// ============================================================================

EquationParser::EquationParser(const std::string& equation, int startTmpCounter)
    : m_tokenizer(equation), m_tmpCounter(startTmpCounter)
{
    advance();
}

void EquationParser::advance() {
    m_currentToken = m_tokenizer.next();
}

void EquationParser::expect(TokenType type) {
    if (m_currentToken.type != type) {
        throw std::runtime_error("Unexpected token in equation");
    }
    advance();
}

std::string EquationParser::newTemp() {
    return "_tmp_" + std::to_string(m_tmpCounter++);
}

std::vector<MathOp> EquationParser::parse() {
    parseEquation();
    return m_operations;
}

std::string EquationParser::parseEquation() {
    // IDENT '=' expression
    if (m_currentToken.type != TokenType::IDENT) {
        throw std::runtime_error("Expected destination identifier at start of equation");
    }
    std::string destName = m_currentToken.stringValue;
    advance();

    expect(TokenType::EQ);

    std::string resultName = parseExpression();

    // If the result is already a field or tmp, rename it to dest
    // Otherwise, the last operation should already have dest set
    if (!m_operations.empty()) {
        m_operations.back().dest = destName;
    }

    return destName;
}

std::string EquationParser::parseExpression() {
    // term (('+' | '-') term)*
    std::string left = parseTerm();

    while (m_currentToken.type == TokenType::PLUS ||
           m_currentToken.type == TokenType::MINUS) {
        std::string op = (m_currentToken.type == TokenType::PLUS) ? "add" : "subtract";
        advance();
        std::string right = parseTerm();

        // Create operation: left op right -> tmp
        MathOp mathOp;
        mathOp.op = op;

        // Determine if left is a field (starts with $), a tmp (starts with _tmp_), or scalar
        if (left[0] == '$') {
            mathOp.src = left.substr(1);  // Remove $ prefix
            mathOp.srcIsField = true;
        } else if (left.substr(0, 5) == "_tmp_") {
            mathOp.src = left;
            mathOp.srcIsField = false;  // It's a temporary
        } else {
            // Scalar - this shouldn't happen for src, but handle it
            mathOp.src = left;
            mathOp.srcIsField = false;
        }

        // Determine if right is a field, tmp, or scalar
        if (right[0] == '$') {
            mathOp.operand = right.substr(1);  // Remove $ prefix
            mathOp.operandIsField = true;
            mathOp.operandValue = 0.0;
        } else if (right.substr(0, 5) == "_tmp_") {
            mathOp.operand = right;
            mathOp.operandIsField = false;
            mathOp.operandValue = 0.0;
        } else {
            // Scalar value
            mathOp.operand = right;
            mathOp.operandIsField = false;
            mathOp.operandValue = std::stod(right);
        }

        std::string dest = newTemp();
        mathOp.dest = dest;

        m_operations.push_back(mathOp);
        left = dest;
    }

    return left;
}

std::string EquationParser::parseTerm() {
    // factor (('*' | '/') factor)*
    std::string left = parseFactor();

    while (m_currentToken.type == TokenType::STAR ||
           m_currentToken.type == TokenType::SLASH) {
        std::string op = (m_currentToken.type == TokenType::STAR) ? "multiply" : "divide";
        advance();
        std::string right = parseFactor();

        // Create operation
        MathOp mathOp;
        mathOp.op = op;

        if (left[0] == '$') {
            mathOp.src = left.substr(1);
            mathOp.srcIsField = true;
        } else if (left.substr(0, 5) == "_tmp_") {
            mathOp.src = left;
            mathOp.srcIsField = false;
        } else {
            mathOp.src = left;
            mathOp.srcIsField = false;
        }

        if (right[0] == '$') {
            mathOp.operand = right.substr(1);
            mathOp.operandIsField = true;
            mathOp.operandValue = 0.0;
        } else if (right.substr(0, 5) == "_tmp_") {
            mathOp.operand = right;
            mathOp.operandIsField = false;
            mathOp.operandValue = 0.0;
        } else {
            mathOp.operand = right;
            mathOp.operandIsField = false;
            mathOp.operandValue = std::stod(right);
        }

        std::string dest = newTemp();
        mathOp.dest = dest;

        m_operations.push_back(mathOp);
        left = dest;
    }

    return left;
}

std::string EquationParser::parseFactor() {
    // primary | '-' factor (unary minus)
    if (m_currentToken.type == TokenType::MINUS) {
        advance();
        std::string operand = parseFactor();

        // Create: 0 - operand
        MathOp mathOp;
        mathOp.op = "subtract";
        mathOp.src = "0";
        mathOp.srcIsField = false;

        if (operand[0] == '$') {
            mathOp.operand = operand.substr(1);
            mathOp.operandIsField = true;
            mathOp.operandValue = 0.0;
        } else if (operand.substr(0, 5) == "_tmp_") {
            mathOp.operand = operand;
            mathOp.operandIsField = false;
            mathOp.operandValue = 0.0;
        } else {
            mathOp.operand = operand;
            mathOp.operandIsField = false;
            mathOp.operandValue = std::stod(operand);
        }

        std::string dest = newTemp();
        mathOp.dest = dest;
        m_operations.push_back(mathOp);
        return dest;
    }

    return parsePrimary();
}

std::string EquationParser::parsePrimary() {
    // FIELD | NUMBER | '(' expression ')'

    if (m_currentToken.type == TokenType::FIELD) {
        std::string fieldName = "$" + m_currentToken.stringValue;
        advance();
        return fieldName;
    }

    if (m_currentToken.type == TokenType::NUMBER) {
        std::string numStr = m_currentToken.stringValue;
        advance();
        return numStr;
    }

    if (m_currentToken.type == TokenType::LPAREN) {
        advance();  // consume '('
        std::string result = parseExpression();
        expect(TokenType::RPAREN);  // consume ')'
        return result;
    }

    throw std::runtime_error("Unexpected token in equation: expected field, number, or parenthesis");
}

// ============================================================================
// Public API
// ============================================================================

std::vector<MathOp> parseEquation(const std::string& equation, int* tmpCounter) {
    int startCounter = tmpCounter ? *tmpCounter : 0;
    EquationParser parser(equation, startCounter);
    auto ops = parser.parse();
    if (tmpCounter) {
        *tmpCounter = parser.getTmpCounter();
    }
    return ops;
}

// ============================================================================
// Extract MathOps from Graph
// ============================================================================

std::vector<MathOp> extractMathOps(
    const NodeGraph& graph,
    const std::string& beginNodeId,
    const std::string& endNodeId)
{
    std::vector<MathOp> result;

    // Build a map of node outputs for quick lookup
    // We need to follow the CSV chain from begin to end
    std::vector<std::string> nodeOrder;
    std::unordered_set<std::string> visited;

    // Start from begin node, follow CSV connections to end
    std::string currentNodeId = beginNodeId;

    while (currentNodeId != endNodeId && !currentNodeId.empty()) {
        if (visited.count(currentNodeId)) break;  // Cycle detection
        visited.insert(currentNodeId);

        // Find the next node connected via CSV output
        std::string nextNodeId;
        for (const auto& conn : graph.getConnections()) {
            if (conn.sourceNodeId == currentNodeId && conn.sourcePortName == "csv") {
                nextNodeId = conn.targetNodeId;
                break;
            }
        }

        if (nextNodeId.empty()) break;

        // If it's a math node, extract it
        auto node = graph.getNode(nextNodeId);
        if (node && node->definitionName.substr(0, 5) == "math/") {
            nodeOrder.push_back(nextNodeId);
        }

        currentNodeId = nextNodeId;
    }

    // Now extract MathOps from each math node
    for (const auto& nodeId : nodeOrder) {
        auto node = graph.getNode(nodeId);
        if (!node) continue;

        MathOp op;

        // Determine operation type from definition name
        std::string defName = node->definitionName;
        if (defName == "math/add") op.op = "add";
        else if (defName == "math/subtract") op.op = "subtract";
        else if (defName == "math/multiply") op.op = "multiply";
        else if (defName == "math/divide") op.op = "divide";
        else continue;  // Unknown math node

        // Find connections to src, operand, dest ports
        for (const auto& conn : graph.getConnections()) {
            if (conn.targetNodeId != nodeId) continue;

            // Get the source node to extract its _value property
            auto srcNode = graph.getNode(conn.sourceNodeId);
            if (!srcNode) continue;

            // Get the _value property from the source node
            auto it = srcNode->properties.find("_value");

            if (conn.targetPortName == "src") {
                if (srcNode->definitionName == "scalar/string_as_field") {
                    op.srcIsField = true;
                    if (it != srcNode->properties.end() && it->second.getType() == NodeType::String) {
                        op.src = it->second.getString();
                    }
                } else if (srcNode->definitionName == "scalar/double_value") {
                    op.srcIsField = false;
                    if (it != srcNode->properties.end()) {
                        double val = 0.0;
                        if (it->second.getType() == NodeType::Double) {
                            val = it->second.getDouble();
                        } else if (it->second.getType() == NodeType::Int) {
                            val = static_cast<double>(it->second.getInt());
                        }
                        std::ostringstream oss;
                        if (val == static_cast<int>(val)) {
                            oss << static_cast<int>(val);
                        } else {
                            oss << val;
                        }
                        op.src = oss.str();
                    }
                }
            } else if (conn.targetPortName == "operand") {
                if (srcNode->definitionName == "scalar/string_as_field") {
                    op.operandIsField = true;
                    if (it != srcNode->properties.end() && it->second.getType() == NodeType::String) {
                        op.operand = it->second.getString();
                    }
                } else if (srcNode->definitionName == "scalar/double_value") {
                    op.operandIsField = false;
                    if (it != srcNode->properties.end()) {
                        if (it->second.getType() == NodeType::Double) {
                            op.operandValue = it->second.getDouble();
                        } else if (it->second.getType() == NodeType::Int) {
                            op.operandValue = static_cast<double>(it->second.getInt());
                        }
                        std::ostringstream oss;
                        if (op.operandValue == static_cast<int>(op.operandValue)) {
                            oss << static_cast<int>(op.operandValue);
                        } else {
                            oss << op.operandValue;
                        }
                        op.operand = oss.str();
                    }
                }
            } else if (conn.targetPortName == "dest") {
                if (srcNode->definitionName == "scalar/string_as_field") {
                    if (it != srcNode->properties.end() && it->second.getType() == NodeType::String) {
                        op.dest = it->second.getString();
                    }
                }
            }
        }

        result.push_back(op);
    }

    return result;
}

// ============================================================================
// Reconstruct Equations from MathOps
// ============================================================================

namespace {

// Helper struct for building expressions with precedence info
struct ExprResult {
    std::string text;
    int precedence;  // 1 = add/sub, 2 = mul/div, 3 = atom
};

// Get precedence level for an operation
int getOpPrecedence(const std::string& op) {
    if (op == "multiply" || op == "divide") return 2;
    if (op == "add" || op == "subtract") return 1;
    return 3;  // Atom
}

// Get operator symbol for an operation
std::string getOpSymbol(const std::string& op) {
    if (op == "add") return " + ";
    if (op == "subtract") return " - ";
    if (op == "multiply") return " * ";
    if (op == "divide") return " / ";
    return "?";
}

// Check if a name is a temporary variable
bool isTemp(const std::string& name) {
    return name.size() >= 5 && name.substr(0, 5) == "_tmp_";
}

// Format a value as an expression (field reference or scalar)
std::string formatValue(const std::string& name, bool isField) {
    if (isField) {
        return "$" + name;
    }
    return name;  // Scalar value as-is
}

// Build expression recursively
ExprResult buildExpression(
    const std::string& name,
    bool isField,
    const std::unordered_map<std::string, const MathOp*>& destMap)
{
    // Check if this is a temporary that we need to inline
    if (isTemp(name)) {
        auto it = destMap.find(name);
        if (it != destMap.end()) {
            const MathOp* op = it->second;
            int opPrec = getOpPrecedence(op->op);

            // Build left and right expressions
            ExprResult left = buildExpression(op->src, op->srcIsField, destMap);
            ExprResult right = buildExpression(op->operand, op->operandIsField, destMap);

            // Add parentheses if needed based on precedence
            std::string leftText = left.text;
            std::string rightText = right.text;

            // Left operand needs parens if lower precedence
            if (left.precedence < opPrec) {
                leftText = "(" + leftText + ")";
            }

            // Right operand needs parens if lower precedence
            // Also need parens for right operand of subtract/divide with same precedence
            if (right.precedence < opPrec ||
                (right.precedence == opPrec && (op->op == "subtract" || op->op == "divide"))) {
                rightText = "(" + rightText + ")";
            }

            return {leftText + getOpSymbol(op->op) + rightText, opPrec};
        }
    }

    // Not a temp or not found in destMap - format as atom
    return {formatValue(name, isField), 3};
}

}  // anonymous namespace

std::vector<std::string> reconstructEquations(const std::vector<MathOp>& ops) {
    if (ops.empty()) {
        return {};
    }

    // Build dest -> MathOp* map for quick lookup
    std::unordered_map<std::string, const MathOp*> destMap;
    for (const auto& op : ops) {
        destMap[op.dest] = &op;
    }

    // Find final ops (dest not used as input by any other op)
    std::unordered_set<std::string> usedAsInput;
    for (const auto& op : ops) {
        if (isTemp(op.src)) {
            usedAsInput.insert(op.src);
        }
        if (isTemp(op.operand)) {
            usedAsInput.insert(op.operand);
        }
    }

    std::vector<const MathOp*> finalOps;
    for (const auto& op : ops) {
        if (usedAsInput.find(op.dest) == usedAsInput.end()) {
            finalOps.push_back(&op);
        }
    }

    // Build equations for each final op
    std::vector<std::string> equations;
    for (const MathOp* op : finalOps) {
        // Build left and right expressions
        ExprResult left = buildExpression(op->src, op->srcIsField, destMap);
        ExprResult right = buildExpression(op->operand, op->operandIsField, destMap);

        int opPrec = getOpPrecedence(op->op);

        std::string leftText = left.text;
        std::string rightText = right.text;

        // Add parentheses based on precedence
        if (left.precedence < opPrec) {
            leftText = "(" + leftText + ")";
        }
        if (right.precedence < opPrec ||
            (right.precedence == opPrec && (op->op == "subtract" || op->op == "divide"))) {
            rightText = "(" + rightText + ")";
        }

        std::string equation = op->dest + " = " + leftText + getOpSymbol(op->op) + rightText;
        equations.push_back(equation);
    }

    return equations;
}

} // namespace nodes
