#include "checker.hpp"

#include <functional>

using std::logic_error;
using std::nullopt;
using std::function;

namespace rx::semantic {

using namespace rx::AST;

void Checker::collect(const Crate& crate) {
    type_names["i32"] = i32;type_names["u32"] = u32;type_names["isize"] = isize;
    type_names["usize"] = usize;type_names["bool"] = boolean;

    scopes[0]["get_i32"] = {NameKind::Builtin, static_cast<size_t>(Builtin::GetI32)};
    scopes[0]["print_i32"] = {NameKind::Builtin, static_cast<size_t>(Builtin::PrintI32)};
    scopes[0]["println_i32"] = {NameKind::Builtin, static_cast<size_t>(Builtin::PrintlnI32)};

    const set<string> protected_names = {"Box", "Vec", "Copy", "Clone", "PartialEq", "Eq"};
    
    // 注册结构体。
    for (const auto& item : crate.items) {
        if (item->kind != nodeKind::StructItem) continue;
        const auto& s = static_cast<const StructItem&>(*item);
        if (type_names.count(s.name) || protected_names.count(s.name)) fail(s.id, "duplicate or protected type name '" + s.name + "'");
        StructInfo info;
        info.name = s.name;
        info.type = result.types.structure(s.id, s.name);
        type_names[s.name] = info.type;
        structures[s.id] = &s;
        result.structs[s.id] = move(info);
    }

    // 登记顶层函数和常量。
    for (const auto& item : crate.items)
        if (item->kind == nodeKind::FunctionItem || item->kind == nodeKind::ConstItem)
            addMember(*item, unknown);

    // 解析 impl 的目标类型，并登记其成员。
    for (const auto& item : crate.items) {
        if (item->kind != nodeKind::ImplItem) continue;
        const auto& impl = static_cast<const ImplItem&>(*item);
        const auto target = resolveType(*impl.target_type, unknown);
        if (result.types[target].kind != TypeKind::Struct) fail(impl.id, "an inherent impl must target a user-defined struct");
        for (const auto& member : impl.items) addMember(*member, target);
    }
}

void Checker::addMember(const Itemnode& item, TypeId self) {
    string name;
    ResolvedName symbol{ NameKind::Function, item.id };
    if (item.kind == nodeKind::FunctionItem) {
        const auto& f = static_cast<const FunctionItem&>(item);
        name = f.name;
        if (self == unknown && f.receiver != Receiver::Null) fail(f.id, "a top-level function cannot declare a self receiver");
        functions[item.id] = {&f, self};
    }
    else {
        const auto& c = static_cast<const ConstItem&>(item);
        name = c.name;
        symbol.kind = NameKind::Constant;
        constants[item.id] = {&c, self};
    }
    auto& names = (self == unknown ? scopes.front() : result.structs.at(result.types[self].declaration).members);
    if (!names.emplace(name, symbol).second) fail(item.id, "duplicate or protected value name '" + name + "'");
}

// 解析一个 PathSegment。
TypeId Checker::resolveSegment(const PathSegment& segment, NodeId node, TypeId self) {
    if (segment.name == "Box" || segment.name == "Vec") {
        if (segment.args.size() != 1) fail(node, segment.name + " requires exactly one concrete type argument");
        auto inner = resolveType(*segment.args.front(), self);
        return segment.name == "Box" ? result.types.box(inner) : result.types.vec(inner);
    }
    if (!segment.args.empty()) fail(node, "type '" + segment.name + "' does not accept concrete type arguments");
    if (segment.kind == PathSegmentKind::SelfType) {
        if (self == unknown) fail(node, "Self is only available in a struct or its impl");
        return self;
    }
    auto found = type_names.find(segment.name);
    if (found == type_names.end()) fail(node, "unknown type '" + segment.name + "'");
    return found->second;
}

// 解析一个 Typenode。
TypeId Checker::resolveType(const Typenode& node, TypeId self) {
    auto found = result.resolved_types.find(node.id);
    if (found != result.resolved_types.end()) return found->second;
    TypeId res = unknown;
    switch (node.kind) {
    case nodeKind::UnitType: res = unit; break;
    case nodeKind::PathType: {
        const auto& path = static_cast<const PathType&>(node).path;
        if (path.size() != 1) fail(node.id, "a type path must name one supported type");
        res = resolveSegment(path.front(), node.id, self);
        break;
    }
    case nodeKind::RefType: {
        const auto& ref = static_cast<const RefType&>(node);
        res = result.types.reference(resolveType(*ref.inner, self), ref.mutability == Mutability::Mutable);
        break;
    }
    case nodeKind::ArrayType: {
        const auto& array = static_cast<const ArrayType&>(node);
        auto element = resolveType(*array.element, self);
        auto length = constExpr(*array.length, usize, self);
        res = result.types.array(element, length.bits);
        break;
    }
    default: throw logic_error("unexpected AST type node");
    }
    result.resolved_types[node.id] = res;
    return res;
}

optional<ResolvedName> Checker::lookupValue(const string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); it++) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return nullopt;
}

// 解析一个 Path。只接受长度 1 或 2。
ResolvedName Checker::resolveName(const Path& path, NodeId node, TypeId self) {
    if (path.empty() || path.size() > 2) fail(node, "unsupported value path");
    if (!path.back().args.empty()) fail(node, "value or method segments do not accept concrete type arguments");
    if (path.size() == 1) {
        auto symbol = lookupValue(path.front().name);
        if (symbol) return *symbol;
        fail(node, "unknown value '" + path.front().name + "'");
    }
    // 长为 2 的 Path。
    auto owner = resolveSegment(path.front(), node, self);
    if (result.types[owner].kind == TypeKind::Struct) {
        const auto& members = result.structs.at(result.types[owner].declaration).members;
        auto found = members.find(path.back().name);
        if (found != members.end()) return found->second;
    }
    fail(node, "unknown value '" + path.back().name + "'");
}

// 绑定一个局部变量。
LocalId Checker::bind(const string& name, TypeId type, bool mut, NodeId declaration) {
    auto id = next_local++;
    result.locals.emplace(id, Local{name, type, mut, current_function, declaration});
    scopes.back()[name] = {NameKind::Local, id};
    return id;
}


void Checker::declarations() {
    // 解析常量，得到具体值。
    for (const auto& entry : constants) evaluateConstant(entry.first);

    // 解析结构体，得到全部字段。
    for (const auto& entry : structures) {
        const auto& node = *entry.second;
        auto& info = result.structs.at(entry.first);
        for (const auto& field : node.fields) {
            const auto index = info.fields.size();
            if (!info.field_indices.emplace(field.name, index).second) fail(node.id, "duplicate field '" + field.name + "'");
            info.fields.push_back({field.name, resolveType(*field.type, info.type)});
        }
        for (const auto& attribute : node.derives)
            for (auto derive : attribute) {
                auto index = static_cast<size_t>(derive);
                if (info.derives[index]) fail(node.id, "duplicate derive entry");
                info.derives[index] = true;
            }
    }

    // 解析函数，得到函数签名。
    for (const auto& entry : functions) {
        const auto& node = *entry.second.node;
        const auto self = entry.second.self;
        CallableInfo signature;
        signature.function = node.id;
        signature.self = self;
        signature.has_receiver = node.receiver != Receiver::Null;
        if (signature.has_receiver) {
            auto receiver = self;
            if (node.receiver == Receiver::SharedReference) receiver = result.types.reference(self);
            if (node.receiver == Receiver::MutableReference) receiver = result.types.reference(self, true);
            signature.parameters.push_back(receiver);
        }
        set<string> parameter_names;
        if (signature.has_receiver) parameter_names.insert("self");
        for (const auto& parameter : node.parameters) {
            if (!parameter_names.insert(parameter.binding.name).second) fail(node.id,  "duplicate parameter '" + parameter.binding.name + "'");
            signature.parameters.push_back(resolveType(*parameter.type, self));
        }
        signature.result = node.return_type ? resolveType(*node.return_type, self) : unit;
        if (!node.body) throw logic_error("function AST has no body");
        result.functions[node.id] = {node.name, move(signature), {}};
    }
}

// 检查结构体内嵌布局。
void Checker::checkLayouts() {
    map<NodeId, int> state;
    function<void(TypeId)> dfs = [&](TypeId id) {
        const auto type = result.types[id];
        if (type.kind == TypeKind::Array) { dfs(type.element); return; }
        if (type.kind != TypeKind::Struct) return ; // 引用、Box、Vec 断开内嵌。
        const auto node = type.declaration;
        if (state[node] == 2) return ;
        if (state[node] == 1) fail(node, "struct '" + result.structs.at(node).name + "' has an infinite inline layout");
        state[node] = 1;
        for (const auto& field : result.structs.at(node).fields) dfs(field.type);
        state[node] = 2;
    };
    for (const auto& entry : result.structs) dfs(entry.second.type);
}

};