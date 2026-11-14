#include "lower.hpp"

using std::string;
using std::to_string;
using std::move;

namespace rx::IR {

using namespace AST;

void Lowerer::declareSourceFunctions() {
    for (const auto& [id, function] : semantic.functions) {
        if (!source_functions.count(id)) fail("semantic function absent from AST");
        auto abi = buildFunctionABI("rx.fn." + to_string(id), function.signature);
        module.declare(abi.symbol, abi.ir_signature);
        abis.emplace(id, move(abi));
    }
    return ;
}

FunctionABI Lowerer::buildFunctionABI(string symbol, const semantic::CallableInfo& source) {
    FunctionABI abi{move(symbol), {}, source.result, source.parameters, isAggregate(source.result)};
    abi.ir_signature.result = abi.indirect_result ? Void : valueReprType(source.result);
    if (abi.indirect_result) abi.ir_signature.parameters.push_back(Ptr);
    for (auto type : source.parameters)
        if (type != semantic::unit)
            abi.ir_signature.parameters.push_back(valueReprType(type));
    return abi;
}

Value Lowerer::valueFromOperand(SourceTypeId type, Operand operand) {
    if (type == semantic::unit) return Value{};
    if (isAggregate(type)) return Value::fromAggregateStorage(type, operand);
    return Value::direct(type, operand);
}

void Lowerer::lowerFunction(const FunctionItem& function) {
    const auto& abi = abis.at(function.id);
    const auto& metadata = semantic.functions.at(function.id);
    beginFunction(module.functions.at(abi.symbol), abi.source_result);
    size_t parameter = 0;
    if (abi.indirect_result) context().result_ptr = builder().parameter(parameter++);
    for (size_t i = 0; i < abi.source_parameters.size(); i++) {
        auto type = abi.source_parameters[i];
        auto local = metadata.parameters[i];
        if (semantic.locals.at(local).type != type) fail("parameter local type mismatch");
        auto slot = createStorageSlot(type);
        context().locals.emplace(local, slot);
        if (type != semantic::unit) storeValue(slot, valueFromOperand(type, builder().parameter(parameter++)));
    }
    auto result = lowerExpr(*function.body);
    if (result.normal()) emitFunctionReturn(snapshot(result));
    endFunction();
    return ;
}

void Lowerer::emitFunctionReturn(Value value) {
    auto& function = context();
    if (value.type() != function.source_return_type) fail("return source type mismatch");
    if (isAggregate(function.source_return_type)) {
        storeValue(Place{function.source_return_type, function.result_ptr.value()}, value);
        builder().ret();
    }
    else if (function.source_return_type == sem::unit) builder().ret();
    else builder().ret(value.directOperand());
    return ;
}

EvalResult Lowerer::visit(const CallExpr& node) {return lowerCallExpr(node);}
EvalResult Lowerer::visit(const MethodCallExpr& node) {return lowerCallExpr(node);}

EvalResult Lowerer::lowerCallExpr(const Exprnode& node) {
    const auto& target = *exprInfo(node).call_target;
    
    vector<Value> arguments;
    auto append = [&](const Exprnode& expression) {
        auto result = lowerExpr(expression);
        if (!result.normal()) return false;
        arguments.push_back(snapshot(result));
        return true;
    };

    if (node.kind == nodeKind::MethodCallExpr) {
        const auto& call = static_cast<const MethodCallExpr&>(node);
        if (!append(*call.receiver)) return EvalResult::terminated();
        for (const auto& argument : call.arguments) if (!append(*argument)) return EvalResult::terminated();
    }
    else {
        const auto& call = static_cast<const CallExpr&>(node);
        const auto& callee = exprInfo(*call.callee);
        if (callee.category != semantic::Category::Callable || !callee.callable) fail("callee contract");
        for (const auto& argument : call.arguments) if (!append(*argument)) return EvalResult::terminated();
    }

    if (target.function) return emitFunctionCall(abis_.at(target.function), arguments);
    return emitBuiltinCall(target, arguments);
}

Value Lowerer::emitFunctionCall(const FunctionABI& abi, const vector<Value>& arguments) {
    StorageSlot output;
    vector<Operand> operands;
    if (abi.indirect_result) {
        output = createStorageSlot(abi.source_result);
        operands.push_back(output.place().address);
    }
    for (size_t i = 0; i < arguments.size(); i++) if (arguments[i].type() != semantic::unit) operands.push_back(arguments[i].abiOperand());
    auto result = builder().call(abi.symbol, move(operands));
    if (abi.indirect_result) return valueFromResultSlot(output);
    return valueFromOperand(abi.source_result, result);
}

void Lowerer::emitEntryPoint() {
    auto& main = module_.declare("main", {I32, {}});
    main.exported = true;
    beginFunction(main, semantic::i32);
    builder().call(abis_.at(semantic_.entry).symbol, {});
    builder().ret(constant(I32, 0));
    endFunction();
    return ;
}

};