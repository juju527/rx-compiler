#pragma once

#include "dispatch.hpp"
#include "semantic.hpp"
#include "ir.hpp"
#include "value.hpp"

using std::map;
using std::optional;
using std::pair;
using std::string;
using std::vector;
using std::move;

namespace rx::IR {

struct StorageLayout {
    TypeId storage;
    uint32_t size;
    uint32_t alignment;
    vector<uint32_t> offsets;
};

struct FunctionABI {
    string symbol;
    Signature signature;
    SourceTypeId result;
    vector<SourceTypeId> parameters;
    // 是否通过函数内的地址返回结果。
    bool indirect_result;
};

struct LoopTarget {
    BlockId next, exit;
    StorageSlot result_slot;
    size_t breaks = 0;// break 语句的数量。
};

enum class RuntimeOp {Clone, Equal, VecPush, VecRemove};
enum class VecField {Data, Length, Capacity};

struct FunctionContext {
    Builder builder;
    semantic::NodeId diagnostic_node;
    SourceTypeId return_type;
    optional<Operand> return_address;
    map<semantic::LocalId, StorageSlot> locals;
    map<semantic::NodeId, LoopTarget> loops;

    FunctionContext(Module& module, Function& function, SourceTypeId result): builder(module), diagnostic_node(function.source_node), return_type(result) {builder.begin(function);}
};

class Lowerer : private AST::ASTvisitor<Lowerer, EvalResult> {
    using SyntaxVisitor = AST::ASTvisitor<Lowerer, EvalResult>;
    friend SyntaxVisitor;

public:
    explicit Lowerer(const semantic::SemanticResult& semantic) : semantic_(semantic) {}
    //lower.cpp
    Module run(const AST::Crate& crate);

private:
    EvalResult visit(const AST::IntLiteral&);
    EvalResult visit(const AST::BoolLiteral&);
    EvalResult visit(const AST::UnitExpr&);
    EvalResult visit(const AST::PathExpr&);
    EvalResult visit(const AST::UnaryExpr&);
    EvalResult visit(const AST::BinaryExpr&);
    EvalResult visit(const AST::AssignExpr&);
    EvalResult visit(const AST::CastExpr&);
    EvalResult visit(const AST::CallExpr&);
    EvalResult visit(const AST::MethodCallExpr&);
    EvalResult visit(const AST::FieldExpr&);
    EvalResult visit(const AST::IndexExpr&);
    EvalResult visit(const AST::ArrayExpr&);
    EvalResult visit(const AST::ArrayRepeatExpr&);
    EvalResult visit(const AST::StructExpr&);
    EvalResult visit(const AST::BlockExpr&);
    EvalResult visit(const AST::IfExpr&);
    EvalResult visit(const AST::WhileExpr&);
    EvalResult visit(const AST::LoopExpr&);
    EvalResult visit(const AST::BreakExpr&);
    EvalResult visit(const AST::ReturnExpr&);
    EvalResult visit(const AST::ContinueExpr&);

    EvalResult visit(const AST::EmptyStmt&);
    EvalResult visit(const AST::LetStmt&);
    EvalResult visit(const AST::ExprStmt&);

    const semantic::SemanticResult& semantic;
    Module module;
    map<SourceTypeId, StorageLayout> layouts;
    map<semantic::NodeId, const AST::Exprnode*> expr_nodes;
    map<semantic::NodeId, const AST::FunctionItem*> source_functions;
    map<semantic::NodeId, FunctionABI> abis;
    map<pair<RuntimeOp, SourceTypeId>, string> runtime_symbols;
    vector<pair<RuntimeOp, SourceTypeId>> pending_runtime_ops;
    optional<FunctionContext> function_ctx;

    // lower.cpp
    static void fail(const string&);
    FunctionContext& context();
    Builder& builder();
    void beginFunction(Function&, SourceTypeId result = semantic::unit);
    void endFunction();
    const semantic::ExprInfo& exprInfo(const AST::Exprnode&) const;
    void indexLoweringSyntax(const AST::ASTnode&);

    // functions.cpp
    void declareSourceFunctions();
    void lowerFunction(const AST::FunctionItem&);
    EvalResult lowerCallExpr(const AST::Exprnode&);
    Value emitFunctionCall(const FunctionABI&, const std::vector<Value>&);
    void emitFunctionReturn(Value);
    FunctionABI buildFunctionABI(string, const semantic::CallableInfo&);
    Value valueFromOperand(SourceTypeId, Operand);
    void emitEntryPoint();

    // layout.cpp
    semantic::Type sourceType(SourceTypeId) const;
    bool isAggregate(SourceTypeId) const;
    StorageLayout storageLayout(SourceTypeId);
    TypeId valueReprType(SourceTypeId);

    // storage.cpp
    StorageSlot createStorageSlot(SourceTypeId);
    Value loadScalar(Place);
    void copyRepresentation(SourceTypeId, Operand destination, Operand source);
    Value snapshot(const EvalResult&);
    Value valueFromResultSlot(const StorageSlot&);
    void storeValue(const StorageSlot& destination, Value);
    Place materialize(const EvalResult&);
    void copyPlaceContents(Place destination, Place source);
};

//lower.cpp
Module lower(const AST::Crate&, const semantic::SemanticResult&);
void lowerAndWriteLLVM(const AST::Crate&, const semantic::SemanticResult&, ostream&);

};