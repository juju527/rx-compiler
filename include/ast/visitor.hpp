#pragma once
#include "ast.hpp"

namespace rx::AST {

class ASTwalker : public ASTvisitor {
public:
    ~ASTwalker() override = default;
    virtual void visit(const Crate&) override;
    virtual void visit(const FunctionItem&) override;
    virtual void visit(const StructItem&) override;
    virtual void visit(const ConstItem&) override;
    virtual void visit(const ImplItem&) override;
    virtual void visit(const UnitType&) override;
    virtual void visit(const PathType&) override;
    virtual void visit(const RefType&) override;
    virtual void visit(const ArrayType&) override;
    virtual void visit(const EmptyStmt&) override;
    virtual void visit(const LetStmt&) override;
    virtual void visit(const ExprStmt&) override;
    virtual void visit(const IntLiteral&) override;
    virtual void visit(const BoolLiteral&) override;
    virtual void visit(const UnitExpr&) override;
    virtual void visit(const PathExpr&) override;
    virtual void visit(const UnaryExpr&) override;
    virtual void visit(const BinaryExpr&) override;
    virtual void visit(const AssignExpr&) override;
    virtual void visit(const CastExpr&) override;
    virtual void visit(const CallExpr&) override;
    virtual void visit(const MethodCallExpr&) override;
    virtual void visit(const FieldExpr&) override;
    virtual void visit(const IndexExpr&) override;
    virtual void visit(const ArrayExpr&) override;
    virtual void visit(const ArrayRepeatExpr&) override;
    virtual void visit(const StructExpr&) override;
    virtual void visit(const BlockExpr&) override;
    virtual void visit(const IfExpr&) override;
    virtual void visit(const WhileExpr&) override;
    virtual void visit(const LoopExpr&) override;
    virtual void visit(const BreakExpr&) override;
    virtual void visit(const ReturnExpr&) override;
    virtual void visit(const ContinueExpr&) override;
};

class ASTprinter : public ASTwalker {
private:
    int dep = 0;
    string tab() const{return string(dep, '\t');}
public:
    ASTprinter() : dep(0) {}
    ~ASTprinter() override = default;
    void visit(const Crate&) override;
    void visit(const FunctionItem&) override;
    void visit(const StructItem&) override;
    void visit(const ConstItem&) override;
    void visit(const ImplItem&) override;
    void visit(const UnitType&) override;
    void visit(const PathType&) override;
    void visit(const RefType&) override;
    void visit(const ArrayType&) override;
    void visit(const EmptyStmt&) override;
    void visit(const LetStmt&) override;
    void visit(const ExprStmt&) override;
    void visit(const IntLiteral&) override;
    void visit(const BoolLiteral&) override;
    void visit(const UnitExpr&) override;
    void visit(const PathExpr&) override;
    void visit(const UnaryExpr&) override;
    void visit(const BinaryExpr&) override;
    void visit(const AssignExpr&) override;
    void visit(const CastExpr&) override;
    void visit(const CallExpr&) override;
    void visit(const MethodCallExpr&) override;
    void visit(const FieldExpr&) override;
    void visit(const IndexExpr&) override;
    void visit(const ArrayExpr&) override;
    void visit(const ArrayRepeatExpr&) override;
    void visit(const StructExpr&) override;
    void visit(const BlockExpr&) override;
    void visit(const IfExpr&) override;
    void visit(const WhileExpr&) override;
    void visit(const LoopExpr&) override;
    void visit(const BreakExpr&) override;
    void visit(const ReturnExpr&) override;
    void visit(const ContinueExpr&) override;
};

};