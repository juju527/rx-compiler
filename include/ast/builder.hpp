#pragma once

#include <any>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "RxParser.h"
#include "RxParserBaseVisitor.h"
#include "ast.hpp"


namespace rx::AST{
using std::optional;
using std::shared_ptr;
using std::any;
using std::any_cast;
using std::variant;
using std::vector;

using Crate_ptr = shared_ptr<Crate>;
using Function_ptr = shared_ptr<FunctionItem>;
using Struct_ptr = shared_ptr<StructItem>;
using Const_ptr = shared_ptr<ConstItem>;
using Impl_ptr = shared_ptr<ImplItem>;

using Parser = rx::RxParser;
using RuleContext = antlr4::ParserRuleContext;
using ParseTree = antlr4::tree::ParseTree;
using Terminal = antlr4::tree::TerminalNode;
using ErrorNode = antlr4::tree::ErrorNode;


struct Ignored {};
struct FunctionParametersResult {
    Receiver receiver = Receiver::Null;
    vector<Parameter> parameters;
};

using UnaryOperatorPlan = std::vector<UnaryOp>;

struct CallSuffix { vector<Exprnode_ptr> arguments; };
struct IndexSuffix { Exprnode_ptr index; };
struct FieldSuffix { Identifier field; };
struct MethodSuffix {
    PathSegment method;
    vector<Exprnode_ptr> arguments;
};
using PostfixPlan = variant<CallSuffix, IndexSuffix, FieldSuffix, MethodSuffix>;


class ASTbuilder : public rx::RxParserBaseVisitor{
public:
    Crate_ptr build(RxParser::CrateContext* root);

    any visitChildren(antlr4::tree::ParseTree* node) override;
    any visitTerminal(antlr4::tree::TerminalNode* node) override;
    any visitErrorNode(antlr4::tree::ErrorNode* node) override;

    any visitCrate(RxParser::CrateContext* ctx) override;
    any visitItem(RxParser::ItemContext* ctx) override;
    any visitUseDeclaration(RxParser::UseDeclarationContext* ctx) override;
    any visitUseTree(RxParser::UseTreeContext* ctx) override;
    any visitUsePath(RxParser::UsePathContext* ctx) override;
    any visitUsePathSegment(RxParser::UsePathSegmentContext* ctx) override;
    any visitFunctionDefinition(RxParser::FunctionDefinitionContext* ctx) override;
    any visitFunctionParameters(RxParser::FunctionParametersContext* ctx) override;
    any visitSelfParam(RxParser::SelfParamContext* ctx) override;
    any visitFunctionParam(RxParser::FunctionParamContext* ctx) override;
    any visitStructDefinition(RxParser::StructDefinitionContext* ctx) override;
    any visitStructField(RxParser::StructFieldContext* ctx) override;
    any visitOuterAttribute(RxParser::OuterAttributeContext* ctx) override;
    any visitDeriveName(RxParser::DeriveNameContext* ctx) override;
    any visitConstantItem(RxParser::ConstantItemContext* ctx) override;
    any visitInherentImpl(RxParser::InherentImplContext* ctx) override;
    any visitAssociatedItem(RxParser::AssociatedItemContext* ctx) override;
    any visitGenericParams(RxParser::GenericParamsContext* ctx) override;
    any visitLifetimeParam(RxParser::LifetimeParamContext* ctx) override;
    any visitLifetime(RxParser::LifetimeContext* ctx) override;
    any visitLifetimeBounds(RxParser::LifetimeBoundsContext* ctx) override;
    any visitTypeParamBounds(RxParser::TypeParamBoundsContext* ctx) override;
    any visitWhereClause(RxParser::WhereClauseContext* ctx) override;
    any visitWhereClauseItem(RxParser::WhereClauseItemContext* ctx) override;
    any visitTypeRef(RxParser::TypeRefContext* ctx) override;
    any visitReferenceType(RxParser::ReferenceTypeContext* ctx) override;
    any visitArrayType(RxParser::ArrayTypeContext* ctx) override;
    any visitTypePath(RxParser::TypePathContext* ctx) override;
    any visitTypePathSegment(RxParser::TypePathSegmentContext* ctx) override;
    any visitPathInExpression(RxParser::PathInExpressionContext* ctx) override;
    any visitPathExprSegment(RxParser::PathExprSegmentContext* ctx) override;
    any visitPathIdentSegment(RxParser::PathIdentSegmentContext* ctx) override;
    any visitGenericArgs(RxParser::GenericArgsContext* ctx) override;
    any visitGenericArg(RxParser::GenericArgContext* ctx) override;
    any visitGenericClose(RxParser::GenericCloseContext* ctx) override;
    any visitClosedCastType(RxParser::ClosedCastTypeContext* ctx) override;
    any visitConstValue(RxParser::ConstValueContext* ctx) override;
    any visitMagnitude(RxParser::MagnitudeContext* ctx) override;
    any visitIdentifierBinding(RxParser::IdentifierBindingContext* ctx) override;
    any visitLetStatement(RxParser::LetStatementContext* ctx) override;
    any visitBlockExpression(RxParser::BlockExpressionContext* ctx) override;
    any visitStatement(RxParser::StatementContext* ctx) override;
    any visitExpressionWithBlock(RxParser::ExpressionWithBlockContext* ctx) override;
    any visitIfExpression(RxParser::IfExpressionContext* ctx) override;
    any visitExpression(RxParser::ExpressionContext* ctx) override;
    any visitAssignmentExpression(RxParser::AssignmentExpressionContext* ctx) override;
    any visitLogicalOrExpression(RxParser::LogicalOrExpressionContext* ctx) override;
    any visitLogicalAndExpression(RxParser::LogicalAndExpressionContext* ctx) override;
    any visitComparisonExpression(RxParser::ComparisonExpressionContext* ctx) override;
    any visitBitOrExpression(RxParser::BitOrExpressionContext* ctx) override;
    any visitClosedBitOrExpression(RxParser::ClosedBitOrExpressionContext* ctx) override;
    any visitBitXorExpression(RxParser::BitXorExpressionContext* ctx) override;
    any visitClosedBitXorExpression(RxParser::ClosedBitXorExpressionContext* ctx) override;
    any visitBitAndExpression(RxParser::BitAndExpressionContext* ctx) override;
    any visitClosedBitAndExpression(RxParser::ClosedBitAndExpressionContext* ctx) override;
    any visitShiftExpression(RxParser::ShiftExpressionContext* ctx) override;
    any visitClosedShiftExpression(RxParser::ClosedShiftExpressionContext* ctx) override;
    any visitAdditiveExpression(RxParser::AdditiveExpressionContext* ctx) override;
    any visitClosedAdditiveExpression(RxParser::ClosedAdditiveExpressionContext* ctx) override;
    any visitMultiplicativeExpression(RxParser::MultiplicativeExpressionContext* ctx) override;
    any visitClosedMultiplicativeExpression(RxParser::ClosedMultiplicativeExpressionContext* ctx) override;
    any visitCastExpression(RxParser::CastExpressionContext* ctx) override;
    any visitClosedCastExpression(RxParser::ClosedCastExpressionContext* ctx) override;
    any visitUnaryExpression(RxParser::UnaryExpressionContext* ctx) override;
    any visitPostfixExpression(RxParser::PostfixExpressionContext* ctx) override;
    any visitConditionExpression(RxParser::ConditionExpressionContext* ctx) override;
    any visitConditionAssignmentExpression(RxParser::ConditionAssignmentExpressionContext* ctx) override;
    any visitConditionLogicalOrExpression(RxParser::ConditionLogicalOrExpressionContext* ctx) override;
    any visitConditionLogicalAndExpression(RxParser::ConditionLogicalAndExpressionContext* ctx) override;
    any visitConditionComparisonExpression(RxParser::ConditionComparisonExpressionContext* ctx) override;
    any visitConditionBitOrExpression(RxParser::ConditionBitOrExpressionContext* ctx) override;
    any visitConditionClosedBitOrExpression(RxParser::ConditionClosedBitOrExpressionContext* ctx) override;
    any visitConditionBitXorExpression(RxParser::ConditionBitXorExpressionContext* ctx) override;
    any visitConditionClosedBitXorExpression(RxParser::ConditionClosedBitXorExpressionContext* ctx) override;
    any visitConditionBitAndExpression(RxParser::ConditionBitAndExpressionContext* ctx) override;
    any visitConditionClosedBitAndExpression(RxParser::ConditionClosedBitAndExpressionContext* ctx) override;
    any visitConditionShiftExpression(RxParser::ConditionShiftExpressionContext* ctx) override;
    any visitConditionClosedShiftExpression(RxParser::ConditionClosedShiftExpressionContext* ctx) override;
    any visitConditionAdditiveExpression(RxParser::ConditionAdditiveExpressionContext* ctx) override;
    any visitConditionClosedAdditiveExpression(RxParser::ConditionClosedAdditiveExpressionContext* ctx) override;
    any visitConditionMultiplicativeExpression(RxParser::ConditionMultiplicativeExpressionContext* ctx) override;
    any visitConditionClosedMultiplicativeExpression(RxParser::ConditionClosedMultiplicativeExpressionContext* ctx) override;
    any visitConditionCastExpression(RxParser::ConditionCastExpressionContext* ctx) override;
    any visitConditionClosedCastExpression(RxParser::ConditionClosedCastExpressionContext* ctx) override;
    any visitConditionUnaryExpression(RxParser::ConditionUnaryExpressionContext* ctx) override;
    any visitConditionPostfixExpression(RxParser::ConditionPostfixExpressionContext* ctx) override;
    any visitConditionBreakExpression(RxParser::ConditionBreakExpressionContext* ctx) override;
    any visitConditionBreakAssignmentExpression(RxParser::ConditionBreakAssignmentExpressionContext* ctx) override;
    any visitConditionBreakLogicalOrExpression(RxParser::ConditionBreakLogicalOrExpressionContext* ctx) override;
    any visitConditionBreakLogicalAndExpression(RxParser::ConditionBreakLogicalAndExpressionContext* ctx) override;
    any visitConditionBreakComparisonExpression(RxParser::ConditionBreakComparisonExpressionContext* ctx) override;
    any visitConditionBreakBitOrExpression(RxParser::ConditionBreakBitOrExpressionContext* ctx) override;
    any visitConditionBreakClosedBitOrExpression(RxParser::ConditionBreakClosedBitOrExpressionContext* ctx) override;
    any visitConditionBreakBitXorExpression(RxParser::ConditionBreakBitXorExpressionContext* ctx) override;
    any visitConditionBreakClosedBitXorExpression(RxParser::ConditionBreakClosedBitXorExpressionContext* ctx) override;
    any visitConditionBreakBitAndExpression(RxParser::ConditionBreakBitAndExpressionContext* ctx) override;
    any visitConditionBreakClosedBitAndExpression(RxParser::ConditionBreakClosedBitAndExpressionContext* ctx) override;
    any visitConditionBreakShiftExpression(RxParser::ConditionBreakShiftExpressionContext* ctx) override;
    any visitConditionBreakClosedShiftExpression(RxParser::ConditionBreakClosedShiftExpressionContext* ctx) override;
    any visitConditionBreakAdditiveExpression(RxParser::ConditionBreakAdditiveExpressionContext* ctx) override;
    any visitConditionBreakClosedAdditiveExpression(RxParser::ConditionBreakClosedAdditiveExpressionContext* ctx) override;
    any visitConditionBreakMultiplicativeExpression(RxParser::ConditionBreakMultiplicativeExpressionContext* ctx) override;
    any visitConditionBreakClosedMultiplicativeExpression(RxParser::ConditionBreakClosedMultiplicativeExpressionContext* ctx) override;
    any visitConditionBreakCastExpression(RxParser::ConditionBreakCastExpressionContext* ctx) override;
    any visitConditionBreakClosedCastExpression(RxParser::ConditionBreakClosedCastExpressionContext* ctx) override;
    any visitConditionBreakUnaryExpression(RxParser::ConditionBreakUnaryExpressionContext* ctx) override;
    any visitConditionBreakPostfixExpression(RxParser::ConditionBreakPostfixExpressionContext* ctx) override;
    any visitStatementExpression(RxParser::StatementExpressionContext* ctx) override;
    any visitStatementAssignmentExpression(RxParser::StatementAssignmentExpressionContext* ctx) override;
    any visitStatementLogicalOrExpression(RxParser::StatementLogicalOrExpressionContext* ctx) override;
    any visitStatementLogicalAndExpression(RxParser::StatementLogicalAndExpressionContext* ctx) override;
    any visitStatementComparisonExpression(RxParser::StatementComparisonExpressionContext* ctx) override;
    any visitStatementBitOrExpression(RxParser::StatementBitOrExpressionContext* ctx) override;
    any visitStatementClosedBitOrExpression(RxParser::StatementClosedBitOrExpressionContext* ctx) override;
    any visitStatementBitXorExpression(RxParser::StatementBitXorExpressionContext* ctx) override;
    any visitStatementClosedBitXorExpression(RxParser::StatementClosedBitXorExpressionContext* ctx) override;
    any visitStatementBitAndExpression(RxParser::StatementBitAndExpressionContext* ctx) override;
    any visitStatementClosedBitAndExpression(RxParser::StatementClosedBitAndExpressionContext* ctx) override;
    any visitStatementShiftExpression(RxParser::StatementShiftExpressionContext* ctx) override;
    any visitStatementClosedShiftExpression(RxParser::StatementClosedShiftExpressionContext* ctx) override;
    any visitStatementAdditiveExpression(RxParser::StatementAdditiveExpressionContext* ctx) override;
    any visitStatementClosedAdditiveExpression(RxParser::StatementClosedAdditiveExpressionContext* ctx) override;
    any visitStatementMultiplicativeExpression(RxParser::StatementMultiplicativeExpressionContext* ctx) override;
    any visitStatementClosedMultiplicativeExpression(RxParser::StatementClosedMultiplicativeExpressionContext* ctx) override;
    any visitStatementCastExpression(RxParser::StatementCastExpressionContext* ctx) override;
    any visitStatementClosedCastExpression(RxParser::StatementClosedCastExpressionContext* ctx) override;
    any visitStatementUnaryExpression(RxParser::StatementUnaryExpressionContext* ctx) override;
    any visitStatementPostfixExpression(RxParser::StatementPostfixExpressionContext* ctx) override;
    any visitPrimaryExpression(RxParser::PrimaryExpressionContext* ctx) override;
    any visitNonBlockPrimary(RxParser::NonBlockPrimaryContext* ctx) override;
    any visitConditionPrimary(RxParser::ConditionPrimaryContext* ctx) override;
    any visitConditionPrimaryWithoutBareBlock(RxParser::ConditionPrimaryWithoutBareBlockContext* ctx) override;
    any visitLiteralExpression(RxParser::LiteralExpressionContext* ctx) override;
    any visitStructExprFields(RxParser::StructExprFieldsContext* ctx) override;
    any visitStructExprField(RxParser::StructExprFieldContext* ctx) override;
    any visitArrayExpression(RxParser::ArrayExpressionContext* ctx) override;
    any visitPostfixSuffix(RxParser::PostfixSuffixContext* ctx) override;
    any visitDotSuffix(RxParser::DotSuffixContext* ctx) override;
    any visitCallArguments(RxParser::CallArgumentsContext* ctx) override;
    any visitUnaryOperator(RxParser::UnaryOperatorContext* ctx) override;
    any visitMultiplicativeOperator(RxParser::MultiplicativeOperatorContext* ctx) override;
    any visitAdditiveOperator(RxParser::AdditiveOperatorContext* ctx) override;
    any visitShiftRight(RxParser::ShiftRightContext* ctx) override;
    any visitComparisonExceptLt(RxParser::ComparisonExceptLtContext* ctx) override;
    any visitAssignmentOperator(RxParser::AssignmentOperatorContext* ctx) override;
    any visitEqualsSign(RxParser::EqualsSignContext* ctx) override;
    any visitIdentifier(RxParser::IdentifierContext* ctx) override;

private:
    size_t next_id = 1;
    template<class T>
    T take(antlr4::tree::ParseTree* node) {return any_cast<T>(visit(node));}
    
    Exprnode_ptr expr(ParseTree* node);
    Typenode_ptr type(ParseTree* node);
    Block_ptr block(RxParser::BlockExpressionContext* ctx);
    Stmtnode_ptr stmt(RxParser::StatementContext* ctx);
    Exprnode_ptr foldBinary(RuleContext* ctx);
    Exprnode_ptr buildComparison(RuleContext* ctx);
    Exprnode_ptr applyUnary(Exprnode_ptr operand, const UnaryOperatorPlan& plan);
    Typenode_ptr applyReference(Typenode_ptr inner, bool doubled, bool mutable_inner);
    Exprnode_ptr applySuffix(Exprnode_ptr base, PostfixPlan suffix);
    Exprnode_ptr makeInteger(const std::string& spelling);
    std::size_t nextId();

};

};
