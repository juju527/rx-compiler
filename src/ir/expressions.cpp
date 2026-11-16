#include "lower.hpp"

namespace rx::IR {

using namespace AST;

EvalResult Lowerer::lowerExpr(const Exprnode& node) {
    const auto& metadata = exprInfo(node);
    if (!metadata.use) fail("missing use plan for node " + std::to_string(node.id));
    const auto& plan = *metadata.use;
    auto result = lowerExprCore(node);
    if (!result.normal()) return result;

    for (auto step : plan.adjustments) {
        switch (step) {
        case semantic::Adjustment::Dereference:
        case semantic::Adjustment::DereferenceBox: {
            auto expected = step == semantic::Adjustment::Dereference ? semantic::TypeKind::Reference : semantic::TypeKind::Box;
            if (sourceType(result.type()).kind != expected) fail("adjustment dereference type mismatch");
            result = dereference(result);
            break;
        }
        case semantic::Adjustment::BorrowShared:
        case semantic::Adjustment::BorrowMutable:
        case semantic::Adjustment::ReborrowMutable: {
            auto place = materialize(result);
            auto target = sourceType(plan.target);
            if (target.kind != semantic::TypeKind::Reference || target.element != place.type) fail("borrow target mismatch");
            result = Value::direct(plan.target, place.address);
            break;
        }
        case semantic::Adjustment::NeverToTarget: fail("never adjustment on a live path");
        }
    }
    if (result.type() != plan.target) fail("use plan target mismatch");
    switch (plan.mode) {
    case semantic::UseMode::Copy:
    case semantic::UseMode::Move:
        return snapshot(result);
    case semantic::UseMode::InspectPlace:
    case semantic::UseMode::BorrowShared:
    case semantic::UseMode::BorrowMutable:
    case semantic::UseMode::Discard:
        return result;
    case semantic::UseMode::Unreachable:
        fail("unreachable use on live path");
    default:
        fail("unknown use mode");
    }
}

EvalResult Lowerer::lowerExprCore(const Exprnode& node) {
    if (!builder().live()) return EvalResult::terminated();
    const auto& metadata = exprInfo(node);
    if (metadata.category == semantic::Category::Place) {
        if (node.kind == nodeKind::PathExpr && !metadata.place) fail("place path missing");
        if (node.kind != nodeKind::PathExpr && metadata.place) fail("place path on non-path");
        return lowerPlaceExpr(node);
    }
    if (metadata.category == semantic::Category::Callable) fail("callable is not a runtime value");
    if (metadata.constant_value) {
        if (metadata.constant_value->type != metadata.type) fail("constant type mismatch");
        return Value::direct(metadata.type, constant(valueReprType(metadata.type), metadata.constant_value->bits));
    }
    return SyntaxVisitor::dispatch(node);
}

EvalResult Lowerer::visit(const IntLiteral& node) {fail("unsupported expression " + node_kind_to_string(node.kind));}
EvalResult Lowerer::visit(const PathExpr& node) {fail("unsupported expression " + node_kind_to_string(node.kind));}
EvalResult Lowerer::visit(const FieldExpr& node) {fail("unsupported expression " + node_kind_to_string(node.kind));}
EvalResult Lowerer::visit(const IndexExpr& node) {fail("unsupported expression " + node_kind_to_string(node.kind));}

EvalResult Lowerer::visit(const BoolLiteral& node) {return Value::direct(semantic::boolean, constant(I1, node.value));}
EvalResult Lowerer::visit(const UnitExpr&) {return Value{};}

EvalResult Lowerer::visit(const CastExpr& node) {
    const auto& metadata = exprInfo(node);
    auto result = lowerExpr(*node.value);
    if (!result.normal()) return result;
    auto value = snapshot(result);
    auto operand = value.directOperand();
    if (value.type() == semantic::boolean) operand = builder().convert(Opcode::ZExt, operand, I32);
    return Value::direct(metadata.type, operand);
}

EvalResult Lowerer::lowerStatement(const Stmtnode& node) {return SyntaxVisitor::dispatch(node);}
EvalResult Lowerer::visit(const LetStmt& binding) {
    auto initializer = lowerExpr(*binding.initializer);
    if (!initializer.normal()) return initializer;
    auto id = semantic.bindings.at(binding.id);
    const auto& local = semantic.locals.at(id);
    auto slot = createStorageSlot(local.type);
    storeValue(slot, snapshot(initializer));
    if (!context().locals.emplace(id, slot).second) fail("duplicate LocalId in function");
    return Value{};
}
EvalResult Lowerer::visit(const ExprStmt& node) {return lowerExpr(*node.expression);}
EvalResult Lowerer::visit(const EmptyStmt& node) {return Value{};}
EvalResult Lowerer::visit(const BlockExpr& node) {
    for (const auto& statement : node.statements) {
        auto result = lowerStatement(*statement);
        if (!result.normal()) return result;
    }
    if (!node.tail) return Value{};
    auto tail = lowerExpr(*node.tail);
    if (!tail.normal()) return tail;
    return snapshot(tail);
}

Operand Lowerer::emitScalarBinaryOp(BinaryOp operation, SourceTypeId type, Operand left, Operand right) {
    const bool signed_integer = semantic::Types::signedInteger(type);
    auto binary = [&](Opcode opcode) { return builder().binary(opcode, left, right); };
    auto compare = [&](Predicate predicate) { return builder().compare(predicate, left, right); };
    switch (operation) {
    case BinaryOp::Add: return binary(Opcode::Add);
    case BinaryOp::Subtract: return binary(Opcode::Sub);
    case BinaryOp::Multiply: return binary(Opcode::Mul);
    case BinaryOp::Divide: return binary(signed_integer ? Opcode::SDiv : Opcode::UDiv);
    case BinaryOp::Remainder: return binary(signed_integer ? Opcode::SRem : Opcode::URem);
    case BinaryOp::BitAnd: return binary(Opcode::And);
    case BinaryOp::BitOr: return binary(Opcode::Or);
    case BinaryOp::BitXor: return binary(Opcode::Xor);
    case BinaryOp::ShiftLeft:
    case BinaryOp::ShiftRight:
        right = builder().binary(Opcode::And, right, constant(I32, 31));
        if (operation == BinaryOp::ShiftLeft) return binary(Opcode::Shl);
        return binary(signed_integer ? Opcode::AShr : Opcode::LShr);
    case BinaryOp::Less: return compare(signed_integer ? Predicate::SLT : Predicate::ULT);
    case BinaryOp::LessEqual: return compare(signed_integer ? Predicate::SLE : Predicate::ULE);
    case BinaryOp::Greater: return compare(signed_integer ? Predicate::SGT : Predicate::UGT);
    case BinaryOp::GreaterEqual: return compare(signed_integer ? Predicate::SGE : Predicate::UGE);
    default: fail("unsupported scalar operation");
    }
}

EvalResult Lowerer::visit(const UnaryExpr& node) {
    auto result = lowerExpr(*node.operand);
    if (!result.normal()) return result;
    if (node.op == UnaryOp::BorrowShared || node.op == UnaryOp::BorrowMutable) return result;
    const auto& operation = exprInfo(node).operation;
    if (!operation) fail("missing unary operator plan");
    if (operation->read_left_reference) result = dereference(result);
    auto value = snapshot(result);
    auto operand = value.directOperand();
    if (node.op == UnaryOp::Negate) operand = builder().binary(Opcode::Sub, constant(I32, 0), operand);
    else if (node.op == UnaryOp::Not) {
        auto mask = constant(operand.type, value.type() == semantic::boolean ? 1 : UINT32_MAX);
        operand = builder().binary(Opcode::Xor, operand, mask);
    }
    else fail("unexpected unary operator");
    return Value::direct(value.type(), operand);
}

EvalResult Lowerer::visit(const BinaryExpr& node) {
    // 短路运算。
    if (node.op == BinaryOp::LogicalAnd || node.op == BinaryOp::LogicalOr) return lowerShortCircuitExpr(node);
    
    auto left = lowerExpr(*node.left);
    if (!left.normal()) return left;
    auto right = lowerExpr(*node.right);
    if (!right.normal()) return right;
    if (!exprInfo(node).operation) fail("missing binary operator plan");
    const auto& operation = *exprInfo(node).operation;

    if (node.op == BinaryOp::Equal || node.op == BinaryOp::NotEqual) {
        if (operation.operand_type == semantic::unit) return Value::direct(semantic::boolean, constant(I1, node.op == BinaryOp::Equal));
        auto left_place = materialize(left);
        auto right_place = materialize(right);
        // 简单的设计，直接调用一个函数来比较两个值的地址。
        auto result = emitEqualityCall(operation.operand_type, left_place.address, right_place.address);
        if (node.op == BinaryOp::NotEqual) result = builder().binary(Opcode::Xor, result, constant(I1, 1));
        return Value::direct(semantic::boolean, result);
    }

    if (operation.read_left_reference) left = dereference(left);
    if (operation.read_right_reference) right = dereference(right);
    for (size_t i = 0; i < operation.ordering_reference_depth; i++) {
        left = dereference(left);
        right = dereference(right);
    }
    auto left_value = snapshot(left);
    auto right_value = snapshot(right);
    auto result = emitScalarBinaryOp(node.op, operation.operand_type, left_value.directOperand(), right_value.directOperand());
    return Value::direct(exprInfo(node).type, result);
}

EvalResult Lowerer::visit(const AssignExpr& node) {
    auto result = lowerExpr(*node.value);
    if (!result.normal()) return result;
    auto right = snapshot(result);
    auto destination = lowerPlaceExpr(*node.target);
    if (!destination.normal()) return destination;
    if (node.op == AssignOp::Assign) {
        if (destination.isPlace()) storeValue(destination.place(), right);
        else storeValue(StorageSlot{destination.type()}, right);
        return Value{};
    }

    if (!exprInfo(node).operation) fail("missing compound assignment plan");
    const auto& operation = *exprInfo(node).operation;
    auto left = loadScalar(destination.place());
    if (operation.read_right_reference) right = loadScalar(dereference(right));
    BinaryOp binary;
    switch (node.op) {
    case AssignOp::Add: binary = BinaryOp::Add; break;
    case AssignOp::Subtract: binary = BinaryOp::Subtract; break;
    case AssignOp::Multiply: binary = BinaryOp::Multiply; break;
    case AssignOp::Divide: binary = BinaryOp::Divide; break;
    case AssignOp::Remainder: binary = BinaryOp::Remainder; break;
    case AssignOp::BitAnd: binary = BinaryOp::BitAnd; break;
    case AssignOp::BitOr: binary = BinaryOp::BitOr; break;
    case AssignOp::BitXor: binary = BinaryOp::BitXor; break;
    case AssignOp::ShiftLeft: binary = BinaryOp::ShiftLeft; break;
    case AssignOp::ShiftRight: binary = BinaryOp::ShiftRight; break;
    default: fail("unknown compound operator");
    }
    auto operand = emitScalarBinaryOp(binary, operation.operand_type, left.directOperand(), right.directOperand());
    storeValue(destination.place(), Value::direct(destination.type(), operand));
    return Value{};
}

EvalResult Lowerer::visit(const ArrayExpr& node) {return lowerAggregateExpr(node);}
EvalResult Lowerer::visit(const ArrayRepeatExpr& node) {return lowerAggregateExpr(node);}
EvalResult Lowerer::visit(const StructExpr& node) {return lowerAggregateExpr(node);}

EvalResult Lowerer::lowerAggregateExpr(const Exprnode& node) {
    const auto& metadata = exprInfo(node);
    if (!metadata.flow.normal) {
        auto evaluate = [&](const Exprnode& expression) { return lowerExpr(expression).normal(); };
        if (node.kind == nodeKind::ArrayExpr) {
            for (const auto& element : static_cast<const ArrayExpr&>(node).elements)
                if (!evaluate(*element)) return EvalResult::terminated();
        }
        else if (node.kind == nodeKind::ArrayRepeatExpr) {
            if (!evaluate(*static_cast<const ArrayRepeatExpr&>(node).value)) return EvalResult::terminated();
        }
        else {
            for (const auto& field : static_cast<const StructExpr&>(node).fields)
                if (!evaluate(*field.value)) return EvalResult::terminated();
        }
        fail("nonreturning initializer remained live");
    }

    auto result = createStorageSlot(metadata.type);
    if (node.kind == nodeKind::ArrayExpr) {
        const auto& array = static_cast<const ArrayExpr&>(node);
        for (size_t i = 0; i < array.elements.size(); i++) {
            auto value = lowerExpr(*array.elements[i]);
            if (!value.normal()) return value;
            storeValue(projectElement(result.place(), constant(I32, uint32_t(i))), snapshot(value));
        }
    }
    else if (node.kind == nodeKind::ArrayRepeatExpr) {
        const auto& repeat = static_cast<const ArrayRepeatExpr&>(node);
        auto initializer = lowerExpr(*repeat.value);
        if (!initializer.normal()) return initializer;
        auto value = snapshot(initializer);
        auto length = sourceType(metadata.type).length;
        emitIndexLoop(constant(I32, length), [&](Operand index) {storeValue(projectElement(result.place(), index), value);});
    }
    else {
        const auto& structure = static_cast<const StructExpr&>(node);
        for (size_t i = 0; i < structure.fields.size(); i++) {
            auto value = lowerExpr(*structure.fields[i].value);
            if (!value.normal()) return value;
            auto destination = projectField(result.place(), metadata.field_order->at(i));
            storeValue(destination, snapshot(value));
        }
    }
    return valueFromResultSlot(result);
}

}