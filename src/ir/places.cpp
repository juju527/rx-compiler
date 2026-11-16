#include "lower.hpp"

namespace rx::IR {

using namespace AST;

Place Lowerer::projectField(Place base, size_t index) {
    auto type = sourceType(base.type);
    if (type.kind != semantic::TypeKind::Struct) fail("source field on non-struct");
    auto field_type = semantic.structs.at(type.declaration).fields.at(index).type;
    auto address = builder().gep(storageLayout(base.type).storage, base.address, {constant(I32, 0), constant(I32, uint32_t(index))});
    return {field_type, address};
}

Operand Lowerer::vecFieldAddress(Place vector, VecField field) {
    if (sourceType(vector.type).kind != semantic::TypeKind::Vec) fail("Vec metadata on non-Vec");
    return builder().gep(storageLayout(vector.type).storage, vector.address, {constant(I32, 0), constant(I32, uint32_t(field))});
}

Place Lowerer::projectElement(Place base, Operand index) {
    auto type = sourceType(base.type);
    if (type.kind == semantic::TypeKind::Array) {
        auto address = builder().gep(storageLayout(base.type).storage, base.address, {constant(I32, 0), index});
        return {type.element, address};
    }
    if (type.kind == semantic::TypeKind::Vec) {
        auto data = builder().load(Ptr, vecFieldAddress(base, VecField::Data));
        return {type.element, builder().gep(storageLayout(type.element).storage, data, {index})};
    }
    fail("index on non-container");
}

Place Lowerer::dereference(const EvalResult& result) {
    auto type = sourceType(result.type());
    if (type.kind != semantic::TypeKind::Reference && type.kind != semantic::TypeKind::Box) fail("dereference contract");
    return {type.element, snapshot(result).directOperand()};
}

EvalResult Lowerer::lowerPlaceExpr(const Exprnode& node) {
    const auto& metadata = exprInfo(node);
    if (metadata.category != semantic::Category::Place) fail("expression is not a semantic place");
    const auto& plan = metadata.place;
    EvalResult result;
    if (plan.temporary) {
        const auto& root = *expr_nodes.at(plan.root);
        const auto& root_info = exprInfo(root);
        if (!root_info.temporary_type) fail("temporary root has no materialization type");
        result = lowerExprCore(root);
        if (!result.normal()) return result;
        if (result.type() != *root_info.temporary_type) fail("temporary type mismatch");
        result = materialize(result);
    }
    else {
        const auto& slot = context().locals.at(plan.root);
        if (slot.location) result = slot.place();
        else if (slot.type == semantic::unit) result = Value{};
        else fail("place root has no storage");
    }

    for (const auto& step : plan.path) {
        switch (step.kind) {
        case semantic::ProjectionKind::Dereference:
        case semantic::ProjectionKind::DereferenceBox: {
            auto expected = step.kind == semantic::ProjectionKind::Dereference ? semantic::TypeKind::Reference : semantic::TypeKind::Box;
            if (sourceType(result.type()).kind != expected) fail("projection dereference type mismatch");
            result = dereference(result);
            break;
        }
        case semantic::ProjectionKind::Field:
            result = projectField(result.place(), step.field);
            break;
        case semantic::ProjectionKind::ArrayIndex:
        case semantic::ProjectionKind::VecIndex: {
            const auto* index = dynamic_cast<const IndexExpr*>(expr_nodes.at(step.node));
            if (!index) fail("index projection must name an IndexExpr");
            auto expected = step.kind == semantic::ProjectionKind::ArrayIndex ? semantic::TypeKind::Array : semantic::TypeKind::Vec;
            if (sourceType(result.type()).kind != expected) fail("projection index type mismatch");
            auto offset = lowerExpr(*index->index);
            if (!offset.normal()) return offset;
            result = projectElement(result.place(), snapshot(offset).directOperand());
            break;
        }
        }
    }
    if (result.type() != metadata.type) fail("place final source type mismatch");
    return result;
}