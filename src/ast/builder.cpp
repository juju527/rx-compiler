#include "builder.hpp"

namespace rx::AST{
using std::make_shared;
using std::vector;
using std::move;
using std::pair;
using std::any;
using std::get_if;
using std::get;

Crate_ptr ASTbuilder::build(Parser::CrateContext* root) {return take<Crate_ptr>(root);}

Exprnode_ptr ASTbuilder::expr(ParseTree* node) {return take<Exprnode_ptr>(node);}
Typenode_ptr ASTbuilder::type(ParseTree* node) {return take<Typenode_ptr>(node);}
Block_ptr ASTbuilder::block(Parser::BlockExpressionContext* ctx) {return take<Block_ptr>(ctx);}
Stmtnode_ptr ASTbuilder::stmt(Parser::StatementContext* ctx) {return take<Stmtnode_ptr>(ctx);}
size_t ASTbuilder::nextId() {return next_id++;}

BinaryOp binaryToken(Terminal* node) {
    switch (node->getSymbol()->getType()) {
        case Parser::OROR: return BinaryOp::LogicalOr;
        case Parser::ANDAND: return BinaryOp::LogicalAnd;
        case Parser::PIPE: return BinaryOp::BitOr;
        case Parser::CARET: return BinaryOp::BitXor;
        case Parser::AMP: return BinaryOp::BitAnd;
        case Parser::SHL: return BinaryOp::ShiftLeft;
        default: throw;
    }
}

// left associative
Exprnode_ptr ASTbuilder::foldBinary(RuleContext* ctx) {
    const auto& children = ctx->children;
    auto result = expr(children.front());
    for(int i = 1; i < children.size(); i += 2) {
        auto* operator_node = children[i];
        auto* terminal = dynamic_cast<Terminal*>(operator_node);
        auto op = terminal ? binaryToken(terminal): take<BinaryOp>(operator_node);
        auto operand = expr(children[i + 1]);
        result = make_shared<BinaryExpr>(nextId(), op, move(result), move(operand));
    }
    return result;
}

Exprnode_ptr ASTbuilder::buildComparison(RuleContext* ctx) {
    const auto& children = ctx->children;
    auto left = expr(children.front());
    if (children.size() == 1) return left;
    auto op = dynamic_cast<Terminal*>(children[1]) ? BinaryOp::Less : take<BinaryOp>(children[1]);
    auto right = expr(children[2]);
    return make_shared<BinaryExpr>(nextId(), op, move(left), move(right));
}

//right associative
Exprnode_ptr ASTbuilder::applyUnary(Exprnode_ptr operand, const UnaryOperatorPlan& plan) {
    for (auto it = plan.rbegin(); it != plan.rend(); it++)
        operand = make_shared<UnaryExpr>(nextId(), *it, move(operand));
    return operand;
}

Typenode_ptr ASTbuilder::applyReference(Typenode_ptr inner, bool doubled, bool mutable_inner) {
    inner = make_shared<RefType>(nextId(), move(inner), mutable_inner ? Mutability::Mutable : Mutability::Immutable);
    if (doubled) inner = make_shared<RefType>(nextId(), move(inner), Mutability::Immutable);
    return inner;
}

Exprnode_ptr ASTbuilder::applySuffix(Exprnode_ptr base, PostfixPlan suffix) {
    if (auto* call = get_if<CallSuffix>(&suffix))
        return make_shared<CallExpr>(nextId(), move(base), move(call->arguments));
    if (auto* index = get_if<IndexSuffix>(&suffix))
        return make_shared<IndexExpr>(nextId(), move(base), move(index->index));
    if (auto* field = get_if<FieldSuffix>(&suffix))
        return make_shared<FieldExpr>(nextId(), move(base), move(field->field));
    auto& method = get<MethodSuffix>(suffix);
    return make_shared<MethodCallExpr>(nextId(), move(base), move(method.method), move(method.arguments));
}

Exprnode_ptr ASTbuilder::makeInteger(const string& spelling) {
    IntegerBase base = IntegerBase::Decimal;
    if (spelling.rfind("0b", 0) == 0) base = IntegerBase::Binary;
    else if (spelling.rfind("0o", 0) == 0) base = IntegerBase::Octal;
    else if (spelling.rfind("0x", 0) == 0) base = IntegerBase::Hex;
    
    IntegerSuffix suffix = IntegerSuffix::None;
    const pair<const char*, IntegerSuffix> suffixes[] = { {"isize", IntegerSuffix::ISize}, {"usize", IntegerSuffix::USize}, {"i32", IntegerSuffix::I32}, {"u32", IntegerSuffix::U32}};

    for (const auto& entry : suffixes) {
        const string ending = entry.first;
        if (spelling.size() >= ending.size() && spelling.compare(spelling.size() - ending.size(), ending.size(), ending) == 0) {
            suffix = entry.second;
            break;
        }
    }
    return make_shared<IntLiteral>(nextId(), spelling, base, suffix);
}


any ASTbuilder::visitChildren(ParseTree*) {throw ;}
any ASTbuilder::visitTerminal(Terminal*) {return Ignored{};}
any ASTbuilder::visitErrorNode(ErrorNode*) {throw ;}

any ASTbuilder::visitCrate(RxParser::CrateContext* ctx) {
    vector<Itemnode_ptr> items;
    for (auto* child : ctx->item()) {
        auto result = take<Itemnode_ptr>(child);
        if (result) items.push_back(move(result));
    }
    return make_shared<Crate>(nextId(), move(items));
}
any ASTbuilder::visitItem(RxParser::ItemContext* ctx) {
    if (ctx->useDeclaration()) return Itemnode_ptr{};
    Itemnode_ptr result;
    if (ctx->functionDefinition()) result = take<Function_ptr>(ctx->functionDefinition());
    else if (ctx->structDefinition()) result = take<Struct_ptr>(ctx->structDefinition());
    else if (ctx->constantItem()) result = take<Const_ptr>(ctx->constantItem());
    else result = take<Impl_ptr>(ctx->inherentImpl());
    return move(result);
}
any ASTbuilder::visitUseDeclaration(RxParser::UseDeclarationContext* ctx) {return Ignored{};}
any ASTbuilder::visitUseTree(RxParser::UseTreeContext* ctx) {return Ignored{};}
any ASTbuilder::visitUsePath(RxParser::UsePathContext* ctx) {return Ignored{};}
any ASTbuilder::visitUsePathSegment(RxParser::UsePathSegmentContext* ctx) {return Ignored{};}

any ASTbuilder::visitFunctionDefinition(RxParser::FunctionDefinitionContext* ctx) {
    auto name = take<Identifier>(ctx->identifier());
    FunctionParametersResult signature;
    if (ctx->functionParameters()) signature = take<FunctionParametersResult>(ctx->functionParameters());
    Typenode_ptr result_type;
    if (ctx->typeRef()) result_type = type(ctx->typeRef());
    auto body = block(ctx->blockExpression());
    return make_shared<FunctionItem>(nextId(), move(name), move(body), move(signature.parameters), move(result_type), signature.receiver);
}
any ASTbuilder::visitFunctionParameters(RxParser::FunctionParametersContext* ctx) {
    FunctionParametersResult result;
    if (ctx->selfParam()) result.receiver = take<Receiver>(ctx->selfParam());
    for (auto* child : ctx->functionParam()) result.parameters.push_back(take<Parameter>(child));
    return result;
}
any ASTbuilder::visitSelfParam(RxParser::SelfParamContext* ctx) {
    if (ctx->AMP()) return ctx->MUT() ? Receiver::MutableReference : Receiver::SharedReference;
    return ctx->MUT() ? Receiver::MutableValue : Receiver::Value;
}
any ASTbuilder::visitFunctionParam(RxParser::FunctionParamContext* ctx) {
    auto binding = take<Binding>(ctx->identifierBinding());
    auto annotation = type(ctx->typeRef());
    return Parameter{move(binding), move(annotation)};
}
any ASTbuilder::visitStructDefinition(RxParser::StructDefinitionContext* ctx) {
    vector<DeriveAttribute> derives;
    for (auto* child : ctx->outerAttribute()) derives.push_back(take<DeriveAttribute>(child));
    auto name = take<Identifier>(ctx->identifier());
    vector<FieldDeclaration> fields;
    for (auto* child : ctx->structField()) fields.push_back(take<FieldDeclaration>(child));
    return make_shared<StructItem>(nextId(), move(name), move(fields), move(derives));
}
any ASTbuilder::visitStructField(RxParser::StructFieldContext* ctx) {
    auto name = take<Identifier>(ctx->identifier());
    auto annotation = type(ctx->typeRef());
    return FieldDeclaration{move(name), move(annotation)};
}
any ASTbuilder::visitOuterAttribute(RxParser::OuterAttributeContext* ctx) {
    DeriveAttribute result;
    for (auto* child : ctx->deriveName()) result.push_back(take<DeriveKind>(child));
    return result;
}
any ASTbuilder::visitDeriveName(RxParser::DeriveNameContext* ctx) {
    if (ctx->COPY()) return DeriveKind::Copy;
    if (ctx->CLONE()) return DeriveKind::Clone;
    if (ctx->PARTIAL_EQ()) return DeriveKind::PartialEq;
    return DeriveKind::Eq;
}
any ASTbuilder::visitConstantItem(RxParser::ConstantItemContext* ctx) {
    auto name = take<Identifier>(ctx->identifier());
    auto annotation = type(ctx->typeRef());
    auto value = expr(ctx->constValue());
    return make_shared<ConstItem>(nextId(), move(name), move(annotation), move(value));
}
any ASTbuilder::visitInherentImpl(RxParser::InherentImplContext* ctx) {
    auto target = type(ctx->typeRef());
    vector<AssociatedItem_ptr> items;
    for (auto* child : ctx->associatedItem()) items.push_back(take<AssociatedItem_ptr>(child));
    return make_shared<ImplItem>(nextId(), move(target), move(items));
}
any ASTbuilder::visitAssociatedItem(RxParser::AssociatedItemContext* ctx) {
    AssociatedItem_ptr result;
    if (ctx->constantItem()) result = take<Const_ptr>(ctx->constantItem());
    else result = take<Function_ptr>(ctx->functionDefinition());
    return result;
}

any ASTbuilder::visitGenericParams(RxParser::GenericParamsContext* ctx) {return Ignored{};}
any ASTbuilder::visitLifetimeParam(RxParser::LifetimeParamContext* ctx) {return Ignored{};}
any ASTbuilder::visitLifetime(RxParser::LifetimeContext* ctx) {return Ignored{};}
any ASTbuilder::visitLifetimeBounds(RxParser::LifetimeBoundsContext* ctx) {return Ignored{};}
any ASTbuilder::visitTypeParamBounds(RxParser::TypeParamBoundsContext* ctx) {return Ignored{};}
any ASTbuilder::visitWhereClause(RxParser::WhereClauseContext* ctx) {return Ignored{};}
any ASTbuilder::visitWhereClauseItem(RxParser::WhereClauseItemContext* ctx) {return Ignored{};}

any ASTbuilder::visitTypeRef(RxParser::TypeRefContext* ctx) {
    if (ctx->LPAREN()) {
        if (ctx->typeRef()) return type(ctx->typeRef());
        return Typenode_ptr{make_shared<UnitType>(nextId())};
    }
    if (ctx->typePath()) {
        auto path = take<Path>(ctx->typePath());
        return Typenode_ptr{make_shared<PathType>(nextId(), move(path))};
    }
    if (ctx->referenceType()) return type(ctx->referenceType());
    return type(ctx->arrayType());
}
any ASTbuilder::visitReferenceType(RxParser::ReferenceTypeContext* ctx) {
    auto inner = type(ctx->typeRef());
    return applyReference(move(inner), ctx->ANDAND() != nullptr, ctx->MUT() != nullptr);
}
any ASTbuilder::visitArrayType(RxParser::ArrayTypeContext* ctx) {
    auto element = type(ctx->typeRef());
    auto length = expr(ctx->constValue());
    return Typenode_ptr{make_shared<ArrayType>(nextId(), move(element), move(length))};
}
any ASTbuilder::visitTypePath(RxParser::TypePathContext* ctx) {
    Path result;
    for (auto* child : ctx->typePathSegment()) result.push_back(take<PathSegment>(child));
    return result;
}
any ASTbuilder::visitTypePathSegment(RxParser::TypePathSegmentContext* ctx) {
    auto result = take<PathSegment>(ctx->pathIdentSegment());
    if (ctx->genericArgs()) result.args = take<GenericArguments>(ctx->genericArgs());
    return result;
}
any ASTbuilder::visitPathInExpression(RxParser::PathInExpressionContext* ctx) {
    Path result;
    for (auto* child : ctx->pathExprSegment()) result.push_back(take<PathSegment>(child));
    return result;
}
any ASTbuilder::visitPathExprSegment(RxParser::PathExprSegmentContext* ctx) {
    auto result = take<PathSegment>(ctx->pathIdentSegment());
    if (ctx->genericArgs()) result.args = take<GenericArguments>(ctx->genericArgs());
    return result;
}
any ASTbuilder::visitPathIdentSegment(RxParser::PathIdentSegmentContext* ctx) {
    if (ctx->identifier()) return PathSegment{PathSegmentKind::Identifier, take<Identifier>(ctx->identifier()), {}};
    if (ctx->SELF_VALUE()) return PathSegment{PathSegmentKind::SelfValue, "self", {}};
    return PathSegment{PathSegmentKind::SelfType, "Self", {}};
}
any ASTbuilder::visitGenericArgs(RxParser::GenericArgsContext* ctx) {
    GenericArguments result;
    for (auto* child : ctx->genericArg()) {
        auto argument = take<Typenode_ptr>(child);
        if (argument) result.push_back(move(argument));
    }
    return result;
}
any ASTbuilder::visitGenericArg(RxParser::GenericArgContext* ctx) {
    if (ctx->lifetime()) return Typenode_ptr{};
    return type(ctx->typeRef());
}

any ASTbuilder::visitGenericClose(RxParser::GenericCloseContext* ctx) {return Ignored{};}

any ASTbuilder::visitClosedCastType(RxParser::ClosedCastTypeContext* ctx) {
    if (ctx->LPAREN()) {
        if (ctx->typeRef()) return type(ctx->typeRef());
        return Typenode_ptr{make_shared<UnitType>(nextId())};
    }
    if (ctx->arrayType()) return type(ctx->arrayType());
    if (ctx->AMP() || ctx->ANDAND()) {
        auto inner = type(ctx->closedCastType());
        return applyReference(move(inner), ctx->ANDAND() != nullptr, ctx->MUT() != nullptr);
    }
    Path path;
    for (auto* child : ctx->typePathSegment()) path.push_back(take<PathSegment>(child));
    auto last = take<PathSegment>(ctx->pathIdentSegment());
    last.args = take<GenericArguments>(ctx->genericArgs());
    path.push_back(move(last));
    return Typenode_ptr{make_shared<PathType>(nextId(), move(path))};
}
any ASTbuilder::visitConstValue(RxParser::ConstValueContext* ctx) {
    if (ctx->INTEGER_LITERAL()) return makeInteger(ctx->INTEGER_LITERAL()->getText());
    if (ctx->TRUE() || ctx->FALSE()) return Exprnode_ptr{make_shared<BoolLiteral>(nextId(), ctx->TRUE() != nullptr)};
    if (ctx->pathInExpression()) {
        auto path = take<Path>(ctx->pathInExpression());
        return Exprnode_ptr{make_shared<PathExpr>(nextId(), move(path))};
    }
    if (ctx->MINUS()) {
        auto value = expr(ctx->magnitude());
        return Exprnode_ptr{make_shared<UnaryExpr>(nextId(), UnaryOp::Negate, move(value))};
    }
    return expr(ctx->constValue());
}
any ASTbuilder::visitMagnitude(RxParser::MagnitudeContext* ctx) {
    if (ctx->INTEGER_LITERAL()) return makeInteger(ctx->INTEGER_LITERAL()->getText());
    if (ctx->pathInExpression()) {
        auto path = take<Path>(ctx->pathInExpression());
        return Exprnode_ptr{make_shared<PathExpr>(nextId(), move(path))};
    }
    return expr(ctx->magnitude());
}
any ASTbuilder::visitIdentifierBinding(RxParser::IdentifierBindingContext* ctx) {
    auto name = take<Identifier>(ctx->identifier());
    return Binding{move(name), ctx->MUT() ? Mutability::Mutable : Mutability::Immutable};
}
any ASTbuilder::visitLetStatement(RxParser::LetStatementContext* ctx) {
    auto binding = take<Binding>(ctx->identifierBinding());
    Typenode_ptr annotation;
    if (ctx->typeRef()) annotation = type(ctx->typeRef());
    auto initializer = expr(ctx->expression());
    return Stmtnode_ptr{make_shared<LetStmt>(nextId(), move(binding), move(initializer), move(annotation))};
}
any ASTbuilder::visitBlockExpression(RxParser::BlockExpressionContext* ctx) {
    auto children = ctx->statement();
    ParseTree* tail_context = ctx->statementExpression();
    if (!tail_context && !children.empty()) {
        auto* last = children.back();
        if (last->expressionWithBlock() && !last->SEMI()) {
            tail_context = last->expressionWithBlock();
            children.pop_back();
        }
    }
    vector<Stmtnode_ptr> statements;
    for (auto* child : children) statements.push_back(stmt(child));
    Exprnode_ptr tail;
    if (tail_context) tail = expr(tail_context);
    return Block_ptr{make_shared<BlockExpr>(nextId(), move(statements), move(tail))};
}
any ASTbuilder::visitStatement(RxParser::StatementContext* ctx) {
    if (ctx->letStatement()) return take<Stmtnode_ptr>(ctx->letStatement());
    if (ctx->expressionWithBlock()) {
        auto value = expr(ctx->expressionWithBlock());
        return Stmtnode_ptr{make_shared<ExprStmt>(nextId(), move(value), ctx->SEMI() != nullptr)};
    }
    if (ctx->statementExpression()) {
        auto value = expr(ctx->statementExpression());
        return Stmtnode_ptr{make_shared<ExprStmt>(nextId(), move(value), true)};
    }
    return Stmtnode_ptr{make_shared<EmptyStmt>(nextId())};
}
any ASTbuilder::visitExpressionWithBlock(RxParser::ExpressionWithBlockContext* ctx) {
    if (ctx->WHILE()) {
        auto condition = expr(ctx->conditionExpression());
        auto body = block(ctx->blockExpression());
        return Exprnode_ptr{make_shared<WhileExpr>(nextId(), move(condition), move(body))};
    }
    if (ctx->LOOP()) {
        auto body = block(ctx->blockExpression());
        return Exprnode_ptr{make_shared<LoopExpr>(nextId(), move(body))};
    }
    if (ctx->ifExpression()) return expr(ctx->ifExpression());
    return Exprnode_ptr{block(ctx->blockExpression())};
}
any ASTbuilder::visitIfExpression(RxParser::IfExpressionContext* ctx) {
    auto condition = expr(ctx->conditionExpression());
    auto branches = ctx->blockExpression();
    auto then_branch = block(branches.front());
    Exprnode_ptr else_branch;
    if (ctx->ifExpression()) else_branch = expr(ctx->ifExpression());
    else if (branches.size() == 2) else_branch = block(branches[1]);
    return Exprnode_ptr{make_shared<IfExpr>(nextId(), move(condition), move(then_branch), move(else_branch))};
}
any ASTbuilder::visitExpression(RxParser::ExpressionContext* ctx) {return expr(ctx->assignmentExpression());}
any ASTbuilder::visitAssignmentExpression(RxParser::AssignmentExpressionContext* ctx) {
     auto left = expr(ctx->logicalOrExpression());
    if (!ctx->assignmentOperator()) return left;
    auto op = take<AssignOp>(ctx->assignmentOperator());
    auto right = expr(ctx->expression()); 
    return Exprnode_ptr{make_shared<AssignExpr>(nextId(), op, move(left), move(right))};
}

any ASTbuilder::visitLogicalOrExpression(RxParser::LogicalOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitLogicalAndExpression(RxParser::LogicalAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitComparisonExpression(RxParser::ComparisonExpressionContext* ctx) {return buildComparison(ctx);}
any ASTbuilder::visitBitOrExpression(RxParser::BitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitClosedBitOrExpression(RxParser::ClosedBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitBitXorExpression(RxParser::BitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitClosedBitXorExpression(RxParser::ClosedBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitBitAndExpression(RxParser::BitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitClosedBitAndExpression(RxParser::ClosedBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitShiftExpression(RxParser::ShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitClosedShiftExpression(RxParser::ClosedShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitAdditiveExpression(RxParser::AdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitClosedAdditiveExpression(RxParser::ClosedAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitMultiplicativeExpression(RxParser::MultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitClosedMultiplicativeExpression(RxParser::ClosedMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}

any ASTbuilder::visitCastExpression(RxParser::CastExpressionContext* ctx) {
    auto value = expr(ctx->unaryExpression());
    for (auto* child : ctx->typeRef()) {
        auto target = type(child);
        value = make_shared<CastExpr>(nextId(), move(value), move(target));
    }
    return value;
}
any ASTbuilder::visitClosedCastExpression(RxParser::ClosedCastExpressionContext* ctx) {
    if (ctx->unaryExpression()) return expr(ctx->unaryExpression());
    auto value = expr(ctx->castExpression());
    auto target = type(ctx->closedCastType());
    return Exprnode_ptr{make_shared<CastExpr>(nextId(), move(value), move(target))};
}
any ASTbuilder::visitUnaryExpression(RxParser::UnaryExpressionContext* ctx) {
    if (ctx->unaryOperator()) {
        auto plan = take<UnaryOperatorPlan>(ctx->unaryOperator());
        auto operand = expr(ctx->unaryExpression());
        return applyUnary(move(operand), plan);
    }
    return expr(ctx->postfixExpression());
}
any ASTbuilder::visitPostfixExpression(RxParser::PostfixExpressionContext* ctx) {
    auto value = expr(ctx->primaryExpression());
    for (auto* child : ctx->postfixSuffix()) {
        auto suffix = take<PostfixPlan>(child);
        value = applySuffix(move(value), move(suffix));
    }
    return value;
}
any ASTbuilder::visitConditionExpression(RxParser::ConditionExpressionContext* ctx) {return expr(ctx->conditionAssignmentExpression());}
any ASTbuilder::visitConditionAssignmentExpression(RxParser::ConditionAssignmentExpressionContext* ctx) {
    auto left = expr(ctx->conditionLogicalOrExpression());
    if (!ctx->assignmentOperator()) return left;
    auto op = take<AssignOp>(ctx->assignmentOperator());
    auto right = expr(ctx->conditionExpression());
    return Exprnode_ptr{make_shared<AssignExpr>(nextId(), op, move(left), move(right))};
}

any ASTbuilder::visitConditionLogicalOrExpression(RxParser::ConditionLogicalOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionLogicalAndExpression(RxParser::ConditionLogicalAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionComparisonExpression(RxParser::ConditionComparisonExpressionContext* ctx) {return buildComparison(ctx);}
any ASTbuilder::visitConditionBitOrExpression(RxParser::ConditionBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionClosedBitOrExpression(RxParser::ConditionClosedBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBitXorExpression(RxParser::ConditionBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionClosedBitXorExpression(RxParser::ConditionClosedBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBitAndExpression(RxParser::ConditionBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionClosedBitAndExpression(RxParser::ConditionClosedBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionShiftExpression(RxParser::ConditionShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionClosedShiftExpression(RxParser::ConditionClosedShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionAdditiveExpression(RxParser::ConditionAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionClosedAdditiveExpression(RxParser::ConditionClosedAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionMultiplicativeExpression(RxParser::ConditionMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionClosedMultiplicativeExpression(RxParser::ConditionClosedMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}

any ASTbuilder::visitConditionCastExpression(RxParser::ConditionCastExpressionContext* ctx) {
    auto value = expr(ctx->conditionUnaryExpression());
    for (auto* child : ctx->typeRef()) {
        auto target = type(child);
        value = make_shared<CastExpr>(nextId(), move(value), move(target));
    }
    return value;
}
any ASTbuilder::visitConditionClosedCastExpression(RxParser::ConditionClosedCastExpressionContext* ctx) {
    if (ctx->conditionUnaryExpression()) return expr(ctx->conditionUnaryExpression());
    auto value = expr(ctx->conditionCastExpression());
    auto target = type(ctx->closedCastType());
    return Exprnode_ptr{make_shared<CastExpr>(nextId(), move(value), move(target))};
}
any ASTbuilder::visitConditionUnaryExpression(RxParser::ConditionUnaryExpressionContext* ctx) {
    if (ctx->unaryOperator()) {
        auto plan = take<UnaryOperatorPlan>(ctx->unaryOperator());
        auto operand = expr(ctx->conditionUnaryExpression());
        return applyUnary(move(operand), plan);
    }
    return expr(ctx->conditionPostfixExpression());
}
any ASTbuilder::visitConditionPostfixExpression(RxParser::ConditionPostfixExpressionContext* ctx) {
    auto value = expr(ctx->conditionPrimary());
    for (auto* child : ctx->postfixSuffix()) {
        auto suffix = take<PostfixPlan>(child);
        value = applySuffix(move(value), move(suffix));
    }
    return value;
}
any ASTbuilder::visitConditionBreakExpression(RxParser::ConditionBreakExpressionContext* ctx) {return expr(ctx->conditionBreakAssignmentExpression());}
any ASTbuilder::visitConditionBreakAssignmentExpression(RxParser::ConditionBreakAssignmentExpressionContext* ctx) {
    auto left = expr(ctx->conditionBreakLogicalOrExpression());
    if (!ctx->assignmentOperator()) return left;
    auto op = take<AssignOp>(ctx->assignmentOperator());
    auto right = expr(ctx->conditionExpression());
    return Exprnode_ptr{make_shared<AssignExpr>(nextId(), op, move(left), move(right))};
}
any ASTbuilder::visitConditionBreakLogicalOrExpression(RxParser::ConditionBreakLogicalOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakLogicalAndExpression(RxParser::ConditionBreakLogicalAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakComparisonExpression(RxParser::ConditionBreakComparisonExpressionContext* ctx) {return buildComparison(ctx);}
any ASTbuilder::visitConditionBreakBitOrExpression(RxParser::ConditionBreakBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakClosedBitOrExpression(RxParser::ConditionBreakClosedBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakBitXorExpression(RxParser::ConditionBreakBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakClosedBitXorExpression(RxParser::ConditionBreakClosedBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakBitAndExpression(RxParser::ConditionBreakBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakClosedBitAndExpression(RxParser::ConditionBreakClosedBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakShiftExpression(RxParser::ConditionBreakShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakClosedShiftExpression(RxParser::ConditionBreakClosedShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakAdditiveExpression(RxParser::ConditionBreakAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakClosedAdditiveExpression(RxParser::ConditionBreakClosedAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakMultiplicativeExpression(RxParser::ConditionBreakMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitConditionBreakClosedMultiplicativeExpression(RxParser::ConditionBreakClosedMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}

any ASTbuilder::visitConditionBreakCastExpression(RxParser::ConditionBreakCastExpressionContext* ctx) {
    auto value = expr(ctx->conditionBreakUnaryExpression());
    for (auto* child : ctx->typeRef()) {
        auto target = type(child);
        value = make_shared<CastExpr>(nextId(), move(value), move(target));
    }
    return value;
}
any ASTbuilder::visitConditionBreakClosedCastExpression(RxParser::ConditionBreakClosedCastExpressionContext* ctx) {
    if (ctx->conditionBreakUnaryExpression()) return expr(ctx->conditionBreakUnaryExpression());
    auto value = expr(ctx->conditionBreakCastExpression());
    auto target = type(ctx->closedCastType());
    return Exprnode_ptr{make_shared<CastExpr>(nextId(), move(value), move(target))};
}
any ASTbuilder::visitConditionBreakUnaryExpression(RxParser::ConditionBreakUnaryExpressionContext* ctx) {
    if (ctx->unaryOperator()) {
        auto plan = take<UnaryOperatorPlan>(ctx->unaryOperator());
        auto operand = expr(ctx->conditionUnaryExpression());
        return applyUnary(move(operand), plan);
    }
    return expr(ctx->conditionBreakPostfixExpression());
}
any ASTbuilder::visitConditionBreakPostfixExpression(RxParser::ConditionBreakPostfixExpressionContext* ctx) {
    auto value = expr(ctx->conditionPrimaryWithoutBareBlock());
    for (auto* child : ctx->postfixSuffix()) {
        auto suffix = take<PostfixPlan>(child);
        value = applySuffix(move(value), move(suffix));
    }
    return value;
}
any ASTbuilder::visitStatementExpression(RxParser::StatementExpressionContext* ctx) {return expr(ctx->statementAssignmentExpression());}
any ASTbuilder::visitStatementAssignmentExpression(RxParser::StatementAssignmentExpressionContext* ctx) {
    auto left = expr(ctx->statementLogicalOrExpression());
    if (!ctx->assignmentOperator()) return left;
    auto op = take<AssignOp>(ctx->assignmentOperator());
    auto right = expr(ctx->expression());
    return Exprnode_ptr{make_shared<AssignExpr>(nextId(), op, move(left), move(right))};
}
any ASTbuilder::visitStatementLogicalOrExpression(RxParser::StatementLogicalOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementLogicalAndExpression(RxParser::StatementLogicalAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementComparisonExpression(RxParser::StatementComparisonExpressionContext* ctx) {return buildComparison(ctx);}
any ASTbuilder::visitStatementBitOrExpression(RxParser::StatementBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementClosedBitOrExpression(RxParser::StatementClosedBitOrExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementBitXorExpression(RxParser::StatementBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementClosedBitXorExpression(RxParser::StatementClosedBitXorExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementBitAndExpression(RxParser::StatementBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementClosedBitAndExpression(RxParser::StatementClosedBitAndExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementShiftExpression(RxParser::StatementShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementClosedShiftExpression(RxParser::StatementClosedShiftExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementAdditiveExpression(RxParser::StatementAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementClosedAdditiveExpression(RxParser::StatementClosedAdditiveExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementMultiplicativeExpression(RxParser::StatementMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}
any ASTbuilder::visitStatementClosedMultiplicativeExpression(RxParser::StatementClosedMultiplicativeExpressionContext* ctx) {return foldBinary(ctx);}

any ASTbuilder::visitStatementCastExpression(RxParser::StatementCastExpressionContext* ctx) {
    auto value = expr(ctx->statementUnaryExpression());
    for (auto* child : ctx->typeRef()) {
        auto target = type(child);
        value = make_shared<CastExpr>(nextId(), move(value), move(target));
    }
    return value;
}
any ASTbuilder::visitStatementClosedCastExpression(RxParser::StatementClosedCastExpressionContext* ctx) {
    if (ctx->statementUnaryExpression()) return expr(ctx->statementUnaryExpression());
    auto value = expr(ctx->statementCastExpression());
    auto target = type(ctx->closedCastType());
    return Exprnode_ptr{make_shared<CastExpr>(nextId(), move(value), move(target))};
}
any ASTbuilder::visitStatementUnaryExpression(RxParser::StatementUnaryExpressionContext* ctx) {
    if (ctx->unaryOperator()) {
        auto plan = take<UnaryOperatorPlan>(ctx->unaryOperator());
        auto operand = expr(ctx->unaryExpression());
        return applyUnary(move(operand), plan);
    }
    return expr(ctx->statementPostfixExpression());
}
any ASTbuilder::visitStatementPostfixExpression(RxParser::StatementPostfixExpressionContext* ctx) {
    Exprnode_ptr value;
    if (ctx->nonBlockPrimary()) value = expr(ctx->nonBlockPrimary());
    else {
        value = expr(ctx->expressionWithBlock());
        auto first = take<PostfixPlan>(ctx->dotSuffix());
        value = applySuffix(move(value), move(first));
    }
    for (auto* child : ctx->postfixSuffix()) {
        auto suffix = take<PostfixPlan>(child);
        value = applySuffix(move(value), move(suffix));
    }
    return value;
}
any ASTbuilder::visitPrimaryExpression(RxParser::PrimaryExpressionContext* ctx) {
    if (ctx->nonBlockPrimary()) return expr(ctx->nonBlockPrimary());
    return expr(ctx->expressionWithBlock());
}
any ASTbuilder::visitNonBlockPrimary(RxParser::NonBlockPrimaryContext* ctx) {
    if (ctx->BREAK()) {
        Exprnode_ptr value;
        if (ctx->expression()) value = expr(ctx->expression());
        return Exprnode_ptr{make_shared<BreakExpr>(nextId(), move(value))};
    }
    if (ctx->RETURN()) {
        Exprnode_ptr value;
        if (ctx->expression()) value = expr(ctx->expression());
        return Exprnode_ptr{make_shared<ReturnExpr>(nextId(), move(value))};
    }
    if (ctx->CONTINUE()) return Exprnode_ptr{make_shared<ContinueExpr>(nextId())};
    if (ctx->literalExpression()) return expr(ctx->literalExpression());
    if (ctx->pathInExpression()) {
        auto path = take<Path>(ctx->pathInExpression());
        if (ctx->LBRACE()) {
            vector<FieldInitializer> fields;
            if (ctx->structExprFields()) fields = take<vector<FieldInitializer>>(ctx->structExprFields());
            return Exprnode_ptr{make_shared<StructExpr>(nextId(), move(path), move(fields))};
        }
        return Exprnode_ptr{make_shared<PathExpr>(nextId(), move(path))};
    }
    if (ctx->LPAREN()) {
        if (ctx->expression()) return expr(ctx->expression());
        return Exprnode_ptr{make_shared<UnitExpr>(nextId())};
    }
    return expr(ctx->arrayExpression());
}
any ASTbuilder::visitConditionPrimary(RxParser::ConditionPrimaryContext* ctx) {
    if (ctx->conditionPrimaryWithoutBareBlock()) return expr(ctx->conditionPrimaryWithoutBareBlock());
    return Exprnode_ptr{block(ctx->blockExpression())};
}
any ASTbuilder::visitConditionPrimaryWithoutBareBlock(RxParser::ConditionPrimaryWithoutBareBlockContext* ctx) {
    if (ctx->BREAK()) {
        Exprnode_ptr value;
        if (ctx->conditionBreakExpression()) value = expr(ctx->conditionBreakExpression());
        return Exprnode_ptr{make_shared<BreakExpr>(nextId(), move(value))};
    }
    if (ctx->RETURN()) {
        Exprnode_ptr value;
        if (ctx->conditionExpression()) value = expr(ctx->conditionExpression());
        return Exprnode_ptr{make_shared<ReturnExpr>(nextId(), move(value))};
    }
    if (ctx->CONTINUE()) return Exprnode_ptr{make_shared<ContinueExpr>(nextId())};
    if (ctx->WHILE()) {
        auto condition = expr(ctx->conditionExpression());
        auto body = block(ctx->blockExpression());
        return Exprnode_ptr{make_shared<WhileExpr>(nextId(), move(condition), move(body))};
    }
    if (ctx->LOOP()) {
        auto body = block(ctx->blockExpression());
        return Exprnode_ptr{make_shared<LoopExpr>(nextId(), move(body))};
    }
    if (ctx->ifExpression()) return expr(ctx->ifExpression());
    if (ctx->literalExpression()) return expr(ctx->literalExpression());
    if (ctx->pathInExpression()) {
        auto path = take<Path>(ctx->pathInExpression());
        return Exprnode_ptr{make_shared<PathExpr>(nextId(), move(path))};
    }
    if (ctx->LPAREN()) {
        if (ctx->expression()) return expr(ctx->expression());
        return Exprnode_ptr{make_shared<UnitExpr>(nextId())};
    }
    return expr(ctx->arrayExpression());
}
any ASTbuilder::visitLiteralExpression(RxParser::LiteralExpressionContext* ctx) {
    if (ctx->INTEGER_LITERAL()) return makeInteger(ctx->INTEGER_LITERAL()->getText());
    return Exprnode_ptr{make_shared<BoolLiteral>(nextId(), ctx->TRUE() != nullptr)};
}
any ASTbuilder::visitStructExprFields(RxParser::StructExprFieldsContext* ctx) {
    vector<FieldInitializer> result;
    for (auto* child : ctx->structExprField()) result.push_back(take<FieldInitializer>(child));
    return result;
}
any ASTbuilder::visitStructExprField(RxParser::StructExprFieldContext* ctx) {
    auto name = take<Identifier>(ctx->identifier());
    auto value = expr(ctx->expression());
    return FieldInitializer{move(name), move(value)};
}
any ASTbuilder::visitArrayExpression(RxParser::ArrayExpressionContext* ctx) {
    auto children = ctx->expression();
    if (ctx->SEMI()) {
        auto value = expr(children.front());
        auto count = expr(ctx->constValue());
        return Exprnode_ptr{make_shared<ArrayRepeatExpr>(nextId(), move(value), move(count))};
    }
    vector<Exprnode_ptr> elements;
    for (auto* child : children) elements.push_back(expr(child));
    return Exprnode_ptr{make_shared<ArrayExpr>(nextId(), move(elements))};
}
any ASTbuilder::visitPostfixSuffix(RxParser::PostfixSuffixContext* ctx) {
    if (ctx->callArguments()) return PostfixPlan{CallSuffix{take<vector<Exprnode_ptr>>(ctx->callArguments())}};
    if (ctx->LBRACKET()) return PostfixPlan{IndexSuffix{expr(ctx->expression())}};
    return take<PostfixPlan>(ctx->dotSuffix());
}
any ASTbuilder::visitDotSuffix(RxParser::DotSuffixContext* ctx) {
    if (ctx->callArguments()) {
        auto method = take<PathSegment>(ctx->pathExprSegment());
        auto arguments = take<vector<Exprnode_ptr>>(ctx->callArguments());
        return PostfixPlan{MethodSuffix{move(method), move(arguments)}};
    }
    return PostfixPlan{FieldSuffix{take<Identifier>(ctx->identifier())}};
}
any ASTbuilder::visitCallArguments(RxParser::CallArgumentsContext* ctx) {
    vector<Exprnode_ptr> result;
    for (auto* child : ctx->expression()) result.push_back(expr(child));
    return result;
}
any ASTbuilder::visitUnaryOperator(RxParser::UnaryOperatorContext* ctx) {
    if (ctx->MINUS()) return UnaryOperatorPlan{UnaryOp::Negate};
    if (ctx->NOT()) return UnaryOperatorPlan{UnaryOp::Not};
    if (ctx->STAR()) return UnaryOperatorPlan{UnaryOp::Dereference};
    auto inner = ctx->MUT() ? UnaryOp::BorrowMutable : UnaryOp::BorrowShared;
    if (ctx->ANDAND()) return UnaryOperatorPlan{UnaryOp::BorrowShared, inner};
    return UnaryOperatorPlan{inner};
}
any ASTbuilder::visitMultiplicativeOperator(RxParser::MultiplicativeOperatorContext* ctx) {
    if (ctx->STAR()) return BinaryOp::Multiply;
    if (ctx->SLASH()) return BinaryOp::Divide;
    return BinaryOp::Remainder;
}
any ASTbuilder::visitAdditiveOperator(RxParser::AdditiveOperatorContext* ctx) {
    if (ctx->PLUS()) return BinaryOp::Add;
    return BinaryOp::Subtract;
}
any ASTbuilder::visitShiftRight(RxParser::ShiftRightContext* ctx) {return BinaryOp::ShiftRight;}
any ASTbuilder::visitComparisonExceptLt(RxParser::ComparisonExceptLtContext* ctx) {
    auto spelling = ctx->getText();
    if (spelling == "==") return BinaryOp::Equal;
    if (spelling == "!=") return BinaryOp::NotEqual;
    if (spelling == "<=") return BinaryOp::LessEqual;
    if (spelling == ">=") return BinaryOp::GreaterEqual;
    return BinaryOp::Greater;
}
any ASTbuilder::visitAssignmentOperator(RxParser::AssignmentOperatorContext* ctx) {
    if (ctx->equalsSign()) return take<AssignOp>(ctx->equalsSign());
    auto spelling = ctx->getText();
    if (spelling == "+=") return AssignOp::Add;
    if (spelling == "-=") return AssignOp::Subtract;
    if (spelling == "*=") return AssignOp::Multiply;
    if (spelling == "/=") return AssignOp::Divide;
    if (spelling == "%=") return AssignOp::Remainder;
    if (spelling == "&=") return AssignOp::BitAnd;
    if (spelling == "|=") return AssignOp::BitOr;
    if (spelling == "^=") return AssignOp::BitXor;
    if (spelling == "<<=") return AssignOp::ShiftLeft;
    return AssignOp::ShiftRight;
}
any ASTbuilder::visitEqualsSign(RxParser::EqualsSignContext* ctx) {return AssignOp::Assign;}
any ASTbuilder::visitIdentifier(RxParser::IdentifierContext* ctx) {return Identifier{ctx->getText()};}

}