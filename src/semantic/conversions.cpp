#include "checker.hpp"

using std::nullopt;

namespace rx::semantic {

using namespace rx::AST;

// 类型转换，生成调整链。
optional<vector<Adjustment>> Checker::coercion(TypeId source, TypeId target) const {
    vector<Adjustment> steps;
    if (source == never) {
        if (target != never) steps.push_back(Adjustment::NeverToTarget);
        return steps;
    }
    const auto from = result.types[source], to = result.types[target];
    if (source == target && !(from.kind == TypeKind::Reference && from.mutable_reference)) return steps;
    if (from.kind != TypeKind::Reference || to.kind != TypeKind::Reference) return nullopt;
    if (to.mutable_reference && !from.mutable_reference) return nullopt;
    auto current = source;
    bool mutable_path = true;
    while (canDereference(current)) {
        const auto t = result.types[current];
        if (t.kind == TypeKind::Reference) {
            steps.push_back(Adjustment::Dereference);
            mutable_path = mutable_path && t.mutable_reference;
        }
        else steps.push_back(Adjustment::DereferenceBox);
        current = t.element;
        if (current == to.element && (!to.mutable_reference || mutable_path)) {
            steps.push_back(to.mutable_reference ? (source == target ? Adjustment::ReborrowMutable : Adjustment::BorrowMutable) : Adjustment::BorrowShared);
            return steps;
        }
    }
    return nullopt;
}

void Checker::use(const ExprInfo& info, NodeId node, TypeId target) {
    if (info.category == Category::Callable) fail(node, "functions are only supported in call position");
    UseInfo plan{target == unknown ? info.type : target, UseMode::Move, {}};
    if (target != unknown) {
        auto conversion = coercion(info.type, target);
        if (!conversion) fail(node, "conversion failed.");
        plan.adjustments = *conversion;
        auto adjusted = info;
        for (auto step : *conversion) {
            switch (step) {
            case Adjustment::Dereference:
            case Adjustment::DereferenceBox:
                adjusted = dereference(move(adjusted), node);break;
            case Adjustment::BorrowMutable:
            case Adjustment::ReborrowMutable:
                requireMutable(adjusted, node);
                plan.mode = UseMode::BorrowMutable;
                break;
            case Adjustment::BorrowShared: plan.mode = UseMode::BorrowShared; break;
            case Adjustment::NeverToTarget: plan.mode = UseMode::Unreachable; break;
            }
        }
    }
    if (info.type == never) plan.mode = UseMode::Unreachable;
    else if (plan.mode == UseMode::Move && capable(info.type, Capability::Copy)) plan.mode = UseMode::Copy;
    if (node) result.expressions.at(node).use = move(plan);
}

ExprInfo Checker::consume(const Exprnode& node, TypeId expected) {
    auto info = expression(node, expected);
    use(info, node.id, expected);
    return result.expressions.at(node.id);
}

TypeId Checker::merge(const vector<PendingUse>& values, TypeId expected, NodeId node) {
    TypeId target = never;
    if (expected != unknown) {
        for (const auto& item : values) {
            use(item.info, item.node, expected);
            if (item.info.type != never) target = expected;
        }
        return target;
    }

    for (const auto& item : values) {
        if (item.info.type == never) continue;
        if (target == never) { target = item.info.type; continue; }
        if (coercion(item.info.type, target)) continue;
        bool fit = true;
        for (const auto& prior : values) {
            if (&prior == &item) break;
            if (!coercion(prior.info.type, item.info.type)) { fit = false; break; }
        }
        if (!fit) fail(node, "result expressions have no supported common type");
        target = item.info.type;
    }

    for (const auto& item : values) use(item.info, item.node, target);
    return target;
}

};