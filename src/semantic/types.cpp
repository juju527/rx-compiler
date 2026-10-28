#include "types.hpp"

namespace rx::semantic {
Types::Types() {
    intern(Type{TypeKind::Unknown});
    intern(Type{TypeKind::Unit});
    intern(Type{TypeKind::Never});
    intern(Type{TypeKind::Bool});
    intern(Type{TypeKind::I32});
    intern(Type{TypeKind::U32});
    intern(Type{TypeKind::ISize});
    intern(Type{TypeKind::USize});
}

// 通过类型驻留（Type interning）确保每个唯一的类型只被存储一次，并为其分配一个唯一的 TypeId。
TypeId Types::intern(Type t) {
    Key key{t.kind, t.element, t.length, t.declaration, t.mutable_reference};
    auto found = interned.find(key);
    if (found != interned.end()) return found->second;
    auto id = types.size();
    types.emplace_back(t);
    interned[key] = id;
    return id;
}

// 注册各种类型。
TypeId Types::structure(NodeId declaration, const string& name) { return intern({TypeKind::Struct, unknown, 0, declaration}); }
TypeId Types::reference(TypeId inner, bool mut) { return intern({TypeKind::Reference, inner, 0, 0, mut}); }
TypeId Types::array(TypeId element, uint32_t length) { return intern({TypeKind::Array, element, length}); }
TypeId Types::box(TypeId element) { return intern({TypeKind::Box, element}); }
TypeId Types::vec(TypeId element) { return intern({TypeKind::Vec, element}); }

};