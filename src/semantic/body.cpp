#include "checker.hpp"

using std::move;
using std::logic_error;

namespace rx::semantic {

Flow alternatives(Flow left, const Flow& right) {
    left.normal = left.normal || right.normal;
    left.returns = left.returns || right.returns;
    left.breaks.insert(right.breaks.begin(), right.breaks.end());
    left.continues.insert(right.continues.begin(), right.continues.end());
    return left;
}

Flow sequence(Flow before, const Flow& after) {
    if (!before.normal) return before;
    before.normal = false;
    return alternatives(move(before), after);
}

ExprInfo Checker::value(TypeId type, Flow flow) const {
    ExprInfo result;
    result.type = type;
    result.flow = move(flow);
    return result;
}

// 检查函数体。
void Checker::functionBody(NodeId id) {
    const auto syntax = functions.at(id);
    const auto& node = *syntax.node;
    auto& info = result.functions.at(id);
    current_function = id;
    self_type = syntax.self;
    return_type = info.signature.result;
    scopes.resize(1); // 保留全局层，再建立当前函数的参数层。
    scopes.emplace_back();
    size_t index = 0;
    if (info.signature.has_receiver)
        info.parameters.push_back(bind("self", info.signature.parameters[index++], node.receiver == Receiver::MutableValue, id));
    for (const auto& parameter : node.parameters)
        info.parameters.push_back(bind(parameter.binding.name, info.signature.parameters[index++], parameter.binding.mutability == Mutability::Mutable, id));
    consume(*node.body, return_type);
    scopes.resize(1);
}

ExprInfo Checker::expression(const Exprnode& node, TypeId expected) {
    auto res = resolveExpression(node, expected);
    res.use.reset();
    result.expressions[node.id] = res;
    return res;
}

ExprInfo Checker::resolveExpression(const Exprnode& node, TypeId expected) {
    switch (node.kind) {
    case nodeKind::IntLiteral: {
        auto c = integerLiteral(static_cast<const IntLiteral&>(node), expected);
        auto res = value(c.type);
        res.constant_value = c;
        return res;
    }
    case nodeKind::BoolLiteral: return value(boolean);
    case nodeKind::UnitExpr: return value(unit);
    case nodeKind::PathExpr: {
        const auto& path = static_cast<const PathExpr&>(node).path;
        if (path.size() == 2) {
            if (!path.back().args.empty()) fail(node.id, "associated values do not accept type arguments");
            auto owner = resolveSegment(path.front(), node.id, self_type);
            if (auto target = associated(owner, path.back().name)) {
                ExprInfo res;
                res.category = Category::Callable;
                res.callable = *target;
                res.resolved_name = target->function ? ResolvedName{NameKind::Function, target->function} : ResolvedName{NameKind::Builtin, static_cast<size_t>(target->builtin)};
                return res;
            }
        }
        auto symbol = resolveName(path, node.id, self_type);
        ExprInfo res;
        res.resolved_name = symbol;
        if (symbol.kind == NameKind::Local) {
            const auto& binding = result.locals.at(symbol.id);
            res.type = binding.type;
            res.category = Category::Place;
            res.place.root = symbol.id;
            res.place.writable = binding.mutable_binding;
            return res;
        }
        if (symbol.kind == NameKind::Constant) {
            res.constant_value = result.constants.at(symbol.id);
            res.type = res.constant_value->type;
            return res;
        }
        res.category = Category::Callable;
        res.callable = callable(symbol, node.id);
        return res;
    }
    case nodeKind::UnaryExpr: return unary(static_cast<const UnaryExpr&>(node));
    case nodeKind::BinaryExpr: return binary(static_cast<const BinaryExpr&>(node));
    case nodeKind::AssignExpr: return assignment(static_cast<const AssignExpr&>(node));
    case nodeKind::CastExpr: return cast(static_cast<const CastExpr&>(node));
    case nodeKind::CallExpr: return call(static_cast<const CallExpr&>(node));
    case nodeKind::MethodCallExpr: return methodCall(static_cast<const MethodCallExpr&>(node));
    case nodeKind::FieldExpr: return field(static_cast<const FieldExpr&>(node));
    case nodeKind::IndexExpr: return index(static_cast<const IndexExpr&>(node));
    case nodeKind::ArrayExpr: return array(static_cast<const ArrayExpr&>(node), expected);
    case nodeKind::ArrayRepeatExpr: return repeat(static_cast<const ArrayRepeatExpr&>(node), expected);
    case nodeKind::StructExpr: return structure(static_cast<const StructExpr&>(node));
    case nodeKind::BlockExpr: return block(static_cast<const BlockExpr&>(node), expected);
    case nodeKind::IfExpr: return conditional(static_cast<const IfExpr&>(node), expected);
    case nodeKind::LoopExpr: return loop(static_cast<const LoopExpr&>(node), expected);
    case nodeKind::WhileExpr: return whileLoop(static_cast<const WhileExpr&>(node));
    case nodeKind::BreakExpr: return breakExpr(static_cast<const BreakExpr&>(node));
    case nodeKind::ContinueExpr: {
        if (loops.size() <= loop_barrier) fail(node.id, "continue has no accessible loop target");
        Flow flow;
        flow.normal = false;
        flow.continues.insert(loops.back().node);
        auto res = value(never, move(flow));
        res.jump_target = loops.back().node;
        return res;
    }
    case nodeKind::ReturnExpr: {
        const auto& ret = static_cast<const ReturnExpr&>(node);
        Flow operand;
        if (ret.value) operand = consume(*ret.value, return_type).flow;
        else if (return_type != unit) fail(node.id, "bare return requires a unit result");
        Flow jump;
        jump.normal = false;
        jump.returns = true;
        auto res = value(never, sequence(std::move(operand), jump));
        res.jump_target = current_function;
        return res;
    }
    default: throw logic_error("unexpected AST expression node");
    }
}

Flow Checker::statement(const Stmtnode& node) {
    switch (node.kind) {
    case nodeKind::EmptyStmt: return {};
    case nodeKind::LetStmt: {
        const auto& let = static_cast<const LetStmt&>(node);
        auto type = let.annotation ? resolveType(*let.annotation, self_type) : unknown;
        auto initializer = consume(*let.initializer, type);
        if (type == unknown) type = initializer.type;
        auto id = bind(let.binding.name, type, let.binding.mutability == Mutability::Mutable, node.id);
        result.bindings[node.id] = id;
        return initializer.flow;
    }
    case nodeKind::ExprStmt: {
        const auto& stmt = static_cast<const ExprStmt&>(node);
        auto info = consume(*stmt.expression, stmt.has_semicolon ? unknown : unit);
        if (stmt.has_semicolon) result.expressions.at(stmt.expression->id).use->mode = UseMode::Discard;
        return info.flow;
    }
    default: throw logic_error("unexpected AST statement node");
    }
}

ExprInfo Checker::block(const BlockExpr& node, TypeId expected) {
    scopes.emplace_back();
    Flow flow;
    // 即使 flow.normal 已经为 false，也必须检查后续语句。
    for (const auto& stmt : node.statements) flow = sequence(move(flow), statement(*stmt));
    TypeId type;
    if (node.tail) {
        auto tail = consume(*node.tail, expected);
        type = expected != unknown && tail.type != never ? expected : tail.type;
        flow = sequence(move(flow), tail.flow);
    }
    else type = flow.normal ? unit : never;
    scopes.pop_back();
    // 块产生新值，不能把 tail 的 place 身份泄漏到整个块。
    return value(type, move(flow));
}

ExprInfo Checker::array(const ArrayExpr& node, TypeId expected) {
    const auto target = result.types[expected];
    TypeId element_expected = target.kind == TypeKind::Array ? target.element : unknown;
    vector<PendingUse> elements;
    Flow flow;
    for (size_t i = 0; i < node.elements.size(); i++) {
        const auto& element = *node.elements[i];
        auto info = expression(element, element_expected);
        flow = sequence(move(flow), info.flow);
        elements.push_back({element.id, move(info)});
    }
    auto element_type = merge(elements, element_expected, node.id);
    if (element_expected != unknown) element_type = element_expected;
    if (node.elements.empty() && element_type == never) element_type = unit;
    if (node.elements.size() > UINT32_MAX) fail(node.id, "array length exceeds 32-bit usize");
    return value(result.types.array(element_type, static_cast<uint32_t>(node.elements.size())), move(flow));
}

ExprInfo Checker::repeat(const ArrayRepeatExpr& node, TypeId expected) {
    auto target = result.types[expected];
    auto element_expected = target.kind == TypeKind::Array ? target.element : unknown;
    const auto count = constExpr(*node.count, usize, self_type);
    auto element = consume(*node.value, element_expected);
    auto element_type = element_expected != unknown ? element_expected : element.type;
    if (count.bits > 1 && !capable(element_type, Capability::Copy)) fail(node.id, "array repetition with length greater than one requires Copy");
    return value(result.types.array(element_type, count.bits), move(element.flow));
}

ExprInfo Checker::structure(const StructExpr& node) {
    if (node.path.size() != 1) fail(node.id, "struct construction requires a user struct name");
    auto type = resolveSegment(node.path.front(), node.id, self_type);
    if (result.types[type].kind != TypeKind::Struct) fail(node.id, "construction requires a user-defined struct");
    const auto& s = result.structs.at(result.types[type].declaration);
    set<size_t> initialized;
    vector<size_t> order;
    Flow flow;
    for (size_t i = 0; i < node.fields.size(); i++) {
        const auto& field = node.fields[i];
        auto found = s.field_indices.find(field.name);
        if (found == s.field_indices.end()) fail(node.id, "unknown initializer field '" + field.name + "'");
        if (!initialized.insert(found->second).second) fail(node.id, "duplicate initializer field '" + field.name + "'");
        order.push_back(found->second);
        auto info = consume(*field.value, s.fields[found->second].type);
        flow = sequence(move(flow), info.flow);
    }
    if (initialized.size() != s.fields.size()) fail(node.id, "struct initializer is missing fields");
    auto res = value(type, move(flow));
    res.field_order = move(order);
    return res;
}

ExprInfo Checker::conditional(const IfExpr& node, TypeId expected) {
    auto condition = consume(*node.condition, boolean);
    if (!node.else_branch) {
        auto then = consume(*node.then_branch, unit);
        return value(unit, sequence(move(condition.flow), alternatives(move(then.flow), Flow{})));
    }
    auto then = expression(*node.then_branch, expected);
    auto otherwise = expression(*node.else_branch, expected);
    auto type = merge({{node.then_branch->id, then}, {node.else_branch->id, otherwise}}, expected, node.id);
    return value(type, sequence(move(condition.flow), alternatives(move(then.flow), otherwise.flow)));
}

ExprInfo Checker::loop(const LoopExpr& node, TypeId expected) {
    loops.push_back({node.id, false, expected, {}});
    auto body = consume(*node.body, unit);
    auto context = move(loops.back());
    loops.pop_back();
    auto type = merge(context.breaks, expected, node.id);
    auto flow = move(body.flow);
    flow.normal = flow.breaks.erase(node.id) != 0;
    flow.continues.erase(node.id);
    return value(type, move(flow));
}

ExprInfo Checker::whileLoop(const WhileExpr& node) {
    // 条件中的跳转不能命中当前 while，也不能命中外层 loop。
    const auto previous_barrier = loop_barrier;
    loop_barrier = loops.size();
    auto condition = consume(*node.condition, boolean);
    loop_barrier = previous_barrier;
    loops.push_back({node.id, true, unit, {}});
    auto body = consume(*node.body, unit);
    loops.pop_back();
    auto flow = move(body.flow);
    flow.breaks.erase(node.id);flow.continues.erase(node.id);
    flow.normal = true;
    return value(unit, sequence(move(condition.flow), flow));
}

ExprInfo Checker::breakExpr(const BreakExpr& node) {
    if (loops.size() <= loop_barrier) fail(node.id, "break has no accessible loop target");
    const auto index = loops.size() - 1;
    const auto target = loops[index].node;
    const auto expected = loops[index].expected;
    if (loops[index].is_while && node.value) fail(node.id, "while accepts only a bare break");
    auto operand = node.value ? expression(*node.value, expected) : value(unit);
    if (!loops[index].is_while) {
        if (node.value) loops[index].breaks.push_back({node.value->id, operand});
        else {
            if (expected != unknown && expected != unit) fail(node.id, "bare break has type ()");
            loops[index].breaks.push_back({0, operand});
        }
    }
    Flow jump;
    jump.normal = false;jump.breaks.insert(target);
    auto res = value(never, sequence(move(operand.flow), jump));
    res.jump_target = target;
    return res;
}


};