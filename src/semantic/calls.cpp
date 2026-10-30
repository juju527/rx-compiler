#include "checker.hpp"

using std::logic_error;
using std::nullopt;
using std::optional;
using std::to_string;

namespace rx::semantic {

using namespace rx::AST;

CallableInfo Checker::callable(ResolvedName name, NodeId where) {
    if (name.kind == NameKind::Function) return result.functions.at(name.id).signature;
    if (name.kind != NameKind::Builtin) fail(where, "this value is not a function");
    CallableInfo result;
    result.builtin = static_cast<Builtin>(name.id);
    if (result.builtin == Builtin::GetI32) result.result = i32;
    else if (result.builtin == Builtin::PrintI32 || result.builtin == Builtin::PrintlnI32) result.parameters = {i32};
    else throw logic_error("unexpected unqualified builtin");
    return result;
}

optional<CallableInfo> Checker::associated(TypeId type, const string& name) {
    const auto t = result.types[type];
    if (t.kind == TypeKind::Struct) {
        const auto& members = result.structs.at(t.declaration).members;
        auto found = members.find(name);
        if (found != members.end()) {
            if (found->second.kind == NameKind::Function) return result.functions.at(found->second.id).signature;
            return nullopt;
        }
    }
    CallableInfo res;
    res.self = type;
    if (name == "new" && t.kind == TypeKind::Box) {
        res.builtin = Builtin::BoxNew;
        res.parameters = {t.element};
        res.result = type;
    }
    else if (name == "new" && t.kind == TypeKind::Vec) {
        res.builtin = Builtin::VecNew;
        res.result = type;
    }
    else if (name == "len" && (t.kind == TypeKind::Vec || t.kind == TypeKind::Array)) {
        res.builtin = t.kind == TypeKind::Vec ? Builtin::VecLen : Builtin::ArrayLen;
        res.parameters = {result.types.reference(type)};
        res.result = usize;
        res.has_receiver = true;
    }
    else if (name == "is_empty" && t.kind == TypeKind::Vec) {
        res.builtin = Builtin::VecIsEmpty;
        res.parameters = {result.types.reference(type)};
        res.result = boolean;
        res.has_receiver = true;
    }
    else if (name == "push" && t.kind == TypeKind::Vec) {
        res.builtin = Builtin::VecPush;
        res.parameters = {result.types.reference(type, true), t.element};
        res.has_receiver = true;
    }
    else if (name == "remove" && t.kind == TypeKind::Vec) {
        res.builtin = Builtin::VecRemove;
        res.parameters = {result.types.reference(type, true), usize};
        res.result = t.element;
        res.has_receiver = true;
    }
    else if (name == "clone" && capable(type, Capability::Clone)) {
        res.builtin = Builtin::Clone;
        res.parameters = {result.types.reference(type)};
        res.result = type;
        res.has_receiver = true;
    }
    else return nullopt;
    return res;
}

vector<CallableInfo> Checker::methods(TypeId receiver, const string& name) {
    vector<CallableInfo> res;
    vector<TypeId> owners{receiver};
    if (result.types[receiver].kind == TypeKind::Reference) owners.push_back(result.types[receiver].element);
    for (auto owner : owners) {
        const auto t = result.types[owner];
        if (auto target = associated(owner, name))
            if (target->has_receiver && target->parameters.front() == receiver)
                res.push_back(*target);
        if (name == "clone" && t.kind == TypeKind::Struct && capable(owner, Capability::Clone)) {
            auto found = result.structs.at(t.declaration).members.find(name);
            if (found != result.structs.at(t.declaration).members.end() && receiver == result.types.reference(owner)) {
                CallableInfo target;
                target.builtin = Builtin::Clone;
                target.self = owner;
                target.parameters = {receiver};
                target.result = owner;
                target.has_receiver = true;
                res.push_back(move(target));
            }
        }
    }
    return res;
}

ExprInfo Checker::call(const CallExpr& node) {
    auto callee = expression(*node.callee);
    if (callee.category != Category::Callable || !callee.callable)
        fail(node.callee->id, "this expression is not callable (a local may shadow a function)");
    const auto target = *callee.callable;
    if (node.arguments.size() != target.parameters.size())
        fail(node.id, "expected " + to_string(target.parameters.size()) + " arguments, found " + to_string(node.arguments.size()));
    auto flow = callee.flow;
    for (size_t i = 0; i < node.arguments.size(); i++) {
        auto argument = consume(*node.arguments[i], target.parameters[i]);
        flow = sequence(move(flow), argument.flow);
    }
    auto res = value(target.result, move(flow));
    res.call_target = target;
    return res;
}

ExprInfo Checker::methodCall(const MethodCallExpr& node) {
    if (!node.method.args.empty()) fail(node.id, "method segments do not accept concrete type arguments");
    auto receiver = expression(*node.receiver);
    auto base = receiver;
    vector<Adjustment> dereferences;
    while (true) {
        const TypeId candidates[3] = {base.type, result.types.reference(base.type), result.types.reference(base.type, true)};
        for (size_t cand = 0; cand < 3; cand++) {
            auto matches = methods(candidates[cand], node.method.name);
            if (matches.empty()) continue;
            if (matches.size() != 1) fail(node.id, "multiple methods match this receiver candidate");
            const auto target = matches.front();
            if (cand == 0) {
                use(base, node.receiver->id, target.parameters.front());
                auto& plan = *result.expressions.at(node.receiver->id).use;
                plan.adjustments.insert(plan.adjustments.begin(), dereferences.begin(), dereferences.end());
            }
            else {
                auto place = materialize(base, node.receiver->id);
                if (cand == 2) requireMutable(place, node.receiver->id);
                auto steps = dereferences;
                steps.push_back(cand == 1 ? Adjustment::BorrowShared : Adjustment::BorrowMutable);
                result.expressions.at(node.receiver->id).use = UseInfo{target.parameters.front(), cand == 1 ? UseMode::BorrowShared : UseMode::BorrowMutable, move(steps)};
            }
            if (node.arguments.size() + 1 != target.parameters.size()) fail(node.id, "wrong number of method arguments");
            auto flow = receiver.flow;
            for (size_t i = 0; i < node.arguments.size(); ++i) {
                auto argument = consume(*node.arguments[i], target.parameters[i + 1]);
                flow = sequence(move(flow), argument.flow);
            }
            auto result = value(target.result, move(flow));
            result.call_target = target;
            return result;
        }
        if (!canDereference(base.type)) break;
        dereferences.push_back(result.types[base.type].kind == TypeKind::Box ? Adjustment::DereferenceBox : Adjustment::Dereference);
        base = dereference(move(base), node.receiver->id);
    }
    fail(node.id, "no method '" + node.method.name);
}

};