#pragma once

#include "ast.hpp"
#include "types.hpp"
#include <optional>
#include <array>
#include <set>
#include <unordered_map>


using std::size_t;
using std::uint32_t;
using std::string;
using std::array;
using std::vector;
using std::map;
using std::set;
using std::tie;
using std::optional;
using std::runtime_error;
using std::move;

namespace rx::semantic {

// 解析后的名字
enum class NameKind { Local, Constant, Function, Builtin };
struct ResolvedName {
    NameKind kind;
    size_t id;
};

struct Local {
    string name;
    TypeId type;
    bool mutable_binding;
    NodeId function, declaration;
};
struct Constant { 
    TypeId type = unknown; 
    uint32_t bits = 0; 
};
enum class Builtin { None, GetI32, PrintI32, PrintlnI32, BoxNew, VecNew, VecLen, VecIsEmpty, VecPush, VecRemove, ArrayLen, Clone };
struct CallableInfo {
    NodeId function = 0; // 0 表示 builtin，源函数使用定义的 NodeId。
    Builtin builtin = Builtin::None;
    TypeId self = unknown;
    vector<TypeId> parameters; // 包含显式 receiver（若有）。
    TypeId result = unit;
    bool has_receiver = false;
};
struct FunctionInfo {
    string name;
    CallableInfo signature;
    vector<LocalId> parameters; // receiver 在最前面。
};

struct Field {
    string name;
    TypeId type;
};
struct StructInfo {
    string name;
    TypeId type = unknown;
    vector<Field> fields;
    map<string, size_t> field_indices;
    array<bool, 4> derives{};// Copy, Clone, PartialEq, Eq
    map<string, ResolvedName> members;
};

// 控制流信息。
struct Flow {
    bool normal = true;// 是否正常结束。
    bool returns = false;// 是否返回。
    set<NodeId> breaks, continues;// 可能的 break/continue 目标 AST 节点编号。
};
Flow alternatives(Flow left, const Flow& right);
Flow sequence(Flow before, const Flow& after);

enum class ProjectionKind { Dereference, DereferenceBox, Field, ArrayIndex, VecIndex };
struct Projection {
    ProjectionKind kind;
    NodeId node;
    size_t field = 0;
};
struct VecGate {
    NodeId index;
    bool mutable_access;
};
struct Place {
    size_t root = 0; // LocalId，或 temporary=true 时的表达式 NodeId。
    bool temporary = false;// 是否为临时变量。
    bool writable = false;// 是否可写。
    bool shared_barrier = false;// 是否跨过共享引用。
    vector<Projection> path;// 按顺序的投影操作。
    vector<VecGate> vec_gates;// 按顺序的 Vec 索引访问。
};

enum class Adjustment { Dereference, DereferenceBox, BorrowShared, BorrowMutable, ReborrowMutable, NeverToTarget };
enum class UseMode { Copy, Move, BorrowShared, BorrowMutable, InspectPlace, Discard, Unreachable };
struct UseInfo {
    TypeId target;
    UseMode mode;
    vector<Adjustment> adjustments;
};

struct OperatorInfo {
    TypeId operand_type;
    bool read_left_reference = false, read_right_reference = false;
    size_t ordering_reference_depth = 0;
};

enum class Category { Value, Place, Callable };
// Expr 检查结果
struct ExprInfo {
    TypeId type = unknown;
    Category category = Category::Value;
    Flow flow;
    Place place;
    optional<CallableInfo> callable;// 表达式本身表示的可调用对象。
    optional<CallableInfo> call_target;// 调用表达式选中的目标。
    optional<UseInfo> use;
    optional<OperatorInfo> operation;
    optional<ResolvedName> resolved_name; // PathExpr 解析到的声明或 builtin。
    optional<Constant> constant_value; // 已求出的常量值
    optional<vector<size_t>> field_order;// 结构体初始化字段顺序
    optional<NodeId> jump_target; // return 的函数，或 break/continue 的循环。
    optional<TypeId> temporary_type;// 父节点需要将本表达式的值物化为位置时，使用 temporary_type 作为临时变量的类型。
};

struct SemanticResult {
    Types types;
    NodeId entry = 0;
    map<NodeId, StructInfo> structs;
    map<NodeId, FunctionInfo> functions;
    map<NodeId, Constant> constants;
    map<LocalId, Local> locals;
    map<NodeId, LocalId> bindings;
    map<NodeId, TypeId> resolved_types;
    map<NodeId, ExprInfo> expressions;
    array<map<TypeId, bool>, 4> capabilities;
};

SemanticResult analyze(const AST::Crate& crate);

struct Diagnostic : runtime_error {
    NodeId node;
    Diagnostic(NodeId node, string message): runtime_error(move(message)), node(node) {}
};

};