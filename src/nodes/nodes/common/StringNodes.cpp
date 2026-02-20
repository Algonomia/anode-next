#include "StringNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "dataframe/DataFrame.hpp"
#include "dataframe/Column.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>
#include <regex>

namespace nodes {

void registerStringNodes() {
    registerAddColumnNode();
    registerJsonExtractNode();
    registerTrimNode();
    registerToLowerNode();
    registerToUpperNode();
    registerReplaceNode();
    registerToIntegerNode();
    registerSubstringNode();
    registerSplitNode();
    registerUnidecodeNode();
    registerTrimIntegerNode();
    registerConcatNode();
    registerConcatPrefixNode();
}

// Helper to create simple string transformation nodes
using StringOp = std::function<std::string(const std::string&)>;

static void registerSimpleStringNode(const std::string& name, StringOp op) {
    NodeBuilder(name, "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field})
        .inputOptional("dest", Type::Field)
        .output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([op, name](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }

            // Scalar mode
            if (src.getType() == Type::String) {
                std::string result = op(src.getString());
                ctx.setOutput("result", result);
                return;
            }

            // Vector mode (Field)
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

            std::string destColName = dest.isNull() ? src.getString() : dest.getString();

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string val = src.getStringAtRow(i, header, csv);
                resultCol->push_back(op(val));
            }

            // Create output CSV: clone original + set result column
            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

void registerAddColumnNode() {
    NodeBuilder("add_column", "string")
        .inputOptional("csv", Type::Csv)
        .input("value", {Type::Int, Type::Double, Type::String, Type::Bool, Type::Field})
        .input("dest", Type::Field)
        .output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto value = ctx.getInputWorkload("value");
            auto dest = ctx.getInputWorkload("dest");

            if (value.isNull()) {
                ctx.setError("Input 'value' is not connected");
                return;
            }
            if (dest.isNull()) {
                ctx.setError("Input 'dest' is not connected");
                return;
            }

            auto csv = ctx.getActiveCsv();
            if (!csv) {
                auto csvInput = ctx.getInputWorkload("csv");
                if (!csvInput.isNull()) {
                    csv = csvInput.getCsv();
                }
            }

            if (!csv) {
                ctx.setError("add_column requires a CSV connection");
                return;
            }

            auto header = csv->getColumnNames();
            size_t rowCount = csv->rowCount();
            std::string destColName = dest.getString();

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                resultCol->push_back(value.getStringAtRow(i, header, csv));
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

void registerTrimNode() {
    registerSimpleStringNode("trim", [](const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return std::string();
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    });
}

void registerToLowerNode() {
    registerSimpleStringNode("to_lower", [](const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    });
}

void registerToUpperNode() {
    registerSimpleStringNode("to_upper", [](const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return result;
    });
}

void registerUnidecodeNode() {
    registerSimpleStringNode("unidecode", [](const std::string& s) {
        std::string result;
        result.reserve(s.size());

        // Map of special Unicode characters to ASCII equivalents
        static const std::unordered_map<std::string, std::string> replacements = {
            // Various dashes -> hyphen
            {"\xe2\x80\x90", "-"},  // U+2010 hyphen
            {"\xe2\x80\x91", "-"},  // U+2011 non-breaking hyphen
            {"\xe2\x80\x92", "-"},  // U+2012 figure dash
            {"\xe2\x80\x93", "-"},  // U+2013 en dash
            {"\xe2\x80\x94", "-"},  // U+2014 em dash
            {"\xe2\x80\x95", "-"},  // U+2015 horizontal bar
            // Non-breaking spaces -> regular space
            {"\xc2\xa0", " "},      // U+00A0 non-breaking space
            {"\xe2\x80\xaf", " "},  // U+202F narrow no-break space
        };

        size_t i = 0;
        while (i < s.size()) {
            unsigned char c = s[i];

            // Check for multi-byte UTF-8 sequences
            bool replaced = false;
            if (c >= 0xC0) {
                // Determine UTF-8 sequence length
                size_t seqLen = 1;
                if ((c & 0xE0) == 0xC0) seqLen = 2;
                else if ((c & 0xF0) == 0xE0) seqLen = 3;
                else if ((c & 0xF8) == 0xF0) seqLen = 4;

                if (i + seqLen <= s.size()) {
                    std::string seq = s.substr(i, seqLen);

                    // Check for special replacements
                    auto it = replacements.find(seq);
                    if (it != replacements.end()) {
                        result += it->second;
                        i += seqLen;
                        replaced = true;
                    } else if (seqLen == 2) {
                        // Check for NFD combining diacritical marks
                        // Skip combining characters (U+0300 - U+036F)
                        unsigned int codepoint = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
                        if (codepoint >= 0x0300 && codepoint <= 0x036F) {
                            // Skip combining mark
                            i += seqLen;
                            replaced = true;
                        } else {
                            // Common Latin-1 supplement characters with accents
                            // à-ÿ range (0xC0-0xFF in Latin-1, 0xC380-0xC3BF in UTF-8)
                            if (c == 0xC3) {
                                unsigned char c2 = s[i + 1];
                                char replacement = 0;

                                // À-Ö (uppercase accented)
                                if (c2 >= 0x80 && c2 <= 0x85) replacement = 'A';
                                else if (c2 == 0x86) replacement = 'A'; // Æ -> A (or AE)
                                else if (c2 == 0x87) replacement = 'C'; // Ç
                                else if (c2 >= 0x88 && c2 <= 0x8B) replacement = 'E';
                                else if (c2 >= 0x8C && c2 <= 0x8F) replacement = 'I';
                                else if (c2 == 0x90) replacement = 'D'; // Ð
                                else if (c2 == 0x91) replacement = 'N'; // Ñ
                                else if (c2 >= 0x92 && c2 <= 0x96) replacement = 'O';
                                else if (c2 == 0x98) replacement = 'O'; // Ø
                                else if (c2 >= 0x99 && c2 <= 0x9C) replacement = 'U';
                                else if (c2 == 0x9D) replacement = 'Y'; // Ý
                                else if (c2 == 0x9E) replacement = 'T'; // Þ
                                else if (c2 == 0x9F) replacement = 's'; // ß -> ss (but single char here)
                                // à-ö (lowercase accented)
                                else if (c2 >= 0xA0 && c2 <= 0xA5) replacement = 'a';
                                else if (c2 == 0xA6) replacement = 'a'; // æ -> a
                                else if (c2 == 0xA7) replacement = 'c'; // ç
                                else if (c2 >= 0xA8 && c2 <= 0xAB) replacement = 'e';
                                else if (c2 >= 0xAC && c2 <= 0xAF) replacement = 'i';
                                else if (c2 == 0xB0) replacement = 'd'; // ð
                                else if (c2 == 0xB1) replacement = 'n'; // ñ
                                else if (c2 >= 0xB2 && c2 <= 0xB6) replacement = 'o';
                                else if (c2 == 0xB8) replacement = 'o'; // ø
                                else if (c2 >= 0xB9 && c2 <= 0xBC) replacement = 'u';
                                else if (c2 == 0xBD) replacement = 'y'; // ý
                                else if (c2 == 0xBE) replacement = 't'; // þ
                                else if (c2 == 0xBF) replacement = 'y'; // ÿ

                                if (replacement) {
                                    result += replacement;
                                    i += 2;
                                    replaced = true;
                                }
                            }
                        }
                    }
                }
            }

            if (!replaced) {
                // ASCII character or unhandled UTF-8
                result += s[i];
                ++i;
            }
        }

        return result;
    });
}

void registerTrimIntegerNode() {
    registerSimpleStringNode("trim_integer", [](const std::string& s) {
        // First trim whitespace
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return std::string();
        auto end = s.find_last_not_of(" \t\n\r");
        std::string trimmed = s.substr(start, end - start + 1);

        // Check if it's a positive integer (only digits)
        bool allDigits = !trimmed.empty() &&
            std::all_of(trimmed.begin(), trimmed.end(), [](unsigned char c) {
                return std::isdigit(c);
            });

        if (allDigits) {
            // Convert to integer to remove leading zeros, then back to string
            try {
                long long val = std::stoll(trimmed);
                return std::to_string(val);
            } catch (...) {
                return trimmed;
            }
        }

        return trimmed;
    });
}

void registerReplaceNode() {
    NodeBuilder("replace", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field})
        .inputOptional("dest", Type::Field)
        .input("search", {Type::String, Type::Int})
        .input("by", {Type::String, Type::Int})
        .output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");
            auto search = ctx.getInputWorkload("search");
            auto by = ctx.getInputWorkload("by");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }
            if (search.isNull()) {
                ctx.setError("Input 'search' is not connected");
                return;
            }
            if (by.isNull()) {
                ctx.setError("Input 'by' is not connected");
                return;
            }

            // Get search and by as strings
            std::string searchStr = (search.getType() == Type::Int)
                ? std::to_string(search.getInt())
                : search.getString();
            std::string byStr = (by.getType() == Type::Int)
                ? std::to_string(by.getInt())
                : by.getString();

            auto replaceFirst = [&searchStr, &byStr](const std::string& s) {
                std::string result = s;
                size_t pos = result.find(searchStr);
                if (pos != std::string::npos) {
                    result.replace(pos, searchStr.length(), byStr);
                }
                return result;
            };

            // Scalar mode
            if (src.getType() == Type::String) {
                std::string result = replaceFirst(src.getString());
                ctx.setOutput("result", result);
                return;
            }

            // Vector mode
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

            std::string destColName = dest.isNull() ? src.getString() : dest.getString();

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string val = src.getStringAtRow(i, header, csv);
                resultCol->push_back(replaceFirst(val));
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

void registerToIntegerNode() {
    NodeBuilder("to_integer", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field})
        .inputOptional("dest", Type::Field)
        .inputOptional("default_value", Type::Int)
        .output("csv", Type::Csv)
        .output("result", Type::Int)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");
            auto defaultValue = ctx.getInputWorkload("default_value");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }

            bool hasDefault = !defaultValue.isNull();
            int64_t defaultVal = hasDefault ? defaultValue.getInt() : 0;

            auto toInt = [hasDefault, defaultVal](const std::string& s) -> int64_t {
                if (s.empty()) {
                    if (hasDefault) return defaultVal;
                    throw std::runtime_error("Empty string cannot be converted to integer");
                }
                try {
                    return std::stoll(s);
                } catch (...) {
                    if (hasDefault) return defaultVal;
                    throw std::runtime_error("Cannot convert '" + s + "' to integer");
                }
            };

            // Scalar mode
            if (src.getType() == Type::String) {
                try {
                    int64_t result = toInt(src.getString());
                    ctx.setOutput("result", result);
                } catch (const std::exception& e) {
                    ctx.setError(e.what());
                }
                return;
            }

            // Vector mode
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

            std::string destColName = dest.isNull() ? src.getString() : dest.getString();

            auto resultCol = std::make_shared<dataframe::IntColumn>(destColName);
            resultCol->reserve(rowCount);

            try {
                for (size_t i = 0; i < rowCount; ++i) {
                    std::string val = src.getStringAtRow(i, header, csv);
                    resultCol->push_back(static_cast<int>(toInt(val)));
                }
            } catch (const std::exception& e) {
                ctx.setError(e.what());
                return;
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", static_cast<int64_t>(resultCol->at(0)));
            }
        })
        .buildAndRegister();
}

void registerSubstringNode() {
    NodeBuilder("substring", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field})
        .inputOptional("dest", Type::Field)
        .inputOptional("begin", {Type::Int, Type::Field})
        .inputOptional("end", {Type::Int, Type::Field})
        .output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");
            auto beginWork = ctx.getInputWorkload("begin");
            auto endWork = ctx.getInputWorkload("end");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }

            // Scalar mode
            if (src.getType() == Type::String) {
                std::string s = src.getString();
                int64_t beginPos = beginWork.isNull() ? 0 : beginWork.getInt();
                int64_t endPos = endWork.isNull() ? static_cast<int64_t>(s.length()) : endWork.getInt();

                if (beginPos < 0) beginPos = 0;
                if (endPos > static_cast<int64_t>(s.length())) endPos = s.length();
                if (beginPos >= endPos) {
                    ctx.setOutput("result", std::string());
                    return;
                }

                ctx.setOutput("result", s.substr(beginPos, endPos - beginPos));
                return;
            }

            // Vector mode
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

            std::string destColName = dest.isNull() ? src.getString() : dest.getString();

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string s = src.getStringAtRow(i, header, csv);

                int64_t beginPos = beginWork.isNull() ? 0 : beginWork.getIntAtRow(i, header, csv);
                int64_t endPos = endWork.isNull() ? static_cast<int64_t>(s.length()) : endWork.getIntAtRow(i, header, csv);

                if (beginPos < 0) beginPos = 0;
                if (endPos > static_cast<int64_t>(s.length())) endPos = s.length();
                if (beginPos >= endPos) {
                    resultCol->push_back("");
                } else {
                    resultCol->push_back(s.substr(beginPos, endPos - beginPos));
                }
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

void registerSplitNode() {
    NodeBuilder("split", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field})
        .inputOptional("dest", Type::Field)
        .input("char", Type::String)
        .input("pos", Type::Int)
        .output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");
            auto charInput = ctx.getInputWorkload("char");
            auto posInput = ctx.getInputWorkload("pos");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }
            if (charInput.isNull()) {
                ctx.setError("Input 'char' is not connected");
                return;
            }
            if (posInput.isNull()) {
                ctx.setError("Input 'pos' is not connected");
                return;
            }

            std::string delimiter = charInput.getString();
            int64_t pos = posInput.getInt();

            auto splitAndGet = [&delimiter, pos](const std::string& s) -> std::string {
                std::vector<std::string> tokens;
                size_t start = 0;
                size_t end;

                while ((end = s.find(delimiter, start)) != std::string::npos) {
                    tokens.push_back(s.substr(start, end - start));
                    start = end + delimiter.length();
                }
                tokens.push_back(s.substr(start));

                if (pos >= 0 && static_cast<size_t>(pos) < tokens.size()) {
                    return tokens[pos];
                }
                return "";
            };

            // Scalar mode
            if (src.getType() == Type::String) {
                ctx.setOutput("result", splitAndGet(src.getString()));
                return;
            }

            // Vector mode
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

            std::string destColName = dest.isNull() ? src.getString() : dest.getString();

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string val = src.getStringAtRow(i, header, csv);
                resultCol->push_back(splitAndGet(val));
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}


void registerConcatNode() {
    auto builder = NodeBuilder("concat", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field, Type::Int, Type::Double})
        .inputOptional("dest", Type::Field)
        .input("suffix", {Type::String, Type::Field, Type::Int, Type::Double});

    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("suffix_" + std::to_string(i), {Type::String, Type::Field, Type::Int, Type::Double});
    }

    builder.output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }

            // Collect all suffixes
            std::vector<Workload> suffixes;
            auto suffix = ctx.getInputWorkload("suffix");
            if (!suffix.isNull()) {
                suffixes.push_back(suffix);
            }

            for (int i = 1; i <= 99; ++i) {
                auto suffixI = ctx.getInputWorkload("suffix_" + std::to_string(i));
                if (suffixI.isNull()) break;
                suffixes.push_back(suffixI);
            }

            auto getStringValue = [](const Workload& w, size_t row,
                                     const std::vector<std::string>& header,
                                     const std::shared_ptr<dataframe::DataFrame>& csv) -> std::string {
                if (w.getType() == Type::Int) {
                    return std::to_string(w.getIntAtRow(row, header, csv));
                } else if (w.getType() == Type::Double) {
                    return std::to_string(w.getDoubleAtRow(row, header, csv));
                } else {
                    return w.getStringAtRow(row, header, csv);
                }
            };

            // Check if any input is a field (vector mode needed)
            bool needsVector = (src.getType() == Type::Field);
            for (const auto& s : suffixes) {
                if (s.getType() == Type::Field) {
                    needsVector = true;
                    break;
                }
            }

            if (!needsVector) {
                // Scalar mode
                std::string result;
                if (src.getType() == Type::Int) {
                    result = std::to_string(src.getInt());
                } else if (src.getType() == Type::Double) {
                    result = std::to_string(src.getDouble());
                } else {
                    result = src.getString();
                }

                for (const auto& s : suffixes) {
                    if (s.getType() == Type::Int) {
                        result += std::to_string(s.getInt());
                    } else if (s.getType() == Type::Double) {
                        result += std::to_string(s.getDouble());
                    } else {
                        result += s.getString();
                    }
                }

                ctx.setOutput("result", result);
                return;
            }

            // Vector mode
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

            std::string destColName;
            if (!dest.isNull()) {
                destColName = dest.getString();
            } else if (src.getType() == Type::Field) {
                destColName = src.getString();
            } else {
                destColName = "_concat_result";
            }

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string result = getStringValue(src, i, header, csv);
                for (const auto& s : suffixes) {
                    result += getStringValue(s, i, header, csv);
                }
                resultCol->push_back(result);
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

void registerConcatPrefixNode() {
    auto builder = NodeBuilder("concat_prefix", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field, Type::Int, Type::Double})
        .inputOptional("dest", Type::Field)
        .input("prefix", {Type::String, Type::Field, Type::Int, Type::Double});

    for (int i = 1; i <= 99; i++) {
        builder.inputOptional("prefix_" + std::to_string(i), {Type::String, Type::Field, Type::Int, Type::Double});
    }

    builder.output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto dest = ctx.getInputWorkload("dest");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }

            // Collect all prefixes
            std::vector<Workload> prefixes;
            auto prefix = ctx.getInputWorkload("prefix");
            if (!prefix.isNull()) {
                prefixes.push_back(prefix);
            }

            for (int i = 1; i <= 99; ++i) {
                auto prefixI = ctx.getInputWorkload("prefix_" + std::to_string(i));
                if (prefixI.isNull()) break;
                prefixes.push_back(prefixI);
            }

            auto getStringValue = [](const Workload& w, size_t row,
                                     const std::vector<std::string>& header,
                                     const std::shared_ptr<dataframe::DataFrame>& csv) -> std::string {
                if (w.getType() == Type::Int) {
                    return std::to_string(w.getIntAtRow(row, header, csv));
                } else if (w.getType() == Type::Double) {
                    return std::to_string(w.getDoubleAtRow(row, header, csv));
                } else {
                    return w.getStringAtRow(row, header, csv);
                }
            };

            // Check if any input is a field (vector mode needed)
            bool needsVector = (src.getType() == Type::Field);
            for (const auto& p : prefixes) {
                if (p.getType() == Type::Field) {
                    needsVector = true;
                    break;
                }
            }

            if (!needsVector) {
                // Scalar mode - prefixes are prepended in order
                std::string result;

                // Add all prefixes first
                for (const auto& p : prefixes) {
                    if (p.getType() == Type::Int) {
                        result += std::to_string(p.getInt());
                    } else if (p.getType() == Type::Double) {
                        result += std::to_string(p.getDouble());
                    } else {
                        result += p.getString();
                    }
                }

                // Then add src
                if (src.getType() == Type::Int) {
                    result += std::to_string(src.getInt());
                } else if (src.getType() == Type::Double) {
                    result += std::to_string(src.getDouble());
                } else {
                    result += src.getString();
                }

                ctx.setOutput("result", result);
                return;
            }

            // Vector mode
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

            std::string destColName;
            if (!dest.isNull()) {
                destColName = dest.getString();
            } else if (src.getType() == Type::Field) {
                destColName = src.getString();
            } else {
                destColName = "_concat_prefix_result";
            }

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string result;

                // Add all prefixes first
                for (const auto& p : prefixes) {
                    result += getStringValue(p, i, header, csv);
                }

                // Then add src
                result += getStringValue(src, i, header, csv);

                resultCol->push_back(result);
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

// Simple JSON key extraction: finds "key":"value" in a JSON string
static std::string extractJsonValue(const std::string& json, const std::string& key) {
    // Search for "key"
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return std::string();

    // Skip past "key" and find the colon
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size() || json[pos] != ':') return std::string();
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return std::string();

    // Extract value
    if (json[pos] == '"') {
        // String value — find closing quote (handle escaped quotes)
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos; // skip escape char
            }
            result += json[pos];
            ++pos;
        }
        return result;
    }

    // Non-string value (number, bool, null) — read until delimiter
    auto end = json.find_first_of(",} \t\n\r", pos);
    if (end == std::string::npos) end = json.size();
    return json.substr(pos, end - pos);
}

void registerJsonExtractNode() {
    NodeBuilder("json_extract", "string")
        .inputOptional("csv", Type::Csv)
        .input("src", {Type::String, Type::Field})
        .input("key", {Type::String, Type::Field})
        .inputOptional("dest", Type::Field)
        .output("csv", Type::Csv)
        .output("result", Type::String)
        .onCompile([](NodeContext& ctx) {
            auto src = ctx.getInputWorkload("src");
            auto key = ctx.getInputWorkload("key");
            auto dest = ctx.getInputWorkload("dest");

            if (src.isNull()) {
                ctx.setError("Input 'src' is not connected");
                return;
            }
            if (key.isNull()) {
                ctx.setError("Input 'key' is not connected");
                return;
            }

            // Read failure mode from widget property
            auto onFailureProp = ctx.getInputWorkload("_on_failure");
            bool identityOnFailure = true; // default: identity
            if (!onFailureProp.isNull() && onFailureProp.getString() == "blank") {
                identityOnFailure = false;
            }

            // Scalar mode
            if (src.getType() == Type::String && key.getType() == Type::String) {
                std::string jsonStr = src.getString();
                std::string keyStr = key.getString();
                std::string extracted = extractJsonValue(jsonStr, keyStr);
                if (extracted.empty() && identityOnFailure) {
                    ctx.setOutput("result", jsonStr);
                } else {
                    ctx.setOutput("result", extracted);
                }
                return;
            }

            // Vector mode
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

            std::string destColName = dest.isNull() ? src.getString() : dest.getString();

            auto resultCol = std::make_shared<dataframe::StringColumn>(
                destColName, csv->getStringPool());
            resultCol->reserve(rowCount);

            for (size_t i = 0; i < rowCount; ++i) {
                std::string jsonStr = src.getStringAtRow(i, header, csv);
                std::string keyStr = key.getStringAtRow(i, header, csv);
                std::string extracted = extractJsonValue(jsonStr, keyStr);
                if (extracted.empty() && identityOnFailure) {
                    resultCol->push_back(jsonStr);
                } else {
                    resultCol->push_back(extracted);
                }
            }

            auto resultCsv = std::make_shared<dataframe::DataFrame>();
            resultCsv->setStringPool(csv->getStringPool());

            for (const auto& colName : header) {
                if (colName != destColName) {
                    resultCsv->addColumn(csv->getColumn(colName)->clone());
                }
            }
            resultCsv->setColumn(resultCol);

            ctx.setOutput("csv", resultCsv);

            if (rowCount > 0) {
                ctx.setOutput("result", resultCol->at(0));
            }
        })
        .buildAndRegister();
}

} // namespace nodes
