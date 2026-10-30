#include "checker.hpp"

#include <algorithm>

using std::remove;

namespace rx::semantic {

// 解析 integer literal。
Constant Checker::integerLiteral(const IntLiteral& node, TypeId expected, bool negative) {
    TypeId type = i32;
    switch (node.suffix) {
    case IntegerSuffix::None: type = Types::integer(expected) ? expected : i32; break;
    case IntegerSuffix::I32: type = i32; break;
    case IntegerSuffix::U32: type = u32; break;
    case IntegerSuffix::ISize: type = isize; break;
    case IntegerSuffix::USize: type = usize; break;
    }
    if (negative && !Types::signedInteger(type)) fail(node.id, "unary minus requires a signed integer");
    auto text = node.text;
    for (string suffix : {"isize", "usize", "i32", "u32"}) {
        if (text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
            text.resize(text.size() - suffix.size());
            break;
        }
    }
    text.erase(remove(text.begin(), text.end(), '_'), text.end());
    const unsigned base = static_cast<unsigned>(node.base);
    uint64_t magnitude = 0;
    uint64_t limit = Types::signedInteger(type) ? (negative ? 2147483648ULL : 2147483647ULL) : 4294967295ULL;
    for (size_t index = base == 10 ? 0 : 2; index < text.size(); index++) {
        char c = text[index];
        unsigned digit = c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10);
        if (digit >= base || magnitude > (limit - digit) / base) fail(node.id, "integer literal is outside ");
        magnitude = magnitude * base + digit;
    }
    auto bits = static_cast<uint32_t>(magnitude);
    if (negative) bits = uint32_t{0} - bits;
    return {type, bits};
}

// 解析常量表达式，判断是否有依赖环。
Constant Checker::evaluateConstant(NodeId id) {
    auto& syntax = constants.at(id);
    if (syntax.state == 1) fail(id, "cyclic constant dependency involving '" + syntax.node->name + "'");
    if (syntax.state == 2) return result.constants.at(id);
    syntax.state = 1;
    auto type = resolveType(*syntax.node->type, syntax.self);
    auto res = constExpr(*syntax.node->value, type, syntax.self);
    result.constants[id] = res;
    syntax.state = 2;
    return res;
}

// 解析常量表达式。
Constant Checker::constExpr(const Exprnode& node, TypeId expected, TypeId self) {
    Constant res;
    optional<OperatorInfo> operation;
    optional<ResolvedName> resolved_name;
    switch (node.kind) {
    case nodeKind::IntLiteral: res = integerLiteral(static_cast<const IntLiteral&>(node), expected); break;
    case nodeKind::BoolLiteral: res = {boolean, static_cast<const BoolLiteral&>(node).value ? 1u : 0u}; break;
    case nodeKind::PathExpr: {
        const auto& path = static_cast<const PathExpr&>(node).path;
        auto symbol = resolveName(path, node.id, self);
        if (symbol.kind != NameKind::Constant) fail(node.id, "a constant path must resolve to a constant item");
        resolved_name = symbol;
        res = evaluateConstant(symbol.id);
        break;
    }
    case nodeKind::UnaryExpr: {
        const auto& unary = static_cast<const UnaryExpr&>(node);
        if (unary.op != UnaryOp::Negate) fail(node.id, "only restricted negation is allowed in a constant");
        if (unary.operand->kind == nodeKind::IntLiteral) {
            res = integerLiteral(static_cast<const IntLiteral&>(*unary.operand), unknown, true);
            auto operand = value(res.type);
            operand.constant_value = Constant{res.type, uint32_t(0) - res.bits};
            result.expressions[unary.operand->id] = move(operand);
        } else {
            if (unary.operand->kind != nodeKind::PathExpr) fail(node.id, "invalid constant magnitude");
            res = constExpr(*unary.operand, unknown, self);
            if (!Types::signedInteger(res.type)) fail(node.id, "constant negation requires a signed integer");
            res.bits = uint32_t(0) - res.bits;
        }
        operation = OperatorInfo{res.type};
        break;
    }
    default: fail(node.id, "expression is not a restricted constant");
    }
    if (expected != unknown && res.type != expected) fail(node.id, "constant expression type mismatch");
    auto info = value(res.type);
    info.operation = operation;
    info.resolved_name = resolved_name;
    info.constant_value = res;
    result.expressions[node.id] = move(info);
    return res;
}

};