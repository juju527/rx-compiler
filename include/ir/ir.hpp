#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

using std::optional;
using std::string;
using std::vector;
using std::set;
using std::map;

namespace rx::IR {

using TypeId = size_t;
using BlockId = size_t;

constexpr TypeId Void = 0, I1 = 1, I8 = 2, I32 = 3, Ptr = 4;
enum class TypeKind { Void, Integer, Pointer, Array, Struct };
struct Type {
    TypeKind kind;
    //Interger
    unsigned bits = 0;
    //Array
    TypeId element = Void;
    uint32_t length = 0;
    //Struct
    string name;
    vector<TypeId> fields;
};

enum class OperandKind { Constant, Register, Parameter, Global };
struct Operand {
    TypeId type = Void;
    OperandKind kind = OperandKind::Constant;
    //Constant: Value, Register: SSA value number, Parameter: parameter index
    uint32_t number = 0;
    //Global
    string symbol;
};
Operand constant(TypeId type, uint32_t bits);
Operand global(string symbol);

enum class Opcode {
    Alloca, Load, Store, GEP,
    Add, Sub, Mul, SDiv, UDiv, SRem, URem,
    And, Or, Xor, Shl, AShr, LShr,
    ICmp,
    ZExt, Trunc,
    Call,
    Br, CondBr, Ret, Unreachable
};
// 谓词。
enum class Predicate { EQ, NE, SLT, SLE, SGT, SGE, ULT, ULE, UGT, UGE };
struct Instruction {
    Opcode opcode;
    optional<Operand> result;
    vector<Operand> operands;
    // 内存类型指令使用。
    TypeId memory_type = Void;
    // ICmp 指令使用。
    Predicate predicate = Predicate::EQ;
    string callee;
    vector<BlockId> targets;
};

struct BasicBlock {
    vector<Instruction> instructions;
    optional<Instruction> terminator;
};

struct Signature {
    TypeId result = Void;
    vector<TypeId> parameters;
    bool variadic = false;//参数是否可变。
};
struct Function {
    string symbol;
    Signature signature;
    bool external = false;
    bool exported = false;
    //原 AST 节点。
    size_t source_node = 0;
    vector<Instruction> allocas;
    vector<BasicBlock> blocks;
    // 每个函数各自维护 SSA 值编号。
    uint32_t next_value = 0;
};

struct Global {
    string symbol;
    vector<uint8_t> bytes;
    TypeId type;
};
struct Module {
    // Arch - Vender - OS - Environment
    string triple = "riscv32-unknown-none-elf";
    // 小端序、ELF 风格符号命名规则、地址空间 32 位、64 位整数对齐到 32 位、原生整数 32 位、128 位栈对齐。
    string datalayout = "e-m:e-p:32:32-i64:64-n32-S128";
    vector<Type> types;
    map<string, Function> functions;
    vector<Global> globals;
    Module();
    TypeId array(TypeId element, uint32_t length);
    TypeId structure(string name, vector<TypeId> fields);
    Function& declare(string name, Signature signature, bool external = false);
};

// 构建 LLVM IR 的工具类。
class Builder {
public:
    Module& module;
    Function* function = nullptr;
    optional<BlockId> current;

    explicit Builder(Module& m) : module(m) {}
    void begin(Function& f);// 开始构建一个函数。
    BlockId block();// 创建一个新的基本块。
    void setBlock(BlockId id);// 切换构造位置。
    bool live() const { return current.has_value(); }

    Operand emit(Instruction instruction, TypeId result = Void);// 生成一条指令。
    
    // 构造对应操作。
    Operand parameter(size_t i) const;
    Operand slot(TypeId storage);
    Operand load(TypeId type, Operand address);
    void store(Operand value, Operand address);
    Operand gep(TypeId storage, Operand address, vector<Operand> indices);// getelementptr
    Operand binary(Opcode opcode, Operand left, Operand right);
    Operand compare(Predicate pred, Operand left, Operand right);
    Operand convert(Opcode opcode, Operand value, TypeId type);
    Operand call(const string& callee, std::vector<Operand> args);
    void jump(BlockId target);
    void branch(Operand condition, BlockId yes, BlockId no);
    void ret(optional<Operand> value = {});
    void unreachable();
};

void writeLLVM(const Module& module, ostream& out);

};