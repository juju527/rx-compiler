#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace rx::AST {

using std::string;
using std::vector;
using std::pair;
using std::shared_ptr;
using std::move;
using std::forward;

enum class nodeKind {
    Crate,
    FunctionItem, StructItem, ConstItem, ImplItem,
    UnitType, PathType, RefType, ArrayType,
    EmptyStmt, LetStmt, ExprStmt,
    IntLiteral, BoolLiteral, UnitExpr, PathExpr,
    UnaryExpr, BinaryExpr, AssignExpr, CastExpr,
    CallExpr, MethodCallExpr, FieldExpr, IndexExpr,
    ArrayExpr, ArrayRepeatExpr, StructExpr, BlockExpr,
    IfExpr, WhileExpr, LoopExpr, BreakExpr, ReturnExpr, ContinueExpr,
};
string node_kind_to_string(nodeKind kind);

enum class Mutability { Immutable, Mutable };
enum class IntegerBase : std::uint8_t { Binary = 2, Octal = 8, Decimal = 10, Hex = 16 };
string intbase_to_string(IntegerBase base);
enum class IntegerSuffix { None, I32, U32, ISize, USize };
string intsuffix_to_string(IntegerSuffix suffix);

class Crate;
class FunctionItem;
class StructItem;
class ConstItem;
class ImplItem;
class UnitType;
class PathType;
class RefType;
class ArrayType;
class EmptyStmt;
class LetStmt;
class ExprStmt;
class IntLiteral;
class BoolLiteral;
class UnitExpr;
class PathExpr;
class UnaryExpr;
class BinaryExpr;
class AssignExpr;
class CastExpr;
class CallExpr;
class MethodCallExpr;
class FieldExpr;
class IndexExpr;
class ArrayExpr;
class ArrayRepeatExpr;
class StructExpr;
class BlockExpr;
class IfExpr;
class WhileExpr;
class LoopExpr;
class BreakExpr;
class ReturnExpr;
class ContinueExpr;

template<class Derived, class Result = void, class... Args>
class ASTvisitor;


class ASTnode {
public:
    size_t id;
    const nodeKind kind;

    virtual ~ASTnode() = default;
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }

protected:
    ASTnode(size_t id, nodeKind kind) : id(id), kind(kind) {}
};

using ASTnode_ptr = shared_ptr<ASTnode>;

//Type 类型
//Expr 表达式 
//Stmt 语句
//Item 声明

class Typenode : public ASTnode {
public:
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }
protected:
    Typenode(size_t id, nodeKind kind) : ASTnode(id, kind) {}
};
class Exprnode : public ASTnode {
public:
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }
protected:
    Exprnode(size_t id, nodeKind kind) : ASTnode(id, kind) {}
};
class Stmtnode : public ASTnode {
public:
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }
protected:
    Stmtnode(size_t id, nodeKind kind) : ASTnode(id, kind) {}
};
class Itemnode : public ASTnode {
public:
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }
protected:
    Itemnode(size_t id, nodeKind kind) : ASTnode(id, kind) {}
};


using Exprnode_ptr = shared_ptr<Exprnode>;
using Stmtnode_ptr = shared_ptr<Stmtnode>;
using Itemnode_ptr = shared_ptr<Itemnode>;
using Typenode_ptr = shared_ptr<Typenode>;

// 辅助结构
using Identifier = std::string;

struct Binding{
    Identifier name;
    Mutability mutability = Mutability::Immutable;
};

using GenericArguments = vector<Typenode_ptr>;//显式实参列表

enum class PathSegmentKind { Identifier, SelfValue, SelfType };
string pathsegmentkind_to_string(PathSegmentKind kind);

struct PathSegment{
    PathSegmentKind kind;
    Identifier name;
    GenericArguments args;
};

using Path = vector<PathSegment>;

struct Parameter {
    Binding binding;
    Typenode_ptr type;
};

enum class Receiver{
    Null,
    Value,             // self
    MutableValue,      // mut self：可变绑定。
    SharedReference,   // &self
    MutableReference,  // &mut self：可变引用。
};
string receiver_to_string(Receiver receiver);

struct FieldDeclaration {
    Identifier name;
    Typenode_ptr type;
};

struct FieldInitializer {
    Identifier name;
    Exprnode_ptr value;
};

enum class DeriveKind { Copy, Clone, PartialEq, Eq };
string derivekind_to_string(DeriveKind kind);
using DeriveAttribute = vector<DeriveKind>;

// Type

class UnitType final : public Typenode {
public:
    explicit UnitType(size_t id) : Typenode(id, nodeKind::UnitType) {}
};

class PathType final : public Typenode {
public:
    Path path;
    PathType(size_t id, Path path): Typenode(id, nodeKind::PathType), path(move(path)){}
};

class RefType final : public Typenode {
public:
    Typenode_ptr inner;
    Mutability mutability;
    RefType(size_t id, Typenode_ptr inner, Mutability mutability): Typenode(id, nodeKind::RefType), inner(move(inner)), mutability(mutability) {}
};

class ArrayType final : public Typenode {
public:
    Typenode_ptr element;
    Exprnode_ptr length;  // 必须是常量。
    ArrayType(size_t id, Typenode_ptr element, Exprnode_ptr length): Typenode(id, nodeKind::ArrayType), element(move(element)), length(move(length)) {}
};

//Expr

class IntLiteral final : public Exprnode {
public:
    string text;
    IntegerBase base;
    IntegerSuffix suffix;
    IntLiteral(size_t id, string text, IntegerBase base = IntegerBase::Decimal, IntegerSuffix suffix = IntegerSuffix::None): Exprnode(id, nodeKind::IntLiteral), text(move(text)), base(base), suffix(suffix) {}
};

class BoolLiteral final : public Exprnode {
public:
    bool value;
    BoolLiteral(size_t id, bool value): Exprnode(id, nodeKind::BoolLiteral), value(value) {}
};

class UnitExpr final : public Exprnode {
public:
    explicit UnitExpr(size_t id): Exprnode(id, nodeKind::UnitExpr) {}
};

class PathExpr final : public Exprnode {
public:
    Path path;
    PathExpr(size_t id, Path path): Exprnode(id, nodeKind::PathExpr), path(move(path)) {}
};

enum class UnaryOp {
    Negate,         // -x
    Not,            // !x，具体含义由操作数类型决定。
    Dereference,    // *x
    BorrowShared,   // &x
    BorrowMutable,  // &mut x
};
string unaryop_to_string(UnaryOp op);

class UnaryExpr final : public Exprnode {
public:
    UnaryOp op;
    Exprnode_ptr operand;
    UnaryExpr(size_t id, UnaryOp op, Exprnode_ptr operand): Exprnode(id, nodeKind::UnaryExpr), op(op), operand(move(operand)) {}
};

enum class BinaryOp {
    Add, Subtract, Multiply, Divide, Remainder,
    ShiftLeft, ShiftRight,
    BitAnd, BitOr, BitXor,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    LogicalAnd, LogicalOr,  // && 和 || 需要短路求值。
};
string binaryop_to_string(BinaryOp op);

class BinaryExpr final : public Exprnode {
public:
    BinaryOp op;
    Exprnode_ptr left;
    Exprnode_ptr right;
    BinaryExpr(size_t id, BinaryOp op, Exprnode_ptr left, Exprnode_ptr right): Exprnode(id, nodeKind::BinaryExpr), op(op), left(move(left)), right(move(right)) {}
};

enum class AssignOp {
    Assign,  // =
    Add, Subtract, Multiply, Divide, Remainder,
    BitAnd, BitOr, BitXor, ShiftLeft, ShiftRight,
};
string assignop_to_string(AssignOp op);

class AssignExpr final : public Exprnode {
public:
    AssignOp op;
    Exprnode_ptr target;
    Exprnode_ptr value;
    AssignExpr(size_t id, AssignOp op, Exprnode_ptr target, Exprnode_ptr value): Exprnode(id, nodeKind::AssignExpr), op(op), target(move(target)), value(move(value)) {}
};

class CastExpr final : public Exprnode {
public:
    Exprnode_ptr value;
    Typenode_ptr target_type;
    CastExpr(size_t id, Exprnode_ptr value, Typenode_ptr target_type): Exprnode(id, nodeKind::CastExpr), value(move(value)), target_type(move(target_type)) {}
};

class CallExpr final : public Exprnode {
public:
    Exprnode_ptr callee;
    vector<Exprnode_ptr> arguments;
    CallExpr(size_t id, Exprnode_ptr callee, vector<Exprnode_ptr> arguments = {}): Exprnode(id, nodeKind::CallExpr), callee(move(callee)), arguments(move(arguments)) {}
};

class MethodCallExpr final : public Exprnode {
public:
    Exprnode_ptr receiver;
    PathSegment method;
    vector<Exprnode_ptr> arguments;
    MethodCallExpr(size_t id, Exprnode_ptr receiver, PathSegment method, vector<Exprnode_ptr> arguments = {}): Exprnode(id, nodeKind::MethodCallExpr), receiver(move(receiver)), method(move(method)), arguments(move(arguments)) {}
};

class FieldExpr final : public Exprnode {
public:
    Exprnode_ptr base;
    Identifier field;
    FieldExpr(size_t id, Exprnode_ptr base, Identifier field): Exprnode(id, nodeKind::FieldExpr), base(move(base)), field(move(field)) {}
};

class IndexExpr final : public Exprnode {
public:
    Exprnode_ptr base;
    Exprnode_ptr index;
    IndexExpr(size_t id, Exprnode_ptr base, Exprnode_ptr index): Exprnode(id, nodeKind::IndexExpr), base(move(base)), index(move(index)) {}
};

class ArrayExpr final : public Exprnode {
public:
    vector<Exprnode_ptr> elements;
    ArrayExpr(size_t id, vector<Exprnode_ptr> elements = {}): Exprnode(id, nodeKind::ArrayExpr), elements(move(elements)) {}
};

class ArrayRepeatExpr final : public Exprnode {
public:
    Exprnode_ptr value;
    Exprnode_ptr count;// 常量。
    ArrayRepeatExpr(size_t id, Exprnode_ptr value, Exprnode_ptr count): Exprnode(id, nodeKind::ArrayRepeatExpr), value(move(value)), count(move(count)) {}

};

class StructExpr final : public Exprnode {
public:
    Path path;
    vector<FieldInitializer> fields;
    StructExpr(size_t id, Path path, vector<FieldInitializer> fields = {}): Exprnode(id, nodeKind::StructExpr), path(move(path)), fields(move(fields)) {}
};

// 注意 statement 列表末尾的无分号 expression with block。
class BlockExpr final : public Exprnode {
public:
    vector<Stmtnode_ptr> statements;
    Exprnode_ptr tail;  // 无尾表达式时为 nullptr。
    BlockExpr(size_t id, vector<Stmtnode_ptr> statements = {}, Exprnode_ptr tail = nullptr): Exprnode(id, nodeKind::BlockExpr), statements(move(statements)), tail(move(tail)) {}
};

using Block_ptr = shared_ptr<BlockExpr>;

class IfExpr final : public Exprnode {
public:
    Exprnode_ptr condition;
    Block_ptr then_branch; // 必须是 BlockExpr。
    Exprnode_ptr else_branch;  // 可选；存在时只能为 BlockExpr 或 IfExpr。
    IfExpr(size_t id, Exprnode_ptr condition, Block_ptr then_branch, Exprnode_ptr else_branch = nullptr): Exprnode(id, nodeKind::IfExpr), condition(move(condition)), then_branch(move(then_branch)), else_branch(move(else_branch)) {}
};

class WhileExpr final : public Exprnode {
public:
    Exprnode_ptr condition;
    Block_ptr body;
    WhileExpr(size_t id, Exprnode_ptr condition, Block_ptr body): Exprnode(id, nodeKind::WhileExpr), condition(move(condition)), body(move(body)) {}
};

class LoopExpr final : public Exprnode {
public:
    Block_ptr body;
    LoopExpr(size_t id, Block_ptr body): Exprnode(id, nodeKind::LoopExpr), body(move(body)) {}
};

class BreakExpr final : public Exprnode {
public:
    Exprnode_ptr value;  // 可选，break 没有操作数时为 nullptr。
    BreakExpr(size_t id, Exprnode_ptr value = nullptr): Exprnode(id, nodeKind::BreakExpr), value(move(value)) {}
};

class ReturnExpr final : public Exprnode {
public:
    Exprnode_ptr value;  // 可选，return 没有操作数时为 nullptr。
    ReturnExpr(size_t id, Exprnode_ptr value = nullptr): Exprnode(id, nodeKind::ReturnExpr), value(move(value)) {}
};

class ContinueExpr final : public Exprnode {
public:
    ContinueExpr(size_t id): Exprnode(id, nodeKind::ContinueExpr) {}
};

// Stmt

class EmptyStmt final : public Stmtnode {
public:
    EmptyStmt(size_t id): Stmtnode(id, nodeKind::EmptyStmt) {}
};

class LetStmt final : public Stmtnode {
public:
    Binding binding;
    Exprnode_ptr initializer;
    Typenode_ptr annotation;  // 可选，没有类型注解时为 nullptr。
    LetStmt(size_t id, Binding binding, Exprnode_ptr initializer, Typenode_ptr annotation = nullptr): Stmtnode(id, nodeKind::LetStmt), binding(move(binding)), initializer(move(initializer)), annotation(move(annotation)) {}
};

class ExprStmt final : public Stmtnode {
public:
    Exprnode_ptr expression;
    bool has_semicolon;// 有没有分号，这个信息方便后面处理。
    ExprStmt(size_t id, Exprnode_ptr expression, bool has_semicolon): Stmtnode(id, nodeKind::ExprStmt), expression(move(expression)), has_semicolon(has_semicolon) {}
};

//Item


// 可以出现在 impl 内的声明。
class AssociatedItem : public Itemnode {
public:
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }
protected:
    AssociatedItem(size_t id, nodeKind kind): Itemnode(id, kind) {}
};

using AssociatedItem_ptr = shared_ptr<AssociatedItem>;

class FunctionItem final : public AssociatedItem {
public:
    Identifier name;
    Receiver receiver;
    vector<Parameter> parameters;
    Typenode_ptr return_type;
    bool has_lifetime = 0;
    Block_ptr body;  // 可选，顶层函数可以没有 body。
    FunctionItem(size_t id, Identifier name, Block_ptr body = nullptr,
                 vector<Parameter> parameters = {}, Typenode_ptr return_type = nullptr,
                 Receiver receiver = Receiver(), bool has_lifetime = false)
        : AssociatedItem(id, nodeKind::FunctionItem), name(move(name)), receiver(receiver),
          parameters(move(parameters)), return_type(move(return_type)), body(move(body)), has_lifetime(has_lifetime) {}
};

class StructItem final : public Itemnode {
public:
    Identifier name;
    vector<FieldDeclaration> fields;
    vector<DeriveAttribute> derives;

    StructItem(size_t id, Identifier name, vector<FieldDeclaration> fields, vector<DeriveAttribute> derives = {})
        : Itemnode(id, nodeKind::StructItem), name(move(name)), fields(move(fields)), derives(move(derives)) {}
};

class ConstItem final : public AssociatedItem {
public:
    Identifier name;
    Typenode_ptr type;
    Exprnode_ptr value;// 必须是常量。
    ConstItem(size_t id, Identifier name, Typenode_ptr type, Exprnode_ptr value)
        : AssociatedItem(id, nodeKind::ConstItem), name(move(name)), type(move(type)), value(move(value)) {}
};

class ImplItem final : public Itemnode {
public:
    Typenode_ptr target_type;
    vector<AssociatedItem_ptr> items;
    ImplItem(size_t id, Typenode_ptr target_type, vector<AssociatedItem_ptr> items = {}): Itemnode(id, nodeKind::ImplItem), target_type(move(target_type)), items(move(items)) {}
};

class Crate final : public ASTnode {
public:
    template<class Visitor, class... Args>
    decltype(auto) accept(Visitor& visitor, Args&&... args) const {
        return visitor.dispatch(*this, forward<Args>(args)...);
    }
    vector<Itemnode_ptr> items;
    Crate(size_t id, vector<Itemnode_ptr> items = {}): ASTnode(id, nodeKind::Crate), items(move(items)) {}
};




}
