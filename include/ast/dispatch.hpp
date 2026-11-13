#pragma once

#include "ast.hpp"

namespace rx::AST {

// 按节点类别分派，返回值和参数由具体处理类决定。
// kind 与节点实际类型保持一致。
template<class Derived, class Result, class... Args>
class ASTvisitor {
public:
    Result dispatch(const Exprnode& node, Args... args) {
        auto& pass = static_cast<Derived&>(*this);
        switch (node.kind) {
        case nodeKind::IntLiteral:
            return pass.visit(static_cast<const IntLiteral&>(node), forward<Args>(args)...);
        case nodeKind::BoolLiteral:
            return pass.visit(static_cast<const BoolLiteral&>(node), forward<Args>(args)...);
        case nodeKind::UnitExpr:
            return pass.visit(static_cast<const UnitExpr&>(node), forward<Args>(args)...);
        case nodeKind::PathExpr:
            return pass.visit(static_cast<const PathExpr&>(node), forward<Args>(args)...);
        case nodeKind::UnaryExpr:
            return pass.visit(static_cast<const UnaryExpr&>(node), forward<Args>(args)...);
        case nodeKind::BinaryExpr:
            return pass.visit(static_cast<const BinaryExpr&>(node), forward<Args>(args)...);
        case nodeKind::AssignExpr:
            return pass.visit(static_cast<const AssignExpr&>(node), forward<Args>(args)...);
        case nodeKind::CastExpr:
            return pass.visit(static_cast<const CastExpr&>(node), forward<Args>(args)...);
        case nodeKind::CallExpr:
            return pass.visit(static_cast<const CallExpr&>(node), forward<Args>(args)...);
        case nodeKind::MethodCallExpr:
            return pass.visit(static_cast<const MethodCallExpr&>(node), forward<Args>(args)...);
        case nodeKind::FieldExpr:
            return pass.visit(static_cast<const FieldExpr&>(node), forward<Args>(args)...);
        case nodeKind::IndexExpr:
            return pass.visit(static_cast<const IndexExpr&>(node), forward<Args>(args)...);
        case nodeKind::ArrayExpr:
            return pass.visit(static_cast<const ArrayExpr&>(node), forward<Args>(args)...);
        case nodeKind::ArrayRepeatExpr:
            return pass.visit(static_cast<const ArrayRepeatExpr&>(node), forward<Args>(args)...);
        case nodeKind::StructExpr:
            return pass.visit(static_cast<const StructExpr&>(node), forward<Args>(args)...);
        case nodeKind::BlockExpr:
            return pass.visit(static_cast<const BlockExpr&>(node), forward<Args>(args)...);
        case nodeKind::IfExpr:
            return pass.visit(static_cast<const IfExpr&>(node), forward<Args>(args)...);
        case nodeKind::WhileExpr:
            return pass.visit(static_cast<const WhileExpr&>(node), forward<Args>(args)...);
        case nodeKind::LoopExpr:
            return pass.visit(static_cast<const LoopExpr&>(node), forward<Args>(args)...);
        case nodeKind::BreakExpr:
            return pass.visit(static_cast<const BreakExpr&>(node), forward<Args>(args)...);
        case nodeKind::ReturnExpr:
            return pass.visit(static_cast<const ReturnExpr&>(node), forward<Args>(args)...);
        case nodeKind::ContinueExpr:
            return pass.visit(static_cast<const ContinueExpr&>(node), forward<Args>(args)...);
        default: __builtin_unreachable();
        }
    }

    Result dispatch(const Stmtnode& node, Args... args) {
        auto& pass = static_cast<Derived&>(*this);
        switch (node.kind) {
        case nodeKind::EmptyStmt:
            return pass.visit(static_cast<const EmptyStmt&>(node), forward<Args>(args)...);
        case nodeKind::LetStmt:
            return pass.visit(static_cast<const LetStmt&>(node), forward<Args>(args)...);
        case nodeKind::ExprStmt:
            return pass.visit(static_cast<const ExprStmt&>(node), forward<Args>(args)...);
        default: __builtin_unreachable();
        }
    }

    Result dispatch(const Typenode& node, Args... args) {
        auto& pass = static_cast<Derived&>(*this);
        switch (node.kind) {
        case nodeKind::UnitType:
            return pass.visit(static_cast<const UnitType&>(node), forward<Args>(args)...);
        case nodeKind::PathType:
            return pass.visit(static_cast<const PathType&>(node), forward<Args>(args)...);
        case nodeKind::RefType:
            return pass.visit(static_cast<const RefType&>(node), forward<Args>(args)...);
        case nodeKind::ArrayType:
            return pass.visit(static_cast<const ArrayType&>(node), forward<Args>(args)...);
        default: __builtin_unreachable();
        }
    }

    Result dispatch(const AssociatedItem& node, Args... args) {
        auto& pass = static_cast<Derived&>(*this);
        switch (node.kind) {
        case nodeKind::FunctionItem:
            return pass.visit(static_cast<const FunctionItem&>(node), forward<Args>(args)...);
        case nodeKind::ConstItem:
            return pass.visit(static_cast<const ConstItem&>(node), forward<Args>(args)...);
        default: __builtin_unreachable();
        }
    }

    Result dispatch(const Itemnode& node, Args... args) {
        auto& pass = static_cast<Derived&>(*this);
        switch (node.kind) {
        case nodeKind::FunctionItem:
            return pass.visit(static_cast<const FunctionItem&>(node), forward<Args>(args)...);
        case nodeKind::StructItem:
            return pass.visit(static_cast<const StructItem&>(node), forward<Args>(args)...);
        case nodeKind::ConstItem:
            return pass.visit(static_cast<const ConstItem&>(node), forward<Args>(args)...);
        case nodeKind::ImplItem:
            return pass.visit(static_cast<const ImplItem&>(node), forward<Args>(args)...);
        default: __builtin_unreachable();
        }
    }

    Result dispatch(const Crate& node, Args... args) {
        return static_cast<Derived&>(*this).visit(node, forward<Args>(args)...);
    }

    Result dispatch(const ASTnode& node, Args... args) {
        auto& pass = static_cast<Derived&>(*this);
        switch (node.kind) {
        case nodeKind::Crate:
            return pass.visit(static_cast<const Crate&>(node), forward<Args>(args)...);
        case nodeKind::FunctionItem:
            return pass.visit(static_cast<const FunctionItem&>(node), forward<Args>(args)...);
        case nodeKind::StructItem:
            return pass.visit(static_cast<const StructItem&>(node), forward<Args>(args)...);
        case nodeKind::ConstItem:
            return pass.visit(static_cast<const ConstItem&>(node), forward<Args>(args)...);
        case nodeKind::ImplItem:
            return pass.visit(static_cast<const ImplItem&>(node), forward<Args>(args)...);
        case nodeKind::UnitType:
            return pass.visit(static_cast<const UnitType&>(node), forward<Args>(args)...);
        case nodeKind::PathType:
            return pass.visit(static_cast<const PathType&>(node), forward<Args>(args)...);
        case nodeKind::RefType:
            return pass.visit(static_cast<const RefType&>(node), forward<Args>(args)...);
        case nodeKind::ArrayType:
            return pass.visit(static_cast<const ArrayType&>(node), forward<Args>(args)...);
        case nodeKind::EmptyStmt:
            return pass.visit(static_cast<const EmptyStmt&>(node), forward<Args>(args)...);
        case nodeKind::LetStmt:
            return pass.visit(static_cast<const LetStmt&>(node), forward<Args>(args)...);
        case nodeKind::ExprStmt:
            return pass.visit(static_cast<const ExprStmt&>(node), forward<Args>(args)...);
        case nodeKind::IntLiteral:
            return pass.visit(static_cast<const IntLiteral&>(node), forward<Args>(args)...);
        case nodeKind::BoolLiteral:
            return pass.visit(static_cast<const BoolLiteral&>(node), forward<Args>(args)...);
        case nodeKind::UnitExpr:
            return pass.visit(static_cast<const UnitExpr&>(node), forward<Args>(args)...);
        case nodeKind::PathExpr:
            return pass.visit(static_cast<const PathExpr&>(node), forward<Args>(args)...);
        case nodeKind::UnaryExpr:
            return pass.visit(static_cast<const UnaryExpr&>(node), forward<Args>(args)...);
        case nodeKind::BinaryExpr:
            return pass.visit(static_cast<const BinaryExpr&>(node), forward<Args>(args)...);
        case nodeKind::AssignExpr:
            return pass.visit(static_cast<const AssignExpr&>(node), forward<Args>(args)...);
        case nodeKind::CastExpr:
            return pass.visit(static_cast<const CastExpr&>(node), forward<Args>(args)...);
        case nodeKind::CallExpr:
            return pass.visit(static_cast<const CallExpr&>(node), forward<Args>(args)...);
        case nodeKind::MethodCallExpr:
            return pass.visit(static_cast<const MethodCallExpr&>(node), forward<Args>(args)...);
        case nodeKind::FieldExpr:
            return pass.visit(static_cast<const FieldExpr&>(node), forward<Args>(args)...);
        case nodeKind::IndexExpr:
            return pass.visit(static_cast<const IndexExpr&>(node), forward<Args>(args)...);
        case nodeKind::ArrayExpr:
            return pass.visit(static_cast<const ArrayExpr&>(node), forward<Args>(args)...);
        case nodeKind::ArrayRepeatExpr:
            return pass.visit(static_cast<const ArrayRepeatExpr&>(node), forward<Args>(args)...);
        case nodeKind::StructExpr:
            return pass.visit(static_cast<const StructExpr&>(node), forward<Args>(args)...);
        case nodeKind::BlockExpr:
            return pass.visit(static_cast<const BlockExpr&>(node), forward<Args>(args)...);
        case nodeKind::IfExpr:
            return pass.visit(static_cast<const IfExpr&>(node), forward<Args>(args)...);
        case nodeKind::WhileExpr:
            return pass.visit(static_cast<const WhileExpr&>(node), forward<Args>(args)...);
        case nodeKind::LoopExpr:
            return pass.visit(static_cast<const LoopExpr&>(node), forward<Args>(args)...);
        case nodeKind::BreakExpr:
            return pass.visit(static_cast<const BreakExpr&>(node), forward<Args>(args)...);
        case nodeKind::ReturnExpr:
            return pass.visit(static_cast<const ReturnExpr&>(node), forward<Args>(args)...);
        case nodeKind::ContinueExpr:
            return pass.visit(static_cast<const ContinueExpr&>(node), forward<Args>(args)...);
        default: __builtin_unreachable();
        }
    }

protected:
    ~ASTvisitor() = default;
};

} // namespace rx::AST
