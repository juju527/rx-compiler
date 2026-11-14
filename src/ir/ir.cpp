#include "ir.hpp"

using std::move;
using std::optional;
using std::set;
using std::string;
using std::vector;
using std::logic_error;

namespace rx::IR {

Operand constant(TypeId type, uint32_t bits) {return {type, OperandKind::Constant, bits, {}}; }
Operand global(string symbol) { return {Ptr, OperandKind::Global, 0, move(symbol)}; }

Module::Module(): types{{TypeKind::Void}, {TypeKind::Integer, 1}, {TypeKind::Integer, 8}, {TypeKind::Integer, 32}, {TypeKind::Pointer}} {}
TypeId Module::array(TypeId element, uint32_t length) {
    for (size_t i = 5; i < types.size(); i++) {
        const auto& type = types[i];
        if (type.kind == TypeKind::Array && type.element == element && type.length == length) return i;
    }
    types.push_back({TypeKind::Array, 0, element, length});
    return types.size() - 1;
}
TypeId Module::structure(string name, vector<TypeId> fields) {
    for (const auto& type : types)
        if (type.kind == TypeKind::Struct && type.name == name)
            throw logic_error("duplicate type " + name);
    types.push_back({TypeKind::Struct, 0, Void, 0, move(name), move(fields)});
    return types.size() - 1;
}
Function& Module::declare(string name, Signature signature, bool external) {
    Function function;
    function.symbol = name;
    function.signature = move(signature);
    function.external = external;
    auto [found, inserted] = functions.emplace(name, move(function));
    if (!inserted) throw logic_error("duplicate function " + name);
    return found->second;
}

void Builder::begin(Function& definition) {
    function = &definition;
    current = block();
    return ;
}
BlockId Builder::block() {
    function->blocks.emplace_back();
    return function->blocks.size() - 1;
}
void Builder::setBlock(BlockId id) {
    if (function->blocks.at(id).terminator) throw logic_error("insertion after terminator");
    current = id;
    return ;
}

Operand Builder::emit(Instruction instruction, TypeId type) {
    if (!live()) throw logic_error("instruction on a terminated path");
    Operand result;
    if (type != Void) {
        result = {type, OperandKind::Register, function->next_value++, {}};
        instruction.result = result;
    }
    auto& block = function->blocks.at(*current);
    if (block.terminator) throw logic_error("instruction after terminator");
    const auto opcode = instruction.opcode;
    if (opcode == Opcode::Br || opcode == Opcode::CondBr || opcode == Opcode::Ret || opcode == Opcode::Unreachable) block.terminator = move(instruction),current.reset();
    else block.instructions.push_back(move(instruction));
    return result;
}

Operand Builder::parameter(size_t index) const { return {function->signature.parameters.at(index), OperandKind::Parameter, uint32_t(index), {}};}
Operand Builder::slot(TypeId storage) {
    Operand address{Ptr, OperandKind::Register, function->next_value++, {}};
    Instruction instruction{Opcode::Alloca};
    instruction.result = address;
    instruction.memory_type = storage;
    function->allocas.push_back(move(instruction));
    return address;
}
Operand Builder::load(TypeId type, Operand address) {
    Instruction instruction{Opcode::Load};
    instruction.operands = {address};
    instruction.memory_type = type;
    return emit(move(instruction), type);
}
void Builder::store(Operand value, Operand address) {
    Instruction instruction{Opcode::Store};
    instruction.operands = {value, address};
    instruction.memory_type = value.type;
    emit(move(instruction));
    return ;
}
Operand Builder::gep(TypeId storage, Operand address, vector<Operand> indices) {
    Instruction instruction{Opcode::GEP};
    instruction.memory_type = storage;
    instruction.operands = {address};
    instruction.operands.insert(instruction.operands.end(), indices.begin(), indices.end());
    return emit(move(instruction), Ptr);
}
Operand Builder::binary(Opcode opcode, Operand left, Operand right) {
    Instruction instruction{opcode};
    instruction.operands = {left, right};
    return emit(move(instruction), left.type);
}
Operand Builder::compare(Predicate predicate, Operand left, Operand right) {
    Instruction instruction{Opcode::ICmp};
    instruction.predicate = predicate;
    instruction.operands = {left, right};
    return emit(move(instruction), I1);
}
// ZExt, Trunc。
Operand Builder::convert(Opcode opcode, Operand value, TypeId type) {
    Instruction instruction{opcode};
    instruction.operands = {value};
    return emit(move(instruction), type);
}
Operand Builder::call(const string& name, vector<Operand> arguments) {
    Instruction instruction{Opcode::Call};
    instruction.callee = name;
    instruction.operands = move(arguments);
    return emit(move(instruction), module.functions[name].signature.result);
}
void Builder::jump(BlockId target) {
    Instruction instruction{Opcode::Br};
    instruction.targets = {target};
    emit(move(instruction));
    return ;
}
void Builder::branch(Operand condition, BlockId yes, BlockId no) {
    Instruction instruction{Opcode::CondBr};
    instruction.operands = {condition};
    instruction.targets = {yes, no};
    emit(move(instruction));
    return ;
}
void Builder::ret(optional<Operand> value) {
    Instruction instruction{Opcode::Ret};
    if (value) instruction.operands = {*value};
    emit(move(instruction));
    return ;
}
void Builder::unreachable() { emit(Instruction{Opcode::Unreachable}); }


};