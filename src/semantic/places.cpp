#include "checker.hpp"

using std::to_string;

namespace rx::semantic {

// 临时物化。
ExprInfo Checker::materialize(ExprInfo info, NodeId node) {
    if (info.category == Category::Callable) fail(node, "a function cannot be used as a stored value");
    if (info.category == Category::Place) return info;
    result.expressions.at(node).temporary_type = info.type;
    info.category = Category::Place;
    info.place = {};info.place.root = node;
    info.place.temporary = true;info.place.writable = true;
    return info;
}

bool Checker::canDereference(TypeId type) const {
    auto kind = result.types[type].kind;
    return kind == TypeKind::Reference || kind == TypeKind::Box;
}

ExprInfo Checker::dereference(ExprInfo info, NodeId node) {
    const auto type = result.types[info.type];
    if (!canDereference(info.type)) fail(node, "cannot dereference");
    info = materialize(move(info), node);
    info.type = type.element;
    if (type.kind == TypeKind::Reference) {
        info.place.shared_barrier = info.place.shared_barrier || !type.mutable_reference;
        info.place.writable = type.mutable_reference && !info.place.shared_barrier;
        info.place.path.push_back({ProjectionKind::Dereference, node});
    } 
    else info.place.path.push_back({ProjectionKind::DereferenceBox, node});
    // 解引用只继承位置与控制流，不继承来源表达式的名称、调用、常量等信息。
    auto res = value(info.type, move(info.flow));
    res.category = Category::Place;
    res.place = move(info.place);
    return res;
}

void Checker::requireMutable(const ExprInfo& info, NodeId node) {
    if (info.category != Category::Place)
        fail(node, "assignment requires a place expression");
    if (!info.place.writable || info.place.shared_barrier)
        fail(node, "this place does not permit mutable access");
    for (const auto& gate : info.place.vec_gates)
        if (!gate.mutable_access)
            fail(node, "mutable access requires a mutable vector at indexing node " + to_string(gate.index));
    return ;
}

ExprInfo Checker::field(const FieldExpr& node) {
    auto base = expression(*node.base);
    while (canDereference(base.type)) base = dereference(move(base), node.base->id);// 语法糖。
    const auto type = result.types[base.type];
    if (type.kind != TypeKind::Struct) fail(node.id, "field access requires a struct");
    const auto& s = result.structs.at(type.declaration);
    auto found = s.field_indices.find(node.field);
    if (found == s.field_indices.end()) fail(node.id, "unknown field '" + node.field + "' on " + s.name);
    base = materialize(move(base), node.base->id);
    base.type = s.fields.at(found->second).type;
    base.place.path.push_back({ProjectionKind::Field, node.id, found->second});
    auto res = value(base.type, move(base.flow));
    res.category = Category::Place;
    res.place = move(base.place);
    return res;
}

ExprInfo Checker::index(const IndexExpr& node) {
    auto base = expression(*node.base);
    while (canDereference(base.type)) base = dereference(move(base), node.base->id);// 语法糖。
    const auto type = result.types[base.type];
    if (type.kind != TypeKind::Array && type.kind != TypeKind::Vec) fail(node.id, "indexing requires an array or Vec");
    auto offset = consume(*node.index, usize);
    base = materialize(move(base), node.base->id);
    base.flow = sequence(move(base.flow), offset.flow);
    base.type = type.element;
    if (type.kind == TypeKind::Vec) {
        base.place.vec_gates.push_back({node.id, base.place.writable && !base.place.shared_barrier});
        base.place.path.push_back({ProjectionKind::VecIndex, node.id});
    }
    else base.place.path.push_back({ProjectionKind::ArrayIndex, node.id});
    auto res = value(base.type, move(base.flow));
    res.category = Category::Place;
    res.place = move(base.place);
    return res;
}

};