#include "ir.hpp"

using std::to_string;

namespace rx::IR {

string typeName(const Module& module, TypeId id) {
    const auto& type = module.types.at(id);
    switch (type.kind) {
    case TypeKind::Void: return "void";
    case TypeKind::Integer: return "i" + to_string(type.bits);
    case TypeKind::Pointer: return "ptr";
    case TypeKind::Array: return "[" + to_string(type.length) + " x " + typeName(module, type.element) + "]";
    case TypeKind::Struct: return "%" + type.name;
    }
    throw logic_error("unknown type");
}

string operandName(const Operand& operand) {
    switch (operand.kind) {
    case OperandKind::Constant: return operand.type == Ptr ? "null" : to_string(operand.number);
    case OperandKind::Register: return "%v" + to_string(operand.number);
    case OperandKind::Parameter: return "%p" + to_string(operand.number);
    case OperandKind::Global: return "@" + operand.symbol;
    }
    throw logic_error("unknown operand");
}

void writeParameters(const Module& module, const Signature& signature, ostream& out, bool names) {
    for (size_t i = 0; i < signature.parameters.size(); i++) {
        if (i) out << ", ";
        out << typeName(module, signature.parameters[i]);
        if (names) out << " %p" << i;
    }
    if (signature.variadic) {
        if (!signature.parameters.empty()) out << ", ";
        out << "...";
    }
    return ;
}

void writeInstruction(const Module& module, const Instruction& instruction, ostream& out) {
    auto value = [&](size_t index) { return operandName(instruction.operands.at(index)); };
    auto typed = [&](size_t index) { return typeName(module, instruction.operands.at(index).type) + " " + value(index);};
    out << "  ";
    if (instruction.result) out << operandName(*instruction.result) << " = ";

    switch (instruction.opcode) {
    case Opcode::Alloca:
        out << "alloca " << typeName(module, instruction.memory_type);
        break;
    case Opcode::Load:
        out << "load " << typeName(module, instruction.memory_type) << ", " << typed(0);
        break;
    case Opcode::Store:
        out << "store " << typed(0) << ", " << typed(1);
        break;
    case Opcode::GEP:
        out << "getelementptr " << typeName(module, instruction.memory_type) << ", " << typed(0);
        for (size_t index = 1; index < instruction.operands.size(); index++) out << ", " << typed(index);
        break;
    case Opcode::Add: case Opcode::Sub: case Opcode::Mul:
    case Opcode::SDiv: case Opcode::UDiv: case Opcode::SRem: case Opcode::URem:
    case Opcode::And: case Opcode::Or: case Opcode::Xor:
    case Opcode::Shl: case Opcode::AShr: case Opcode::LShr: {
        static const char* names[] = {
            "add", "sub", "mul", "sdiv", "udiv", "srem", "urem",
            "and", "or", "xor", "shl", "ashr", "lshr"
        };
        auto index = static_cast<int>(instruction.opcode) - static_cast<int>(Opcode::Add);
        out << names[index] << " " << typed(0) << ", " << value(1);
        break;
    }
    case Opcode::ICmp: {
        static const char* names[] = {"eq", "ne", "slt", "sle", "sgt", "sge", "ult", "ule", "ugt", "uge"};
        out << "icmp " << names[static_cast<int>(instruction.predicate)] << " " << typed(0) << ", " << value(1);
        break;
    }
    case Opcode::ZExt:
    case Opcode::Trunc:
        out << (instruction.opcode == Opcode::ZExt ? "zext " : "trunc ") << typed(0) << " to " << typeName(module, instruction.result->type);
        break;
    case Opcode::Call: {
        const auto& signature = module.functions.at(instruction.callee).signature;
        out << "call " << typeName(module, signature.result) << " ";
        if (signature.variadic) {
            out << "(";
            writeParameters(module, signature, out, false);
            out << ") ";
        }
        out << "@" << instruction.callee << "(";
        for (size_t index = 0; index < instruction.operands.size(); index++) {
            if (index) out << ", ";
            out << typed(index);
        }
        out << ")";
        break;
    }
    case Opcode::Br:
        out << "br label %b" << instruction.targets.at(0);
        break;
    case Opcode::CondBr:
        out << "br " << typed(0) << ", label %b" << instruction.targets.at(0)
            << ", label %b" << instruction.targets.at(1);
        break;
    case Opcode::Ret:
        out << "ret " << (instruction.operands.empty() ? "void" : typed(0));
        break;
    case Opcode::Unreachable:
        out << "unreachable";
        break;
    default: throw logic_error("unsupported opcode in printer");
    }
    out << '\n';
    return ;
}

void writeFunction(const Module& module, const Function& function, ostream& out) {
    const char* linkage = function.external ? "declare " : function.exported ? "define " : "define internal ";
    out << '\n' << linkage << typeName(module, function.signature.result) << " @" << function.symbol << "(";
    writeParameters(module, function.signature, out, !function.external);
    out << ")";
    if (function.external) {
        out << '\n';
        return ;
    }
    out << " {\n";
    for (size_t block = 0; block < function.blocks.size(); block++) {
        out << "b" << block << ":\n";
        if (block == 0) for (const auto& instruction : function.allocas) writeInstruction(module, instruction, out);
        for (const auto& instruction : function.blocks[block].instructions) writeInstruction(module, instruction, out);
        if (!function.blocks[block].terminator) throw logic_error("unterminated block");
        writeInstruction(module, *function.blocks[block].terminator, out);
    }
    out << "}\n";
    return ;
}

void writeLLVM(const Module& module, std::ostream& out) {
    out << "; Rx IR reference (RV32 / ILP32)\n"
        << "target datalayout = \"" << module.datalayout << "\"\n"
        << "target triple = \"" << module.triple << "\"\n\n";
    for (const auto& type : module.types) {
        if (type.kind != TypeKind::Struct) continue;
        out << "%" << type.name << " = type { ";
        for (size_t i = 0; i < type.fields.size(); i++) {
            if (i) out << ", ";
            out << typeName(module, type.fields[i]);
        }
        out << " }\n";
    }
    for (const auto& global : module.globals) {
        out << "@" << global.symbol << " = private constant " << typeName(module, global.type) << " c\"";
        for (auto byte : global.bytes)
            out << '\\' << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << unsigned(byte);
        out << std::dec << "\", align 1\n";
    }
    for (const auto& entry : module.functions) writeFunction(module, entry.second, out);
    if (!out) throw runtime_error("LLVM output write failed");
    return ;
}

};