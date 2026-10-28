#pragma once

#include "semantic.hpp"

using std::optional;
using std::map;

namespace rx::semantic {

using namespace rx::AST;

class Checker {
public:
    // checker.cpp：分析总流程
    SemanticResult run(const Crate& crate);
private:
    // 最终返回的语义结果；
    SemanticResult result;

    // 声明与名字环境；AST 指针只在检查期间使用，不进入 SemanticResult。
    map<string, TypeId> type_names;
    using Scope = map<string, ResolvedName>;
    // 第 0 层是全局值作用域。
    std::vector<Scope> scopes{1};
    struct FunctionSyntax { const FunctionItem* node; TypeId self; };
    struct ConstSyntax { const ConstItem* node; TypeId self; int state = 0; };
    map<NodeId, FunctionSyntax> functions;
    map<NodeId, ConstSyntax> constants;
    map<NodeId, const StructItem*> structures;

    // 当前函数的局部作用域与类型环境。
    NodeId current_function = 0;
    TypeId self_type = unknown, return_type = unit;
    LocalId next_local = 1;

    // 待确定公共类型的表达式，以及循环检查环境。
    // node=0 仅表示裸 break 的隐含 unit，参与定型但不生成表达式使用记录。
    struct PendingUse { NodeId node; ExprInfo info; };
    struct LoopContext {
        NodeId node;
        bool is_while;
        TypeId expected;
        vector<PendingUse> breaks;
    };
    vector<LoopContext> loops;
    size_t loop_barrier = 0;

    // checker.cpp：报错。
    void fail(NodeId node, const string& message) const;

    // declarations.cpp：声明、名字与类型解析、绑定登记、内嵌布局检查。
    void collect(const Crate& crate);
    void addMember(const Itemnode& item, TypeId self);
    TypeId typeSegment(const PathSegment& segment, NodeId node, TypeId self);
    TypeId resolveType(const Typenode& node, TypeId self);
    optional<ResolvedName> lookupValue(const string& name) const;
    ResolvedName resolveName(const Path& path, NodeId node, TypeId self);
    LocalId bind(const string& name, TypeId type, bool mut, NodeId declaration);
    void declarations();
    void checkLayouts();

    // constants.cpp：字面量范围、受限常量求值及依赖环。
    Constant integerLiteral(const IntLiteral& node, TypeId expected, bool negative = false);
    Constant evaluateConstant(NodeId id);
    Constant constExpr(const Exprnode& node, TypeId expected, TypeId self);

    // capabilities.cpp：类型能力查询与 derive 验证。
    bool capable(TypeId type, Capability capability);
    void checkDerives();

    // body.cpp：函数体环境、表达式分派、语句、聚合与控制流。
    ExprInfo value(TypeId type, Flow flow = {}) const;
    void functionBody(NodeId id);
    ExprInfo expression(const Exprnode& node, TypeId expected = unknown);
    ExprInfo expressionImpl(const Exprnode& node, TypeId expected);
    Flow statement(const Stmtnode& node);
    ExprInfo block(const BlockExpr& node, TypeId expected);
    ExprInfo array(const ArrayExpr& node, TypeId expected);
    ExprInfo repeat(const ArrayRepeatExpr& node, TypeId expected);
    ExprInfo structure(const StructExpr& node);
    ExprInfo conditional(const IfExpr& node, TypeId expected);
    ExprInfo loop(const LoopExpr& node, TypeId expected);
    ExprInfo whileLoop(const WhileExpr& node);
    ExprInfo breakExpr(const BreakExpr& node);

    // places.cpp：临时位置、解引用、访问权限及字段/下标投影。
    ExprInfo materialize(ExprInfo info, NodeId node);
    bool canDereference(TypeId type) const;
    ExprInfo dereference(ExprInfo info, NodeId node);
    void requireMutable(const ExprInfo& info, NodeId node);
    ExprInfo field(const FieldExpr& node);
    ExprInfo index(const IndexExpr& node);

    // conversions.cpp：隐式转换、使用计划与公共类型。
    optional<vector<Adjustment>> coercion(TypeId source, TypeId target) const;
    void use(const ExprInfo& info, NodeId node, TypeId target = unknown);
    ExprInfo consume(const Exprnode& node, TypeId expected = unknown);
    TypeId merge(const vector<PendingUse>& values, TypeId expected, NodeId where);

    // operators.cpp：一元/二元运算、赋值和显式 as 转换。
    TypeId scalarOperand(const ExprInfo& info, NodeId node) const;
    ExprInfo unary(const UnaryExpr& node);
    OperatorInfo checkBinaryOperands(BinaryOp op, const ExprInfo& left, const ExprInfo& right, NodeId node);
    ExprInfo binary(const BinaryExpr& node);
    ExprInfo assignment(const AssignExpr& node);
    ExprInfo cast(const CastExpr& node);

    // calls.cpp：可调用目标、内建签名、普通调用与点方法查找。
    CallableInfo callable(ResolvedName name, NodeId where);
    optional<CallableInfo> associated(TypeId type, const string& name);
    vector<CallableInfo> methods(TypeId receiver, const string& name);
    ExprInfo call(const CallExpr& node);
    ExprInfo methodCall(const MethodCallExpr& node);
};

};