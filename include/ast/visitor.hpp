#pragma once
#include <string>   
#include "dispatch.hpp"

namespace rx::AST {
using std::string;

class ASTwalker : public ASTvisitor<ASTwalker> {
public:
    virtual ~ASTwalker() = default;
    
    virtual void visit(const UnitType&);
    virtual void visit(const PathType&);
    virtual void visit(const RefType&);
    virtual void visit(const ArrayType&);

    virtual void visit(const IntLiteral&);
    virtual void visit(const BoolLiteral&);
    virtual void visit(const UnitExpr&);
    virtual void visit(const PathExpr&);
    virtual void visit(const UnaryExpr&);
    virtual void visit(const BinaryExpr&);
    virtual void visit(const AssignExpr&);
    virtual void visit(const CastExpr&);
    virtual void visit(const CallExpr&);
    virtual void visit(const MethodCallExpr&);
    virtual void visit(const FieldExpr&);
    virtual void visit(const IndexExpr&);
    virtual void visit(const ArrayExpr&);
    virtual void visit(const ArrayRepeatExpr&);
    virtual void visit(const StructExpr&);
    virtual void visit(const BlockExpr&);
    virtual void visit(const IfExpr&);
    virtual void visit(const WhileExpr&);
    virtual void visit(const LoopExpr&);
    virtual void visit(const BreakExpr&);
    virtual void visit(const ReturnExpr&);
    virtual void visit(const ContinueExpr&);
    
    virtual void visit(const EmptyStmt&);
    virtual void visit(const LetStmt&);
    virtual void visit(const ExprStmt&);
    
    
    virtual void visit(const FunctionItem&);
    virtual void visit(const StructItem&);
    virtual void visit(const ConstItem&);
    virtual void visit(const ImplItem&);
    
    virtual void visit(const Crate&);
    
    
};

class ASTprinter : public ASTwalker {
private:
    int dep = 0;
    string tab() const{return string(dep, '\t');}
public:
    ASTprinter() : dep(0) {}
    ~ASTprinter() override = default;
    virtual void visit(const UnitType&) override;
    virtual void visit(const PathType&) override;
    virtual void visit(const RefType&) override;
    virtual void visit(const ArrayType&) override;

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
    
    virtual void visit(const EmptyStmt&) override;
    virtual void visit(const LetStmt&) override;
    virtual void visit(const ExprStmt&) override;
    
    
    virtual void visit(const FunctionItem&) override;
    virtual void visit(const StructItem&) override;
    virtual void visit(const ConstItem&) override;
    virtual void visit(const ImplItem&) override;
    
    virtual void visit(const Crate&) override;
};

};