#include "checker.hpp"

using std::move;
using std::logic_error;

namespace rx::semantic {

using namespace rx::AST;

// 标量化操作数，至多去掉一层共享引用。
TypeId Checker::scalarOperand(const ExprInfo& info, NodeId node) const {
    auto type = info.type;
    const auto t = result.types[type];
    if (t.kind == TypeKind::Reference && !t.mutable_reference) type = t.element;
    if (!Types::scalar(type)) fail(node, "operator requires a scalar or one shared reference to a scalar");
    return type;
}

ExprInfo Checker::unary(const UnaryExpr& node) {
    ExprInfo operand;
    optional<Constant> constant_value;
    if (node.op == UnaryOp::Negate && node.operand->kind == nodeKind::IntLiteral) {
        const auto c = integerLiteral(static_cast<const IntLiteral&>(*node.operand), unknown, true);
        operand = value(c.type);
        operand.constant_value = Constant{c.type, uint32_t(0) - c.bits};
        result.expressions[node.operand->id] = operand;
        constant_value = c;
    }
    else operand = expression(*node.operand);
    if (node.op == UnaryOp::Dereference) {
        result.expressions.at(node.operand->id).use = UseInfo{operand.type, UseMode::InspectPlace, {}};
        return dereference(move(operand), node.operand->id);
    }
    if (node.op == UnaryOp::BorrowShared || node.op == UnaryOp::BorrowMutable) {
        const bool mut = node.op == UnaryOp::BorrowMutable;
        auto place = materialize(operand, node.operand->id);
        if (mut) requireMutable(place, node.operand->id);
        auto type = result.types.reference(operand.type, mut);
        result.expressions.at(node.operand->id).use = UseInfo{type, mut ? UseMode::BorrowMutable : UseMode::BorrowShared, {mut ? Adjustment::BorrowMutable : Adjustment::BorrowShared}};
        return value(type, move(operand.flow));
    }
    auto type = scalarOperand(operand, node.id);
    if (node.op == UnaryOp::Negate && !Types::signedInteger(type)) fail(node.id, "unary minus requires a signed integer");
    use(operand, node.operand->id);
    auto res = value(type, move(operand.flow));
    res.operation = OperatorInfo{type, result.types[operand.type].kind == TypeKind::Reference};
    res.constant_value = constant_value;
    return res;
}

OperatorInfo Checker::checkBinaryOperands(BinaryOp op, const ExprInfo& left, const ExprInfo& right, NodeId node) {
    if (op == BinaryOp::LogicalAnd || op == BinaryOp::LogicalOr) {
        if (left.type != boolean || right.type != boolean) fail(node, "lazy boolean operators require bool values");
        return {boolean};
    }
    if (op == BinaryOp::Equal || op == BinaryOp::NotEqual) {
        if (left.type != right.type) fail(node, "equality operands must have exactly the same source type");
        if (!capable(left.type, Capability::PartialEq)) fail(node, "equality requires PartialEq");
        return {left.type};
    }
    if (op == BinaryOp::Less || op == BinaryOp::LessEqual || op == BinaryOp::Greater || op == BinaryOp::GreaterEqual) {
        const auto l = result.types[left.type], r = result.types[right.type];
        bool same = left.type == right.type;
        bool outer_adjustment = l.kind == TypeKind::Reference && !l.mutable_reference;
        outer_adjustment &= (r.kind == TypeKind::Reference && r.mutable_reference && l.element == r.element);
        if (!same && !outer_adjustment) fail(node, "incompatible ordering operands");
        auto scalar = left.type;
        size_t depth = 0;
        while (result.types[scalar].kind == TypeKind::Reference) { scalar = result.types[scalar].element; ++depth; }
        if (!Types::scalar(scalar)) fail(node, "ordering requires matching scalars or references to scalars");
        return {scalar, false, false, depth};
    }
    auto l = scalarOperand(left, node), r = scalarOperand(right, node);
    const bool shift = op == BinaryOp::ShiftLeft || op == BinaryOp::ShiftRight;
    const bool bitwise = op == BinaryOp::BitAnd || op == BinaryOp::BitOr || op == BinaryOp::BitXor;
    if ((shift && (!Types::integer(l) || !Types::integer(r))) || (!shift && l != r) || (!shift && !bitwise && !Types::integer(l)))
        fail(node, "incompatible scalar operand types");
    return {l, result.types[left.type].kind == TypeKind::Reference, result.types[right.type].kind == TypeKind::Reference};
}

ExprInfo Checker::binary(const BinaryExpr& node) {
    auto left = expression(*node.left), right = expression(*node.right);
    const auto operation = checkBinaryOperands(node.op, left, right, node.id);
    const bool equality = node.op == BinaryOp::Equal || node.op == BinaryOp::NotEqual;
    const bool ordering = node.op == BinaryOp::Less || node.op == BinaryOp::LessEqual || node.op == BinaryOp::Greater || node.op == BinaryOp::GreaterEqual;
    const bool lazy = node.op == BinaryOp::LogicalAnd || node.op == BinaryOp::LogicalOr;
    
    if (equality) {
        // 相等比较隐式借用；&mut T 比较也不能变成 consuming move 或强制可变重借用。
        result.expressions.at(node.left->id).use = UseInfo{left.type, UseMode::BorrowShared, {}};
        result.expressions.at(node.right->id).use = UseInfo{right.type, UseMode::BorrowShared, {}};
    }
    else if (ordering) {
        result.expressions.at(node.left->id).use = UseInfo{left.type, UseMode::BorrowShared, {}};
        result.expressions.at(node.right->id).use = UseInfo{left.type, UseMode::BorrowShared, {}};
        if (left.type != right.type) result.expressions.at(node.right->id).use->adjustments = *coercion(right.type, left.type);
    }
    else {
        use(left, node.left->id);
        use(right, node.right->id);
    }

    auto flow = sequence(move(left.flow), lazy ? alternatives(move(right.flow), Flow{}) : right.flow);
    auto type = equality || ordering || lazy ? boolean : operation.operand_type;
    auto res = value(type, move(flow));
    res.operation = operation;
    return res;
}

ExprInfo Checker::assignment(const AssignExpr& node) {
    auto target = expression(*node.target);
    requireMutable(target, node.target->id);
    result.expressions.at(node.target->id).use = UseInfo{target.type, UseMode::InspectPlace, {}};
    if (node.op == AssignOp::Assign) {
        auto rhs = consume(*node.value, target.type);
        return value(unit, sequence(move(rhs.flow), target.flow));
    }
    if (!Types::scalar(target.type)) fail(node.id, "compound destination must have a primitive scalar type");
    auto rhs = expression(*node.value);
    BinaryOp operation;
    switch (node.op) {
    case AssignOp::Add: operation = BinaryOp::Add; break;
    case AssignOp::Subtract: operation = BinaryOp::Subtract; break;
    case AssignOp::Multiply: operation = BinaryOp::Multiply; break;
    case AssignOp::Divide: operation = BinaryOp::Divide; break;
    case AssignOp::Remainder: operation = BinaryOp::Remainder; break;
    case AssignOp::BitAnd: operation = BinaryOp::BitAnd; break;
    case AssignOp::BitOr: operation = BinaryOp::BitOr; break;
    case AssignOp::BitXor: operation = BinaryOp::BitXor; break;
    case AssignOp::ShiftLeft: operation = BinaryOp::ShiftLeft; break;
    case AssignOp::ShiftRight: operation = BinaryOp::ShiftRight; break;
    default: throw logic_error("unexpected assignment operator");
    }
    const auto operator_info = checkBinaryOperands(operation, target, rhs, node.id);
    use(rhs, node.value->id);
    auto res = value(unit, sequence(move(rhs.flow), target.flow));
    res.operation = operator_info;
    return res;
}

ExprInfo Checker::cast(const CastExpr& node) {
    auto operand = consume(*node.value);
    auto target = resolveType(*node.target_type, self_type);
    if (!Types::integer(target) || !Types::scalar(operand.type)) fail(node.id, "as supports only integer-to-integer and bool-to-integer conversions");
    auto result = value(target, move(operand.flow));
    result.operation = OperatorInfo{operand.type};
    return result;
}

};