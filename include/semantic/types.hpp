#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using std::size_t;
using std::uint32_t;
using std::tuple;
using std::vector;
using std::map;
using std::string;

namespace rx::semantic {

using NodeId = size_t;// 节点编号
using TypeId = size_t;// 类型编号
using LocalId = size_t;// 局部变量编号
inline constexpr TypeId unknown = 0, unit = 1, never = 2, boolean = 3, i32 = 4, u32 = 5, isize = 6, usize = 7;

enum class TypeKind { Unknown, Unit, Never, Bool, I32, U32, ISize, USize, Struct, Reference, Array, Box, Vec };
enum class Capability { Copy, Clone, PartialEq, Eq };

struct Type {
    TypeKind kind = TypeKind::Unknown;
    TypeId element = unknown;// 内部元素类型
    uint32_t length = 0;// 数组长度
    NodeId declaration = 0;// 结构体声明节点编号
    bool mutable_reference = false;// 是否为可变引用
};

class Types {
public:
    Types();
    Type operator[](TypeId id) const { return types[id]; }
    size_t size() const { return types.size(); }
    TypeId structure(NodeId declaration, const string& name);
    TypeId reference(TypeId inner, bool mut = false);
    TypeId array(TypeId element, uint32_t length);
    TypeId box(TypeId element);
    TypeId vec(TypeId element);
    static bool integer(TypeId id) { return id >= i32 && id <= usize; }
    static bool signedInteger(TypeId id) { return id == i32 || id == isize; }
    static bool scalar(TypeId id) { return integer(id) || id == boolean; }
private:
    using Key = tuple<TypeKind, TypeId, uint32_t, NodeId, bool>;
    vector<Type> types;
    map<Key, TypeId> interned;
    TypeId intern(Type type);
};

};