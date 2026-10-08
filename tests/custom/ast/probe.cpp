// Test-only driver: exercise each parser entry and inspect the actual AST fields.
#include "RxLexer.h"
#include "ast/builder.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace rx::AST;
using Fields = std::vector<std::pair<std::string, std::string>>;

std::string quote(const std::string& value) {
    std::string result = "\"";
    const char* digits = "0123456789abcdef";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { result += '\\'; result += c; }
        else if (c < 32) {
            result += "\\u00";
            result += digits[c >> 4]; result += digits[c & 15];
        } else result += c;
    }
    return result + '"';
}

std::string array(const std::vector<std::string>& values) {
    std::string result = "[";
    for (const auto& value : values) {
        if (result.size() > 1) result += ',';
        result += value;
    }
    return result + ']';
}

std::string object(const Fields& fields) {
    std::string result = "{";
    for (const auto& field : fields) {
        if (result.size() > 1) result += ',';
        result += quote(field.first) + ':' + field.second;
    }
    return result + '}';
}

struct InspectAST {
    std::set<std::size_t> ids;
    std::set<std::string> kinds;

    template<class T>
    std::string required(const std::shared_ptr<T>& ptr) {
        if (!ptr) throw std::runtime_error("required AST child is null");
        return node(*ptr);
    }
    template<class T>
    std::string optional(const std::shared_ptr<T>& ptr) {
        return ptr ? node(*ptr) : "null";
    }
    template<class T>
    std::string nodes(const std::vector<std::shared_ptr<T>>& values) {
        std::vector<std::string> result;
        for (const auto& value : values) result.push_back(required(value));
        return array(result);
    }
    std::string segment(const PathSegment& value) {
        return object({{"kind", quote(pathsegmentkind_to_string(value.kind))},
                       {"name", quote(value.name)}, {"args", nodes(value.args)}});
    }
    std::string path(const Path& value) {
        if (value.empty()) throw std::runtime_error("AST path is empty");
        std::vector<std::string> result;
        for (const auto& part : value) result.push_back(segment(part));
        return array(result);
    }

    std::string node(const ASTnode& value) {
        if (!value.id || !ids.insert(value.id).second)
            throw std::runtime_error("AST node ID is zero or repeated (sharing/cycle)");
        Fields f{{"id", std::to_string(value.id)}};
        auto add = [&](const std::string& name, const std::string& data) {
            f.emplace_back(name, data);
        };
        // dynamic_cast also verifies that kind agrees with the concrete class.
#define NODE(T) case nodeKind::T: { \
    const auto& n = dynamic_cast<const T&>(value); \
    add("kind", quote(#T)); kinds.insert(#T);
#define END break; }
        switch (value.kind) {
        NODE(UnitType) END
        NODE(PathType) add("path", path(n.path)); END
        NODE(RefType)
            add("mutability", quote(n.mutability == Mutability::Mutable ? "Mutable" : "Immutable"));
            add("inner", required(n.inner)); END
        NODE(ArrayType) add("element", required(n.element)); add("length", required(n.length)); END
        NODE(IntLiteral)
            add("text", quote(n.text)); add("base", quote(intbase_to_string(n.base)));
            add("suffix", quote(intsuffix_to_string(n.suffix))); END
        NODE(BoolLiteral) add("value", n.value ? "true" : "false"); END
        NODE(UnitExpr) END
        NODE(PathExpr) add("path", path(n.path)); END
        NODE(UnaryExpr) add("op", quote(unaryop_to_string(n.op))); add("operand", required(n.operand)); END
        NODE(BinaryExpr)
            add("op", quote(binaryop_to_string(n.op)));
            add("left", required(n.left)); add("right", required(n.right)); END
        NODE(AssignExpr)
            add("op", quote(assignop_to_string(n.op)));
            add("target", required(n.target)); add("value", required(n.value)); END
        NODE(CastExpr) add("value", required(n.value)); add("target_type", required(n.target_type)); END
        NODE(CallExpr) add("callee", required(n.callee)); add("arguments", nodes(n.arguments)); END
        NODE(MethodCallExpr)
            add("receiver", required(n.receiver)); add("method", segment(n.method));
            add("arguments", nodes(n.arguments)); END
        NODE(FieldExpr) add("base", required(n.base)); add("field", quote(n.field)); END
        NODE(IndexExpr) add("base", required(n.base)); add("index", required(n.index)); END
        NODE(ArrayExpr) add("elements", nodes(n.elements)); END
        NODE(ArrayRepeatExpr) add("value", required(n.value)); add("count", required(n.count)); END
        NODE(StructExpr) {
            add("path", path(n.path));
            std::vector<std::string> fields;
            for (const auto& field : n.fields)
                fields.push_back(object({{"name", quote(field.name)}, {"value", required(field.value)}}));
            add("fields", array(fields));
        } END
        NODE(BlockExpr) add("statements", nodes(n.statements)); add("tail", optional(n.tail)); END
        NODE(IfExpr)
            if (n.else_branch && n.else_branch->kind != nodeKind::IfExpr && n.else_branch->kind != nodeKind::BlockExpr)
                throw std::runtime_error("if else is neither an if nor a block");
            add("condition", required(n.condition)); add("then_branch", required(n.then_branch));
            add("else_branch", optional(n.else_branch)); END
        NODE(WhileExpr) add("condition", required(n.condition)); add("body", required(n.body)); END
        NODE(LoopExpr) add("body", required(n.body)); END
        NODE(BreakExpr) add("value", optional(n.value)); END
        NODE(ReturnExpr) add("value", optional(n.value)); END
        NODE(ContinueExpr) END
        NODE(EmptyStmt) END
        NODE(LetStmt)
            add("name", quote(n.binding.name));
            add("mutability", quote(n.binding.mutability == Mutability::Mutable ? "Mutable" : "Immutable"));
            add("annotation", optional(n.annotation)); add("initializer", required(n.initializer)); END
        NODE(ExprStmt) add("expression", required(n.expression)); add("has_semicolon", n.has_semicolon ? "true" : "false"); END
        NODE(FunctionItem) {
            add("name", quote(n.name)); add("receiver", quote(receiver_to_string(n.receiver)));
            std::vector<std::string> params;
            for (const auto& param : n.parameters)
                params.push_back(object({{"name", quote(param.binding.name)},
                    {"mutability", quote(param.binding.mutability == Mutability::Mutable ? "Mutable" : "Immutable")},
                    {"type", required(param.type)}}));
            add("parameters", array(params)); add("return_type", optional(n.return_type));
            add("body", required(n.body));
        } END
        NODE(StructItem) {
            add("name", quote(n.name));
            std::vector<std::string> fields, attributes;
            for (const auto& field : n.fields)
                fields.push_back(object({{"name", quote(field.name)}, {"type", required(field.type)}}));
            for (const auto& attr : n.derives) {
                std::vector<std::string> entries;
                for (auto entry : attr) entries.push_back(quote(derivekind_to_string(entry)));
                attributes.push_back(array(entries));
            }
            add("fields", array(fields)); add("derives", array(attributes));
        } END
        NODE(ConstItem) add("name", quote(n.name)); add("type", required(n.type)); add("value", required(n.value)); END
        NODE(ImplItem) add("target_type", required(n.target_type)); add("items", nodes(n.items)); END
        NODE(Crate) add("items", nodes(n.items)); END
        default: throw std::runtime_error("unknown AST kind");
        }
#undef NODE
#undef END
        return object(f);
    }
};

struct Errors : antlr4::BaseErrorListener {
    std::vector<std::string> messages;
    void syntaxError(antlr4::Recognizer*, antlr4::Token*, std::size_t line,
                     std::size_t column, const std::string& msg, std::exception_ptr) override {
        messages.push_back(std::to_string(line) + ':' + std::to_string(column) + ' ' + msg);
    }
};

struct TracingBuilder : ASTbuilder {
    std::set<std::size_t> rules;
    std::any visit(antlr4::tree::ParseTree* node) override {
        if (auto* ctx = dynamic_cast<antlr4::ParserRuleContext*>(node)) rules.insert(ctx->getRuleIndex());
        return rx::RxParserBaseVisitor::visit(node);
    }
};

struct Result {
    ASTnode_ptr root;
    std::set<std::string> rules;
    std::string parse_tree;
};

struct ParseError : std::runtime_error { using std::runtime_error::runtime_error; };

Result parse(const std::string& entry, const std::string& source, bool include_tree) {
    antlr4::ANTLRInputStream input(source);
    rx::RxLexer lexer(&input);
    Errors errors;
    lexer.removeErrorListeners(); lexer.addErrorListener(&errors);
    antlr4::CommonTokenStream tokens(&lexer);
    tokens.fill();
    for (auto* token : tokens.getTokens()) {
        auto name = lexer.getVocabulary().getSymbolicName(token->getType());
        if (name.rfind("INVALID_", 0) == 0 || name == "ERROR_CHAR" || name == "UNTERMINATED_BLOCK_COMMENT")
            errors.messages.push_back("invalid lexical token: " + token->getText());
    }
    if (!errors.messages.empty()) throw ParseError(errors.messages.front());
    rx::RxParser parser(&tokens);
    parser.removeErrorListeners(); parser.addErrorListener(&errors);
    antlr4::tree::ParseTree* tree = nullptr;
    if (entry == "crate") tree = parser.crate();
    else if (entry == "item") tree = parser.item();
    else if (entry == "typeRef") tree = parser.typeRef();
    else if (entry == "expression") tree = parser.expression();
    else if (entry == "letStatement") tree = parser.letStatement();
    else throw std::runtime_error("unsupported parser entry: " + entry);
    if (!errors.messages.empty()) throw ParseError(errors.messages.front());
    if (tokens.LA(1) != antlr4::Token::EOF) throw ParseError("trailing input after parser entry");
    TracingBuilder builder;
    Result result;
    if (entry == "crate") result.root = builder.build(dynamic_cast<rx::RxParser::CrateContext*>(tree));
    else {
        auto value = builder.visit(tree);
        if (entry == "item") result.root = std::any_cast<Itemnode_ptr>(value);
        else if (entry == "typeRef") result.root = std::any_cast<Typenode_ptr>(value);
        else if (entry == "expression") result.root = std::any_cast<Exprnode_ptr>(value);
        else result.root = std::any_cast<Stmtnode_ptr>(value);
    }
    if (!result.root && entry != "item") throw std::runtime_error("AST root is null");
    for (auto rule : builder.rules) result.rules.insert(parser.getRuleNames().at(rule));
    if (include_tree) result.parse_tree = tree->toStringTree(&parser);
    return result;
    // Parser, lexer and tokens are destroyed before the caller inspects the AST.
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) return 2;
    if (argc == 4 && std::string(argv[3]) != "--tree") return 2;
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) return 2;
    std::ostringstream source;
    source << input.rdbuf();
    try {
        auto result = parse(argv[1], source.str(), argc == 4);
        InspectAST inspector;
        auto ast = inspector.optional(result.root);
        std::vector<std::string> rules, kinds;
        for (const auto& rule : result.rules) rules.push_back(quote(rule));
        for (const auto& kind : inspector.kinds) kinds.push_back(quote(kind));
        Fields output{{"status", quote("ok")}, {"ast", ast},
            {"node_count", std::to_string(inspector.ids.size())},
            {"kinds", array(kinds)}, {"visited_rules", array(rules)}};
        if (argc == 4) output.emplace_back("parse_tree", quote(result.parse_tree));
        std::cout << object(output) << '\n';
        return 0;
    } catch (const ParseError& error) {
        std::cout << object({{"status", quote("parse-error")}, {"error", quote(error.what())}}) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cout << object({{"status", quote("builder-error")}, {"error", quote(error.what())}}) << '\n';
        return 2;
    }
}
