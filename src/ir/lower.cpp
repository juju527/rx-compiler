#include "lower.hpp"

using std::logic_error;
using std::to_string; 
using std::move;

namespace rx::IR {

using namespace AST;

void fail(const string& message) {throw logic_error("IR lowering: " + message);}

FunctionContext& Lowerer::context() {
    if (!function_ctx) fail("no active function");
    return *function_ctx;
}

Builder& Lowerer::builder() {return context().builder;}

void Lowerer::beginFunction(Function& function, SourceTypeId result) {
    if (function_ctx) fail("previous function is still active");
    function_ctx.emplace(module, function, result);
    return ;
}
void Lowerer::endFunction() {
    if (builder().live()) fail("function ended without a terminator");
    function_ctx.reset();
    return ;
}

const semantic::ExprInfo& Lowerer::exprInfo(const Exprnode& node) const {
    auto found = semantic.expressions.find(node.id);
    if (found == semantic.expressions.end()) fail("missing semantic expression " + to_string(node.id));
    return found->second;
}

// 拿到 IR 阶段需要的 AST 节点。
void Lowerer::indexLoweringSyntax(const ASTnode& node) {
    auto expression = dynamic_cast<const Exprnode*>(&node);
    if (expression && !expr_nodes.emplace(node.id, expression).second) fail("duplicate expression ID");
    auto child = [&](const auto& pointer) { if (pointer) indexLoweringSyntax(*pointer); };
    switch (node.kind) {
    case nodeKind::Crate:
        for (const auto& item : static_cast<const Crate&>(node).items) child(item);
        break;
    case nodeKind::ImplItem:
        for (const auto& item : static_cast<const ImplItem&>(node).items) child(item);
        break;
    case nodeKind::FunctionItem: {
        const auto& function = static_cast<const FunctionItem&>(node);
        source_functions.emplace(node.id, &function);
        child(function.body);
        break;
    }
    case nodeKind::LetStmt: child(static_cast<const LetStmt&>(node).initializer); break;
    case nodeKind::ExprStmt: child(static_cast<const ExprStmt&>(node).expression); break;
    case nodeKind::BlockExpr: {
        const auto& block = static_cast<const BlockExpr&>(node);
        for (const auto& statement : block.statements) child(statement);
        child(block.tail);
        break;
    }
    case nodeKind::UnaryExpr: child(static_cast<const UnaryExpr&>(node).operand); break;
    case nodeKind::BinaryExpr: {
        const auto& expression = static_cast<const BinaryExpr&>(node);
        child(expression.left);
        child(expression.right);
        break;
    }
    case nodeKind::AssignExpr: {
        const auto& expression = static_cast<const AssignExpr&>(node);
        child(expression.target);
        child(expression.value);
        break;
    }
    case nodeKind::CastExpr: child(static_cast<const CastExpr&>(node).value); break;
    case nodeKind::CallExpr: {
        const auto& call = static_cast<const CallExpr&>(node);
        child(call.callee);
        for (const auto& argument : call.arguments) child(argument);
        break;
    }
    case nodeKind::MethodCallExpr: {
        const auto& call = static_cast<const MethodCallExpr&>(node);
        child(call.receiver);
        for (const auto& argument : call.arguments) child(argument);
        break;
    }
    case nodeKind::FieldExpr: child(static_cast<const FieldExpr&>(node).base); break;
    case nodeKind::IndexExpr: {
        const auto& expression = static_cast<const IndexExpr&>(node);
        child(expression.base);
        child(expression.index);
        break;
    }
    case nodeKind::ArrayExpr: for (const auto& element : static_cast<const ArrayExpr&>(node).elements) child(element); break;
    case nodeKind::ArrayRepeatExpr: child(static_cast<const ArrayRepeatExpr&>(node).value); break;
    case nodeKind::StructExpr: for (const auto& field : static_cast<const StructExpr&>(node).fields) child(field.value); break;
    case nodeKind::IfExpr: {
        const auto& expression = static_cast<const IfExpr&>(node);
        child(expression.condition);
        child(expression.then_branch);
        child(expression.else_branch);
        break;
    }
    case nodeKind::WhileExpr: {
        const auto& expression = static_cast<const WhileExpr&>(node);
        child(expression.condition);
        child(expression.body);
        break;
    }
    case nodeKind::LoopExpr: child(static_cast<const LoopExpr&>(node).body); break;
    case nodeKind::BreakExpr: child(static_cast<const BreakExpr&>(node).value); break;
    case nodeKind::ReturnExpr: child(static_cast<const ReturnExpr&>(node).value); break;
    default: break; // UnitExpr, ContinueExpr, EmptyStmt, IntLiteral, BoolLiteral, PathExpr
    }
    return ;
}

Module Lowerer::run(const Crate& crate) {
    indexLoweringSyntax(crate);
    emitRuntimeSupport();
    declareSourceFunctions();
    for (const auto& entry : source_functions) lowerFunction(*entry.second);
    emitEntryPoint();
    emitPendingRuntimeDefinitions();
    return move(module);
}

Module lower(const AST::Crate& crate, const semantic::SemanticResult& semantic) {return Lowerer(semantic).run(crate);}
void lowerAndWriteLLVM(const Crate& crate, const semantic::SemanticResult& semantic, ostream& out) {writeLLVM(lower(crate, semantic), out);return ;}

};