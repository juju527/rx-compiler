# IR 阶段实施规划

本文规划如何在 AST 与 Semantic 的基础上建立 IR 阶段。实现应使用 C++17，在 `rx::ir` 命名空间中组织代码，以 AST 和 SemanticResult 为输入，构造独立的结构化 Module，再输出面向 RV32IM 的 LLVM IR。

下文的文件划分、类、函数和接入步骤均作为待实施的设计约定。阅读时可先结合 [AST 的 Visitor 接口](ast.md#visitor-redesign)与 [Semantic 的结果协议](semantic.md#result-protocol)，再依次实现值表示、存储、调用和控制流。实施进度应以最后的验收清单为准。

## 1. 目标、范围与目标平台

### 1.1 阶段边界

计划建立以下链路：

```text
Rx 源程序 → Lexer / Parser → AST → SemanticResult
                                     ↓
                            lower(AST, SemanticResult)
                                     ↓
                             结构化 Module
                                     ↓
                              writeLLVM
                                     ↓
                                    .ll
                                     ↓
                   LLVM 检查 → Clang → RV32IM 汇编 → REIMU
```

IR 阶段应负责源类型的运行时表示、存储布局、表达式求值、基本块、函数调用约定，以及 I/O、Box、Vec、Clone 和相等比较所需的运行支持。名字解析、类型推断、借用权限、移动合法性和派生能力应继续由 Semantic 决定。

第一版宜采用内存槽保存源变量、SSA 临时值保存计算结果的方式。源变量的重新赋值应生成 store；一个 SSA 寄存器只定义一次。条件表达式和循环的结果先通过结果槽合流，暂不要求 phi、完整 SSA 构造或优化 pass。

核心 Module 应可以供后续 Optimize 和 Codegen 直接读取。后续自主指令选择、寄存器分配及机器指令输出应围绕 Module 继续规划，不需要重新分析 AST 或解析已经输出的 LLVM 文本。

### 1.2 固定目标约定

目标应与[测试运行器](../scripts/test.py)一致：

| 项目 | 计划采用的约定 |
| --- | --- |
| LLVM 工具链 | Clang/LLVM 22 |
| target triple | `riscv32-unknown-none-elf` |
| 指令集 / ABI | RV32IM / ILP32 |
| datalayout | `e-m:e-p:32:32-i64:64-n32-S128` |
| 指针、isize、usize | 32 位 |
| 字节序 | 小端 |
| 功能测试的汇编生成 | `-O0 -mllvm -riscv-no-aliases` |
| REIMU 资源限制 | 256 MiB 内存、1 MiB 栈 |

类型大小、布局和分配长度应依据目标平台计算。宿主机可能使用 64 位指针，不能通过宿主 `sizeof` 推导源对象大小。开始接入前，应实际执行一次 RV32 编译并核对 datalayout，具体命令见第 18 节。

第一版应覆盖规范规定的有效执行范围。Drop/free、panic/unwind、越界或非法借用的运行时检查不作为本阶段新增功能；这些范围约定仍要求合法程序具有正确的复制、引用、控制流和资源行为。

## 2. 分层、文件与公开入口

### 2.1 依赖方向

建议将模块分成两层：

```text
AST + SemanticResult
         ↓
lower.hpp / value.hpp + lowering 实现
         ↓
ir.hpp / ir.cpp：Module、Builder、指令和操作数
         ↓
printer.cpp：LLVM 文本
```

核心 IR 与打印器应只依赖自己的模型及标准库。只有 lowering 层需要解释源 TypeId、语义结果表和 AST 节点。LLVM 校验应作为测试工具调用，第一版无需建立独立的内部 verifier、支配分析或合流槽初始化分析框架。

### 2.2 建议文件划分

计划使用三个头文件和十一个实现文件：

| 文件 | 计划职责 |
| --- | --- |
| `include/ir/ir.hpp` | IR 类型、操作数、指令、块、函数、Module、Builder 和 writeLLVM 声明 |
| `include/ir/value.hpp` | SourceTypeId、Place、Value、StorageSlot、NoReturn 和 EvalResult |
| `include/ir/lower.hpp` | lower 入口、Lowerer、Visitor 重载、布局、ABI 和函数上下文 |
| `src/ir/ir.cpp` | 类型与符号登记、Builder 和指令构造 |
| `src/ir/printer.cpp` | Module 到 LLVM 文本的序列化 |
| `src/ir/lower.cpp` | 会话流程、运行时 AST 索引、函数上下文生命周期 |
| `src/ir/functions.cpp` | 源函数 ABI、参数、调用、返回和机器 main |
| `src/ir/layout.cpp` | 源类型到计算表示、存储类型、大小和对齐的映射 |
| `src/ir/storage.cpp` | 槽分配、标量读取、快照、结果取值、物化和表示复制 |
| `src/ir/places.cpp` | 语义位置求值、字段/元素投影、解引用和 Vec 元数据地址 |
| `src/ir/expressions.cpp` | 使用计划、表达式入口、语句、标量运算、赋值和聚合构造 |
| `src/ir/control.cpp` | if、短路、循环、跳转和生成的索引循环 |
| `src/ir/builtins.cpp` | 处理语义分析选定的 builtin 及其已求值实参 |
| `src/ir/runtime.cpp` | 外部声明、I/O 包装、按类型登记与生成的运行时函数 |

各 lowering 文件应实现同一个 Lowerer 的成员。文件拆分只划分职责，不增加另一份状态容器或通用 pass 框架。CLI、阶段调度和输出文件发布应放在 `src/main.cpp`。

命名宜保持以下含义：`lower*` 表示带有源语义契约的生成入口或共享算法；`visit` 处理具体节点；`emit*` 构造具体 IR 或函数体；`ensureRuntimeFunction` 登记运行时函数并返回符号；`valueFrom*` 从已经准备好的结果取得 Value。

代码风格应沿用 AST、Semantic 的命名、缩进和标准库名称引入方式。节点和操作集合使用显式枚举、声明与 switch；不引入节点登记宏。示例中的 `vector`、`map`、`optional`、`ostream` 等名称按工程惯例使用。

### 2.3 公开 API 与所有权

建议公开以下入口：

```cpp
// ir.hpp：独立于前端。
void writeLLVM(const Module& module, ostream& out);

// lower.hpp：前端与 IR 的连接点。
Module lower(const AST::Crate& crate,
             const semantic::SemanticResult& semantic);
void lowerAndWriteLLVM(const AST::Crate& crate,
                       const semantic::SemanticResult& semantic,
                       ostream& out);
```

`lowerAndWriteLLVM` 应仅组合 `lower → writeLLVM`，两种调用方式的输出应一致。lowering 应通过 Builder 构造结构化对象，LLVM 文本统一由打印器生成。

一次 lower 调用期间，输入 AST 与 SemanticResult 应保持有效且只读，并且来自同一次语义分析。返回的 Module 应独立拥有类型、符号、全局字节数据、函数、块和指令；其中不保存前端对象的地址。前端对象销毁后，Module 仍应能够打印并供后续阶段处理。

## 3. 核心 IR 模型与 Builder

### 3.1 对象和身份

核心模型可按以下字段组织：

| 对象 | 建议保存的信息 |
| --- | --- |
| Type | kind、整数位宽、数组元素/长度、结构体名称/字段类型 |
| Operand | IR TypeId、kind、常量位模式或编号、全局符号 |
| Instruction | opcode、可选 result、operands、memory_type、predicate、callee、targets |
| BasicBlock | 普通指令序列、单独保存的可选终结指令 |
| Signature | 返回类型、参数类型、是否可变参数 |
| Function | 符号、签名、external/exported、allocas、blocks、next_value |
| Global | 符号、完整字节数组、IR 类型 |
| Module | triple、datalayout、类型表、函数表、全局数据 |

建议预置 `Void`、`I1`、`I8`、`I32`、`Ptr` 五个 IR TypeId，再按需登记数组和命名结构体。相同元素和长度的数组类型可复用；源结构体应按声明身份命名。指针使用 LLVM 的不透明 `ptr`。

实现中应明确区分以下身份：

| 身份 | 所属范围 |
| --- | --- |
| NodeId / LocalId | 源 AST 节点与语义绑定 |
| SourceTypeId | SemanticResult.types 中的源类型 |
| IR TypeId | 一个 Module 中的目标类型 |
| SSA 编号 / BlockId | 一个 Function 中的值与基本块 |

四种源整数都可以映射到 I32；Reference、Box 和聚合结果地址都可以使用 Ptr。它们的源语义仍有差别，因此 lowering 不应根据 IR TypeId 反推源类型或所有权。

OperandKind 建议只包含 Constant、Register、Parameter、Global。调用目标应单独写入 Instruction.callee，并通过 Module 的函数表取得签名；源语言 Callable 不需要变成普通运行时操作数。

### 3.2 指令子集

第一版宜覆盖下列指令：

| 类别 | 指令 |
| --- | --- |
| 内存 | Alloca、Load、Store、GEP |
| 算术 | Add、Sub、Mul、SDiv、UDiv、SRem、URem |
| 位操作 | And、Or、Xor、Shl、AShr、LShr |
| 比较与转换 | ICmp、ZExt、Trunc |
| 调用 | Call |
| 终结 | Br、CondBr、Ret、Unreachable |

Load、Store、GEP 和 Alloca 应保留所需的 `memory_type`。不透明指针本身没有元素类型，打印 GEP 时必须知道其基础存储类型。比较应保留明确谓词，有符号和无符号条件不能留到打印时猜测。

内存复制应通过普通 call 表示。Builder 只记录被调用符号和实参，第一版无需为 memmove 引入特殊指令或整槽读写元数据。

### 3.3 插入点与栈槽

Builder 建议持有 Module 引用、当前 Function 和 `optional<BlockId> current`。块应通过编号访问，以免 `vector` 扩容使缓存的块引用失效。

| Builder 操作 | 计划行为 |
| --- | --- |
| `begin(function)` | 建立函数入口块并设置插入点 |
| `block()` | 新建块并返回编号，不切换插入点 |
| `setBlock(id)` | 将后续指令插入指定的未终结块 |
| `slot(storage)` | 分配 SSA 地址编号，将 alloca 放入函数前序区 |
| `emit(instruction, result_type)` | 必要时分配结果编号，再插入当前块 |
| `jump/branch/ret/unreachable` | 写入当前块的终结指令并清除插入点 |
| `live()` | 返回当前是否有可继续插入的块 |

固定栈槽的 alloca 应统一打印在入口块最前面，初始化仍在实际执行位置生成。这样在循环体中请求的槽也只会在每次函数调用时分配一次。循环回边应指向单独的 test/body 块，入口块不接受回边。

终结后继续插入普通指令属于实现错误。可以在 Builder 的边界保留这一约束；无需让每个 visit 重复检查整套 IR 结构。保留在函数内的每个块最终都应有终结指令。

## 4. 值、位置、存储槽与控制流结果

### 4.1 四种表示

建议先建立这四种类型，再编写具体节点处理：

| 类型 | 回答的问题 | 主要内容 |
| --- | --- | --- |
| Place | 源对象存放在哪里？ | SourceTypeId、指向其存储的 Operand |
| Value | 本次求值已经取得什么？ | Unit、直接操作数或稳定的聚合存储 |
| StorageSlot | 为某个源类型准备了什么存储？ | SourceTypeId、可选 Place |
| EvalResult | 本条求值路径是否正常结束？ | `variant<NoReturn, Value, Place>` |

`semantic::Place` 应继续表示根身份和投影路径；`ir::Place` 则表示已经生成的运行时地址。两者承担不同层次的工作。

### 4.2 Value 的表示

Value 建议采用以下接口和形态：

| Repr | 含义 | 取得内容的接口 |
| --- | --- | --- |
| Unit | 正常得到 `()`，没有数据操作数 | `type()` |
| Direct | 操作数本身就是值，包括整数、bool、引用和 Box 指针 | `directOperand()` |
| StoredAggregate | 地址指向本次求值已经保存好的数组、结构体或 Vec | `storageAddress()` |

`Value{}` 应表示 Unit，`Value::direct(type, operand)` 应构造 Direct。`type()` 始终返回源类型，`repr()` 返回表示方式。选定 ABI 后可用 `abiOperand()` 取得 Direct 操作数或 StoredAggregate 的存储地址；Unit 不占 ABI 操作数。

`Value::fromAggregateStorage(type, address)` 宜只对 Lowerer 开放。其地址应来自 `snapshot` 创建的副本、已经填好的构造/调用/合流结果槽，或调用方按 ABI 提供的聚合快照。

这里的“稳定”指本次值使用期间内容保持为求值时的结果。一个可被后续表达式修改的数组位置，不能直接包装成已取得的聚合 Value。引用与 Box 的 Direct 值则保存指针本身；复制这个操作数不会复制指向的对象。

### 4.3 EvalResult 与 NoReturn

建议提供以下基本用法：

```cpp
EvalResult unit = Value{};
EvalResult result = value;
EvalResult location = place;
EvalResult ended = EvalResult::noReturn();
```

调用者应先检查 `normal()`，再按 `isPlace()` 读取 `place()` 或 `value()`。`type()` 可为 NoReturn 返回 `semantic::never`，但 NoReturn 本身不携带正常值。

Unit 应表示正常完成且没有数据；NoReturn 应表示当前生成路径已经通过 return、break、continue 或其他控制流终结。跳转表达式必须先执行其操作数，再发出终结指令，最后返回 NoReturn。

若子表达式返回 NoReturn，父节点应直接传播，不再求后续子节点、读取结果槽或发出本来依赖正常值的操作。语义表中的 `flow.normal == false` 不能用来跳过整个表达式，例如 `return f()` 仍须先执行 `f()`。

### 4.4 StorageSlot 的边界

StorageSlot 建议保存源类型和 `optional<Place> location`。普通类型分配物理槽；Unit 和 never 的常规结果槽不分配存储。

申请 never 的结果槽只是规划存储，不会自动终结基本块。只有实际生成的控制转移才产生 NoReturn。因此 if/loop 可以先准备结果槽，再根据正常前驱或 break 出口决定是否能够取得结果。

临时值需要显式取地址时，Unit 可以物化为零字节存储；普通 Unit 求值、参数与返回仍不应产生标量 load/store。零大小对象的可观察存储行为不应在第一版中扩展出规范以外的要求。

## 5. SemanticResult 的消费契约

### 5.1 按语义身份取信息

lowering 应消费 [semantic.hpp](../include/semantic/semantic.hpp) 已确定的结果：

| 生成任务 | 应读取的信息 |
| --- | --- |
| 确定源入口 | `SemanticResult.entry` |
| 声明函数与初始化参数 | `functions[id].signature`、`parameters`、`locals` |
| 建立 let 的局部槽 | `bindings[let.id]` 对应的 LocalId 与 `locals` |
| 确定节点类型与父级使用类型 | `ExprInfo.type`、`use.target` |
| 区分值、位置和可调用对象 | `category`、`callable` |
| 取得路径身份和常量 | `resolved_name`、`constant_value`、`constants` |
| 求地址及临时物化 | `place`、`temporary_type` |
| 选择调用目标 | 调用节点的 `call_target` |
| 选择运算类型及引用读取方式 | `operation` |
| 决定初始化字段 | AST 初始化顺序与 `field_order` |
| 选择跳转目标 | `jump_target` |
| 选择成员类型与派生操作 | `types`、`structs`、`capabilities` |

源函数和局部变量应按 ID 对应，不能重新按名字寻找定义。遮蔽会产生新的 LocalId；参数的 LocalId 顺序应直接取 FunctionInfo.parameters，receiver 位于最前。

语义表应保持只读。查表可以使用 `at`；读取语义结果时不应通过 `operator[]` 补出默认记录。实现应依赖已经成立的语义契约，缺失必要记录属于阶段接口错误，不应退回名字解析、默认 i32 或重新类型推断。也无需在 IR 中再执行一遍完整语义验证。

### 5.2 记录的适用范围

并非所有 AST 表达式都属于同一种运行时入口：

- 普通子表达式被父节点消费时，应有相应 use 计划。
- 普通函数调用的 callee 应作为 Callable 读取目标信息，无需按值求值。
- 位置投影的 base 应通过完整语义路径处理，不要求对每个中间节点应用 use。
- 类型中的数组长度、重复数组的长度及常量定义应使用 Semantic 已求出的结果，不生成运行时求值。

`unknown`、`never` 和 Unit 应分别处理。Callable 的 unknown 类型不代表一个默认整数；never 不存在正常运行时值；Unit 则可以正常结束。

### 5.3 节点结果与父级使用

Semantic 的 `ExprInfo.type` 描述节点本身，`UseInfo` 描述直接父节点如何使用该节点，详见[结果的登记与使用计划](semantic.md#result-protocol)。IR 应保留这一分工：先生成节点原始结果，再依次应用 adjustments，最后按 UseMode 取得值或保留位置。

生成的寄存器、地址和 EvalResult 不应写回 SemanticResult，也不应缓存在 `NodeId → 已求得的运行时值` 表中。NodeId 索引只用于找语法与语义信息；每个动态执行时刻的值应由生成的指令、槽和 CFG 表达。

## 6. Lowerer 如何使用 ASTvisitor

### 6.1 一个 Visitor 基类覆盖两类节点

Lowerer 的表达式与语句都应返回 EvalResult，因此建议共用一个 `ASTvisitor<Lowerer, EvalResult>` 基类。它与 Checker 的三个返回类型不同的 Visitor 基类形成对应：Lowerer 不需要额外的类型 Visitor，源类型解析已经由 Semantic 完成。

核心声明可组织为下面的形状；这里只列出 Visitor 部分：

```cpp
class Lowerer : private AST::ASTvisitor<Lowerer, EvalResult> {
    using SyntaxVisitor = AST::ASTvisitor<Lowerer, EvalResult>;
    friend SyntaxVisitor;

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
};
```

建议由 `SyntaxVisitor::dispatch(const Exprnode&)` 和 `dispatch(const Stmtnode&)` 分别进入表达式与语句重载。两类返回类型相同，可以共用同一基类；分类 dispatch 又使 Lowerer 只需覆盖 22 种表达式和 3 种语句。

Lowerer 不宜使用 `dispatch(const ASTnode&)` 作为运行时求值入口，否则会要求同时处理全部类型和声明节点。模块级声明扫描可以单独完成，不需要给类型语法或 const 定义安排伪造的 EvalResult。

AST 的分类 accept 可以保留；Lowerer 内部直接调用指定基类的 dispatch，能够明确当前类别。`friend SyntaxVisitor` 用于让基类调用私有 visit。visit 无需改成虚函数，Visitor 基类也不负责递归或共享返回值存储。

### 6.2 三个表达式入口

建议明确区分以下入口：

```cpp
EvalResult lowerExpr(const AST::Exprnode& node);
EvalResult lowerExprCore(const AST::Exprnode& node);
EvalResult lowerPlaceExpr(const AST::Exprnode& node);
EvalResult lowerStatement(const AST::Stmtnode& node);
```

| 入口 | 计划职责 |
| --- | --- |
| lowerExpr | 通常的子表达式入口；生成节点并应用本节点的一次 use 计划 |
| lowerExprCore | 只生成本节点自身，跳过本节点自己的 use；其子表达式仍按各自契约生成 |
| lowerPlaceExpr | 按语义根和完整投影链求位置；索引或临时根也可能产生 NoReturn |
| lowerStatement | 使用语句分类 dispatch，传播该语句的正常或终止结果 |

推荐调用链为：

```text
lowerExpr(node)
  读取本节点的 use
  lowerExprCore(node)
    已无插入点 → NoReturn
    语义类别为 Place → lowerPlaceExpr(node)
    语义类别为 Callable → 不进入普通运行时求值
    有 constant_value → 构造常量 Value
    其余 → SyntaxVisitor::dispatch(node)
             visit(具体表达式)
               lowerExpr(child) / lowerPlaceExpr(child)
  若结果为 NoReturn → 直接传播
  应用本节点 adjustments
  按 UseMode 返回 Value 或 Place
```

位置与常量处理应保留在 dispatch 之前。合法的 IntLiteral、常量路径、局部路径、FieldExpr 和 IndexExpr 通常会在这些路径中完成；仍应显式声明对应 visit，防止模板分派缺项，但不在其中重复解析字面量或回退到另一套投影算法。

调用、聚合、循环和跳转的 visit 可分别委托 `lowerCallExpr`、`lowerAggregateExpr`、`lowerLoopExpr`、`lowerJumpExpr`。一元、二元、cast、赋值、块与 if 的具体逻辑可直接放在相应 visit 中。

### 6.3 adjustments 与 UseMode

调整链应按记录顺序执行：

| Adjustment | 计划动作 |
| --- | --- |
| Dereference / DereferenceBox | 取得引用或 Box 的指针，形成 element 类型的 Place |
| BorrowShared / BorrowMutable / ReborrowMutable | materialize 取得地址，构造目标引用类型的 Direct Value |
| NeverToTarget | 不制造目标类型的值；实际求值应已经产生 NoReturn |

调整结束后，正常结果应具有 `use.target` 指定的源类型，再按模式处理：

| UseMode | 计划动作 |
| --- | --- |
| Copy / Move | snapshot，取得本次消费的 Value |
| InspectPlace | 保留位置，供赋值或投影使用 |
| BorrowShared / BorrowMutable | 保留调整后的结果，不再额外借用一次 |
| Discard | 保留副作用，允许父节点忽略正常结果 |
| Unreachable | 应在执行必要前缀后得到 NoReturn，不产生正常目标值 |

Move 可以搬运表示，不应隐式调用 Clone，也不需要清零旧槽。移动后的使用是否合法由 Semantic 决定。

BorrowShared 模式本身不保证要产生 `&T`。相等比较可能记录“原类型、BorrowShared、空调整链”，用于稍后观察原对象。实际构造引用应由调整链决定。显式 `&x` 的 operand 已经包含借用调整，UnaryExpr 的 visit 应直接交回该结果。

### 6.4 递归与返回值

每次递归应通过局部变量保存 EvalResult，例如二元运算先保存 left，再求 right；后一次访问不会覆盖前一次结果。Lowerer 不应建立“最后一个值”成员。

常规递归应调用 `lowerExpr(child)`；需要完整目标地址时调用 `lowerPlaceExpr(child)`；临时投影根需要保留原类型时调用 `lowerExprCore(root)`。直接对子节点调用 visit、dispatch 或 accept 会绕过这些入口的 use 与位置协议，应限于入口内部的分派。

尤其应明确：`lowerExprCore(root)` 只跳过 root 自己的 use，不会递归跳过整棵子树的 use。调用实参、块中语句、构造元素等仍须按各自的正常入口生成。

## 7. 会话、AST 索引与函数上下文

### 7.1 模块状态

Lowerer 应代表一次 lower 调用，建议保存：

| 成员 | 生命周期和用途 |
| --- | --- |
| semantic_ | 本次调用期间的只读语义结果引用 |
| module_ | 正在构造的模块 |
| layouts_ | SourceTypeId 到 StorageLayout 的缓存 |
| expr_nodes_ | 运行时表达式 NodeId 到 AST 节点的索引 |
| source_functions_ | 源函数 NodeId 到 FunctionItem 的索引 |
| abis_ | 源函数 NodeId 到 FunctionABI |
| runtime_symbols_ | `(RuntimeOp, SourceTypeId)` 到函数符号 |
| pending_runtime_ops_ | 已登记、待生成的运行时函数队列 |
| function_ctx_ | 当前唯一活动的 FunctionContext |

建议通过 `Lowerer(semantic).run(crate)` 实现公开 lower。每次调用创建新实例，编号、布局和运行时函数登记表不应使用跨模块共享的可变静态缓存。

### 7.2 索引范围与模块生成顺序

`indexRuntimeSyntax` 可使用显式 switch 扫描 Crate、impl 内函数、函数体及运行时子表达式。索引只提供定位能力，不执行表达式，也不申请运行时存储。

应跳过类型语法、常量定义、数组类型长度以及 ArrayRepeatExpr 的长度子树；这些内容应消费 Semantic 的求值结果。重复数组的元素表达式仍需索引并在运行时求值。

`run` 建议按以下顺序执行：

1. 建立运行时表达式和源函数索引。
2. 登记外部支持函数并生成固定的 I/O 包装。
3. 为全部源函数建立 FunctionABI 并登记签名。
4. 逐个生成源函数体。
5. 生成机器入口 main。
6. 排空待生成的按类型运行时函数队列。
7. 移交独立拥有数据的 Module。

所有源签名应在源函数体之前登记，以支持前向调用、递归和互递归。符号可按 `rx.fn.<NodeId>`、`rx.struct.<NodeId>`、`rx.rt.*` 组织，输出顺序应保持确定。

### 7.3 FunctionContext

当前函数的状态建议集中为：

```cpp
struct FunctionContext {
    Builder builder;
    SourceTypeId source_return_type;
    optional<Operand> result_ptr;
    map<semantic::LocalId, StorageSlot> locals;
    map<semantic::NodeId, LoopTarget> loops;
};
```

构造上下文时应使 Builder 开始对应 Function。`result_ptr` 仅保存聚合返回所需的隐藏输出地址。locals 应按 LocalId 对应槽；loops 应按循环 NodeId 对应 continue 块、exit 块、结果槽和已生成的 break 出口数。

计划采用唯一的 `function_ctx_`，处理函数通过 `context()` 和 `builder()` 访问它。既然上下文由成员保存，Visitor 就无需再通过模板参数传入另一份 FunctionContext。

所有函数种类应使用同一生命周期：

```text
无活动函数
  → beginFunction：创建 FunctionContext、建立入口块
  → 初始化参数 / 生成函数体 / 终结最后一条活动路径
  → endFunction：结束并销毁上下文
  → 无活动函数
```

源函数、机器 main、I/O 包装和按类型生成的运行时函数都应遵守这一边界。生成源函数时只能登记所需运行时函数，不能嵌套切换当前函数去生成其函数体。

源作用域的可见性已经由 Semantic 处理，IR 无需重新维护名字作用域。不同遮蔽绑定应保留各自槽，已有引用继续指向原槽。固定槽保留到函数返回，递归调用由不同运行时栈帧提供隔离；这不应放宽源语言的生命周期限制。

## 8. 类型表示与存储布局

### 8.1 计算表示与内存表示

建议分别提供 `valueReprType(SourceTypeId)` 和 `storageLayout(SourceTypeId)`。前者供计算和 ABI 使用，后者返回 `StorageLayout { storage_type, size, alignment }`，大小应包含尾部 padding。

| 源类型 | Value 形态 / 计算表示 | 存储类型 | RV32 大小 / 对齐 |
| --- | --- | --- | --- |
| i32、u32、isize、usize | Direct / i32 | i32 | 4 / 4 |
| bool | Direct / i1 | i8，保存 0 或 1 | 1 / 1 |
| &T、&mut T | Direct / ptr | ptr | 4 / 4 |
| Box<T> | Direct / ptr | ptr | 4 / 4 |
| [T; N] | StoredAggregate / ptr | `[N x Storage(T)]` | N × 元素大小 / 元素对齐 |
| 用户结构体 | StoredAggregate / ptr | 按声明字段顺序的命名 struct | 按字段计算 |
| Vec<T> | StoredAggregate / ptr | `{ ptr, i32, i32 }` | 12 / 4 |
| Unit | Unit / 无操作数 | 常规结果无槽；取地址时可用 `[0 x i8]` | 0 / 1 |
| never | 无正常 Value | 不请求运行时布局 | 不适用 |

应由 `loadScalar` 将 bool 的 i8 存储截断为 i1，由 `storeValue` 将 i1 零扩展为 i8。函数参数、返回和条件使用 i1，局部槽、字段和数组元素使用 i8。这样 bool 与 i32 混排时不会覆盖相邻对象。

Reference、Box 的布局应在指针处停止；Vec 自身布局也不展开元素。`Node { children: Vec<Node> }` 的布局因此可以终止。按值递归结构的合法性应使用 Semantic 的检查结果，不需要在 IR 再建立一套源类型递归验证。

### 8.2 数组和结构体布局

结构体大小建议按以下步骤计算：

1. 按声明顺序取得每个字段的 StorageLayout。
2. 将当前大小向字段 alignment 取整，作为该字段起点。
3. 加上字段 size，并更新结构体的最大 alignment。
4. 最后将总大小向最大 alignment 取整。

例如 `{ bool, i32, bool }` 应得到偏移 0、4、8，大小 12、对齐 4。数组元素步长应使用包含尾部 padding 的大小，所以该结构体组成数组时步长为 12。

LLVM 命名结构体应使用普通、非 packed 的字段序列，与上述目标布局一致。字段求址可直接使用结构体 GEP 的声明下标，不必额外维护一份供寻址使用的字段偏移表。布局计算出的 size 主要供分配和表示复制使用。

大小计算可用较宽的宿主无符号整数保存中间结果，再确认可表示为目标的 32 位字节数。最终 Module 中的类型、GEP 和 malloc 长度应全部遵循同一目标约定。

### 8.3 按需布局

布局应按实际生成需要计算并缓存。Semantic 可能为了检查不可达表达式而建立含 never 的类型；IR 不应遍历整个源类型表并强制为所有条目生成布局。

ArrayRepeatExpr 的长度应读取已解析数组类型的 length。常量路径应使用 semantic 的常量位模式。`unknown` 和 never 不应静默转换为 i32 存储类型。

## 9. 统一存储操作与位置求值

### 9.1 存储操作的接口

建议集中实现以下操作，供表达式、调用、控制流和运行时函数共同使用：

```cpp
StorageSlot createStorageSlot(SourceTypeId type);
Value loadScalar(Place place);
Value snapshot(const EvalResult& result);
Value valueFromResultSlot(const StorageSlot& slot);
Place materialize(const EvalResult& result);
void storeValue(const StorageSlot& destination, Value value);
void copyRepresentation(SourceTypeId type, Operand destination, Operand source);
void copyPlaceContents(Place destination, Place source);
```

| 操作 | 应承担的行为 |
| --- | --- |
| createStorageSlot | 普通类型请求入口 alloca；Unit/never 只保留类型 |
| loadScalar | 从标量 Place 读取 Direct，统一处理 bool；Unit 得到 Unit |
| snapshot | Value 原样保留；标量 Place 读取；聚合 Place 复制到独立槽 |
| valueFromResultSlot | 从本次已填好的专有结果槽取值；聚合直接使用该槽地址 |
| materialize | Place 原样保留；StoredAggregate 复用地址；Direct 建槽并写入 |
| storeValue | 同源类型写入：标量 store、聚合复制、Unit 无数据写入 |
| copyRepresentation | 按源类型布局大小调用 memmove，零字节复制跳过 |
| copyPlaceContents | 在两个同类型位置间搬运内容：标量读写或聚合表示复制 |

`snapshot`、`materialize` 和 `valueFromResultSlot` 的调用点应先满足正常求值的前提。never 槽不能产生正常值，NoReturn 也不能通过 snapshot 转成 Unit。

### 9.2 快照、结果槽与地址身份

snapshot 应保存“求值发生时的内容”。例如数组 a 作为较早实参时，需要在较晚实参修改 a 之前完成复制。

valueFromResultSlot 应用于已经属于本次求值的构造槽、调用输出槽或合流槽。此时聚合内容已经独立，直接返回 StoredAggregate 可以避免再次复制。二者应按存储来源选择，不应仅因为都返回 Value 就互换。

materialize 应用于借用和投影。例如一个整数临时值需要先写入槽，才能取得 `&i32`；一个聚合调用结果已经有专有存储，可以直接借用该地址。

对既有聚合变量赋值时，应向原槽复制内容，保持它的地址身份。若直接用右侧临时地址替换 locals 中的槽映射，旧引用将继续指向失去更新的对象，源程序行为会发生变化。

表示复制与深 Clone 应分开。数组、结构体或 Vec 的 Move 可以复制表示；Vec 的表示只包含 data、len、cap。共享引用字段的表示复制应保留原指向。可能重叠的搬运应使用 memmove，尤其是 Vec remove 的后缀移动。

### 9.3 完整语义投影链

`lowerPlaceExpr` 应采用“根求值一次，再回放完整 path”的算法：

| 阶段 / ProjectionKind | 计划处理 |
| --- | --- |
| 普通根 | 按 LocalId 从 FunctionContext.locals 取得槽 |
| 临时根 | 按 NodeId 找 AST，调用 lowerExprCore，再按 temporary_type 物化 |
| Dereference / DereferenceBox | 取得指针值，形成 element 类型的 Place |
| Field | 按声明字段号生成结构体 GEP，结果带字段的源类型 |
| ArrayIndex | 求索引一次，再以 `[0, index]` 对数组存储生成 GEP |
| VecIndex | 求索引一次，再读取 data，以元素存储类型为步长生成 GEP |

索引投影中的 `Projection.node` 指向 IndexExpr，应由它找到 index 子表达式，再通过 lowerExpr 生成索引。中间 base 的完整语义路径已经包含在当前 path 中，不应先递归生成 base，再重放整条路径。

对于 `make_array()[index()]`，应先生成 make_array 的临时根，再生成 index，二者各一次。临时根使用 lowerExprCore，是为了取得节点本身的类型和内容；根内部的实参仍需使用正常的 lowerExpr。

`temporary_type` 应描述物化前原始值的源类型，不能使用父级转换后的 target 代替。`writable`、`shared_barrier` 和 `vec_gates` 的权限已经由 Semantic 检查，不需要转成额外运行时权限状态。

### 9.4 Vec 元数据与源对象

建议提供 `projectField(Place, index)`、`projectElement(Place, index)` 和 `dereference(EvalResult)`，这些函数应返回带有正确源类型的 Place。

Vec 的 data、len、cap 是内部表示字段，宜单独用 `vecFieldAddress(Place, VecField)` 返回 IR 地址，再明确执行 Ptr 或 I32 的 load/store。data 字段中的指针不应包装成“源类型为整个 Vec 的 Direct Value”。

引用也应分清层次：引用 Value 的操作数指向目标 T；保存这个引用的槽包含一个 ptr；对引用值 materialize 得到的是保存引用的槽地址。后续 Clone/equality 的存储参数协议会依赖这个区别。

## 10. 源函数 ABI、调用与返回

### 10.1 建立统一 FunctionABI

FunctionABI 建议保存 symbol、ir_signature、source_result、source_parameters 和 indirect_result。源函数定义与调用点都应读取这一份描述。

| 源参数 / 返回值 | 计划的 IR 约定 |
| --- | --- |
| 四种整数 | i32 参数 / 返回 |
| bool | i1 参数 / 返回 |
| Reference、Box | ptr 参数 / 返回 |
| Unit | 参数不占 ABI 位置；返回为 void |
| 数组、结构体、Vec 参数 | 传入求值快照的 ptr，被调方复制到自己的参数槽 |
| 数组、结构体、Vec 返回 | 第一个参数为隐藏输出 ptr，函数返回 void |

例如 `fn edit(mut a: [i32; 2], n: i32) -> [i32; 2]` 可计划为以下签名形状：

```text
源签名：([i32; 2], i32) -> [i32; 2]
IR 签名：void @rx.fn.<NodeId>(ptr output, ptr array_snapshot, i32 n)
```

这些 ptr 参数先使用普通 LLVM 参数，不额外承诺 sret、byval 或 noalias 属性。实际寄存器和栈上传参应交给 Clang，IR 阶段只需固定模块内一致的调用协议。

### 10.2 参数初始化和函数体

`lowerFunction` 建议按以下顺序生成：

1. 通过 NodeId 取得 FunctionABI 和 FunctionInfo，开始 FunctionContext。
2. 若为聚合返回，先读取第一个 IR 参数并保存到 result_ptr。
3. 按源参数顺序及对应 LocalId 建立参数槽。
4. 非 Unit 参数用 valueFromABI 转成 Value，再 storeValue 到参数槽；Unit 不消耗 IR 参数位置。
5. 对函数体调用 lowerExpr。
6. 正常结果经 snapshot 后交给 emitFunctionReturn；NoReturn 不再补返回。
7. 结束函数上下文。

聚合参数传入的是调用方快照地址，被调方仍应复制到自己的局部槽。这样修改可变参数不会影响调用方，也不会通过改写参数槽破坏其他调用的实参。

`emitFunctionReturn` 应依据源返回类型处理：Unit 发出 ret void；Direct 返回计算态操作数；聚合写入隐藏输出地址后 ret void。返回聚合时不能把被调方局部槽地址交给调用方长期使用。

### 10.3 实参按源码顺序立即保存

普通调用应读取 call_target，按源码顺序生成 arguments；方法调用应先生成 receiver，再生成其余 arguments。每个参数正常结束后应立即 snapshot 并保存到 `vector<Value>`。若某个参数产生 NoReturn，后续参数与 call 均不再生成。

下面的程序应打印 1：

```rust
fn change(a: &mut [i32; 2]) -> i32 {
    a[0] = 9;
    0
}

fn first(a: [i32; 2], ignored: i32) -> i32 {
    a[0]
}

fn main() {
    let mut a = [1, 2];
    println_i32(first(a, change(&mut a)));
}
```

第一个实参应在 change 执行前保存数组内容；第二个实参中的引用则应指向原 a。若只保存第一个实参的地址，等所有实参结束后才复制，会错误地打印 9。

方法 receiver 的自动借用、重借用或解引用应使用它自己的 use 计划。普通 CallExpr 的 callee 只作为 Callable 读取，不应调用 lowerExpr 试图得到函数值。

### 10.4 发出调用与取得结果

`emitFunctionCall` 应根据 FunctionABI 分配必要的聚合输出槽，将隐藏输出地址放在实参最前，再追加所有非 Unit 参数的 ABI 操作数，最后发出 call。

聚合调用结果应通过 valueFromResultSlot 取得；标量结果通过 valueFromABI 包装；Unit 应返回 Value{}。builtin 也应接收已经求值的 `vector<Value>`，不能回到 AST 重算参数。

### 10.5 源 main 与机器 main

源 main 应作为普通内部源函数生成，源代码中的 `main()` 调用继续指向它。另行生成唯一导出的 `i32 @main()`，调用源入口后返回 0。

源 main 的 return 应只返回当前源函数调用；不能直接变成退出整个程序。机器入口应同样通过 beginFunction/endFunction 生成，其返回类型上下文为源 i32。

## 11. 语句、块与聚合构造

### 11.1 语句与绑定

建议按以下方式实现三个语句 visit：

| 节点 | 计划行为 |
| --- | --- |
| EmptyStmt | 返回正常 Unit |
| LetStmt | 先 lowerExpr(initializer)，正常后按 bindings/locals 建槽、保存值并登记 LocalId |
| ExprStmt | 调用 lowerExpr(expression)，交回正常或 NoReturn 结果 |

BlockExpr 应按源码顺序调用 lowerStatement。普通语句的正常值由块忽略；遇到 NoReturn 时立即结束当前块的后续生成。存在 tail 时，应 lowerExpr(tail)，正常后 snapshot 作为块的值；无 tail 时返回 Unit。

let 的 initializer 应先于新绑定的运行时槽登记完成。遮蔽使用不同 LocalId，不需要用名字覆盖原槽。

### 11.2 数组与结构体

正常结束的聚合构造应先建立专有结果槽，逐个写入内容，完成后通过 valueFromResultSlot 返回：

| 节点 | 求值与写入规则 |
| --- | --- |
| ArrayExpr | 按元素源码顺序求值、snapshot，再写入对应元素位置 |
| ArrayRepeatExpr | 元素表达式只求值一次，保存结果，再按类型 length 生成填充循环 |
| StructExpr | 按 AST initializer 的顺序求值，通过 field_order 找声明字段下标后写入 |

结构体初始化顺序不能按内存偏移重排。例如源程序先写第二个字段的初始化，再写第一个字段，副作用仍应依源码顺序执行；field_order 只决定写到哪个字段。

重复数组应使用已经验证的 Copy 条件。长度为 1 时可按语义许可使用非 Copy 元素；生成过程无需再次判断能力。即使长度为 0，元素表达式也应按语义规则求值一次。填充过程宜生成运行时循环，避免随长度展开大量 IR。

### 11.3 不正常结束的初始化前缀

若语义结果表明聚合构造不能正常结束，应仍按顺序生成初始化表达式，直到实际遇到 NoReturn。例如前一个字段打印、后一个字段 return 时，打印必须保留。

这种情况下不应预先为含 never 的聚合请求运行时布局。`flow.normal` 可用于选择“不准备聚合存储，只执行必要前缀”的路径；实际控制流仍由递归求值的 EvalResult 决定。

## 12. 标量运算与求值顺序

### 12.1 运算映射

建议通过 `emitScalarBinaryOp` 统一普通二元运算与复合赋值的标量部分：

| 源操作 | 计划的 LLVM 操作 |
| --- | --- |
| 整数 +、-、* | add、sub、mul i32 |
| 有符号 /、% | sdiv、srem |
| 无符号 /、% | udiv、urem |
| 整数负号 | `sub i32 0, value` |
| 位与、或、异或 | and、or、xor |
| 整数取反 | 与全 1 位模式 xor |
| bool ! | `xor i1 value, true` |
| 非短路 bool 位操作 | 在 i1 上执行 and、or、xor，求值两侧 |
| 左移 | 先将计数与 31 做 and，再 shl |
| 右移 | 同样屏蔽计数；有符号用 ashr，无符号用 lshr |
| 大小比较 | 按源类型选择 signed/unsigned icmp，bool 使用 unsigned |
| 整数之间 as | 保留 32 位位模式，更新 Value 的源类型 |
| bool 到整数 as | zext i1 到 i32 |

应依据 OperatorInfo.operand_type 选择运算符号性，而不是仅观察 I32。常量的 bits 应按 32 位位模式使用，打印负数或最小整数时无需重新进行可能溢出的宿主有符号运算。

整数回绕操作不应附加 nsw、nuw、exact 等额外承诺。规范排除的除零和有符号除法溢出不要求在本阶段新增 panic 支持。

### 12.2 引用目标的读取时机

非短路二元表达式应先完成 left 和 right 的表达式求值，再根据 `read_left_reference`、`read_right_reference` 读取目标内容。较早求得一个引用，只固定了指针；后一个操作数的副作用可能改变它指向的值。

排序比较应按 `ordering_reference_depth` 逐层读取到最终标量。相等比较应按 operand_type 请求相应的 Equal 运行时函数。IR 不应自行扩展一套对所有 Box 或多层引用通用的自动解引用规则。

相等比较中的空调整 BorrowShared 应允许保留 Place，供两侧求值完成后只读观察。比较不能误触发非 Copy 对象的移动，也不能为保存观察位置而深 Clone 对象。

### 12.3 赋值

普通赋值建议严格按以下顺序生成：

```text
lowerExpr(value) → snapshot 保存 RHS
  → lowerPlaceExpr(target) 求目标地址一次
  → storeValue → Unit
```

复合赋值建议采用：

```text
求 RHS 并保存
  → 求目标地址一次
  → 读取目标旧值，以及 operation 指定的 RHS 引用目标
  → emitScalarBinaryOp
  → 写回原位置 → Unit
```

任一步骤返回 NoReturn 都应阻止后续步骤。不能把复合赋值简单改写成会再次求值 target 的二元表达式。目标包含函数调用或索引副作用时，求值次数应仍为一次。

## 13. 控制流与结果合流

### 13.1 插入点与 EvalResult

Builder 的插入点应描述“当前还能向哪个块生成指令”；EvalResult 应描述“当前源表达式是否有正常结果”。控制流处理应同时维护这两个层面。

ret、br 和 unreachable 应清除当前插入点。预建但没有正常前驱的 merge/exit 块若仍保留，应以 unreachable 终结，该表达式返回 NoReturn，不读取其结果槽。Semantic 已检查的不可达源码无需继续生成执行指令。

### 13.2 if

建议按以下算法生成 if：

1. lowerExpr(condition)，若终止则直接传播。
2. 新建 then、else、merge 块，按表达式结果类型准备 StorageSlot。
3. 从条件求值结束后的实际插入点发出条件跳转。
4. 分别生成两个分支；正常分支保存结果并跳 merge，终止分支不写槽、不增加合流边。
5. 若至少有一条正常合流边，在 merge 通过 valueFromResultSlot 取得结果。
6. 若没有正常合流边，以 unreachable 终结 merge 并返回 NoReturn。

没有 else 时，else 路径应提供 Unit。结果为 Unit 时无需物理结果槽。bool 合流应使用 i8 槽，取得结果时转回 i1；引用合流保存已经调整好的指针；聚合合流应复制内容到结果槽。

示意 CFG 为：

```text
condition
  ├─ true  → then ── 正常结果写槽 ──┐
  └─ false → else ── 正常结果写槽 ──┤
                                   ↓
                                 merge → 取值
```

已 return/break/continue 的分支应走自己的终结边，不连接到此处 merge。

### 13.3 短路

`a && b` 应在 a 为 false 时直接得到 false；`a || b` 应在 a 为 true 时直接得到 true。另一条路径才求 b。

建议使用 rhs、shortcut、merge 三个块及一个 bool 结果槽。shortcut 路径写入固定值并合流；rhs 仅在正常结束时写入其结果并合流。因为 shortcut 始终提供正常路径，即使 rhs return，merge 仍可从 shortcut 取得结果。

不能预先生成 b 的执行再用 and、or 或 select 选择结果。rhs 中的打印、修改或跳转应只发生在对应运行时路径。

### 13.4 while、loop 与跳转

| 结构 | 计划的块与出口 |
| --- | --- |
| while | 前序块 → test，条件转 body/exit；body 正常结束回 test |
| while 的 continue | 跳 test，重新执行条件 |
| loop | 前序块 → body，body 正常结束回 body |
| loop 的 continue | 跳本 loop 的 body |
| break | 按 jump_target 找 LoopTarget，保存可选结果后跳 exit |
| return | 求可选返回值，交给当前源函数的 emitFunctionReturn |

LoopTarget 建议保存 continue_block、exit_block、result_slot、break_edge_count。该计数应统计生成的正常 break 边，用于判断 loop 的结果槽是否有来源，不表示运行时执行次数。

循环表应以 NodeId 为键。break 和 continue 应直接使用 jump_target，避免重新根据 AST 嵌套猜测目标。裸 break、裸 return 的值应作为 Unit 处理，不查找不存在的子表达式。

break 的值本身可能再次 return 或 break；只有它正常结束后，外层 break 才能写结果槽并增加出口数。没有正常 break 边的 loop 应返回 NoReturn，其预留 exit 块不能读取未初始化结果。

循环条件和循环体也可能创建多个块。回边应从递归生成结束后的实际插入点发出，不能假定 body 的初始块仍然可插入。

### 13.5 共用索引循环

建议提供 `emitIndexLoop(length, body)`，以计数槽构造 test、iteration、exit，用于重复数组、Clone 和 equality 的元素遍历。

计数槽 alloca 应进入函数前序区，初始化 0 应发生在本次循环开始处。body 若正常结束，应递增并回 test；若已发出终结指令，则不再追加递增。数组长度较大时，IR 规模应主要取决于循环体，而非元素数量。

结果槽的“所有正常前驱先写、合流后再读”应由这些生成算法保证。LLVM 检查与专门的分支执行测试共同验收这一约定，第一版不另加合流槽效果标注系统。

## 14. builtin、I/O、Box 与 Vec

### 14.1 区分三类运行支持

建议明确区分以下概念：

| 概念 | 例子 | 计划处理位置 |
| --- | --- | --- |
| 源语言 builtin | ArrayLen、BoxNew、VecPush、Clone | builtins.cpp 的 emitBuiltinCall |
| 固定外部支持与包装 | malloc、memmove、printf、rx.rt.println | runtime.cpp 的 emitRuntimeSupport |
| 按源类型生成的操作 | Clone、Equal、VecPush、VecRemove | RuntimeOp 登记表、队列和函数体生成 |

emitBuiltinCall 应接收 `CallableInfo` 与已经保存好的 `vector<Value>`，依据 `Builtin` 显式分派。ArrayLen 可以直接得到常量，VecLen 可以直接 load，不要求每个 builtin 都生成独立运行时函数。二元相等比较则可以直接请求 Equal 操作，不必经过源 builtin 调用。

### 14.2 外部函数与 I/O

底层外部函数应使用 [REIMU 支持的 libc 接口](../vendor/REIMU/docs/support.md)。建议首先登记这些签名：

```llvm
declare i32 @printf(ptr, ...)
declare i32 @scanf(ptr, ...)
declare ptr @malloc(i32)
declare ptr @memmove(ptr, ptr, i32)
```

若实现中确有非重叠复制需要，可另外登记 `ptr @memcpy(ptr, ptr, i32)`；聚合表示复制与 Vec 后缀移动统一使用 memmove 即可。长度和分配字节数应使用 ILP32 的 i32。

建议生成三个内部包装：

| 包装 | 计划行为 |
| --- | --- |
| `void rx.rt.print(i32)` | 用 `%d\00` 调用 printf，不补空格或换行 |
| `void rx.rt.println(i32)` | 用 `%d\0A\00` 调用 printf |
| `i32 rx.rt.get()` | 建立 i32 槽，scanf 写入，再 load 返回 |

格式串应作为包含终止零字节的 Global 保存。print/println 的源结果为 Unit，printf 自身的 i32 返回值不应成为源表达式的值。I/O 包装也应使用统一的函数上下文边界。

### 14.3 ArrayLen 与 Box

ArrayLen 应在 receiver 求值完成后返回其源数组类型的长度常量。即使长度已知，也应保留 receiver 中的函数调用和其他副作用。

BoxNew 应先接收已经保存的元素 Value，再按元素 StorageLayout.size 申请堆存储，调用 storeValue 初始化，返回源 Box 类型的 Direct 指针。Box 解引用应得到指向元素的 Place，Move 应搬运指针表示。

Box 的 Clone 应另行分配元素存储并递归 Clone 元素。不能因 Box 在 IR 中也是 ptr，就与共享引用的 Clone 使用同一种深度规则。

### 14.4 Vec 表示与基本操作

建议使用 `{data: ptr, len: i32, cap: i32}`，维持 `0 <= len <= cap`：

| 操作 | 计划算法 |
| --- | --- |
| VecNew | 建立结果槽，写 data=null、len=0、cap=0 |
| VecLen | receiver 求值后读取 len |
| VecIsEmpty | 读取 len，与 0 比较 |
| VecIndex | 求索引后读取 data，以 Storage(T) 为步长取得元素地址 |
| VecPush | 参数全部完成后读取元数据，必要时增长，再写元素并增加 len |
| VecRemove | 保存选中元素，搬移后缀，减少 len，取得保存的结果 |

push/remove 的 receiver 应先求地址，但元数据应由运行时函数在所有实参完成后读取。`v.push(v.len())` 和实参中的其他合法副作用应使用正确时刻的状态。

### 14.5 push 的增长路径

容量增长建议从 0 到 4，之后在满时翻倍：

1. 读取当前 len、cap；若 `len != cap`，直接进入写入路径。
2. 计算新容量及 `new_capacity × element_size`，申请新缓冲区。
3. len 非零时搬运 `len × element_size` 个字节，只复制有效元素。
4. 更新 data 和 cap；在第 len 个元素位置写入已保存的参数。
5. 写入 `len + 1`，返回 Unit。

空 Vec 的 data 可以为 null。第一次增长应跳过旧缓冲区复制，不依赖零长度内存调用对 null 的处理。增长不得读取 `[len, cap)` 中尚未初始化的内容，也不应逐元素调用 Clone。

传给 push 的聚合元素应已经保存快照。运行时函数可通过 `copyPlaceContents` 将元素表示写入缓冲区，不要求元素具备 Copy 或 Clone；源语义已经决定此处是否允许移动。

### 14.6 remove 的返回值与搬移

remove 建议使用调用方提供的输出地址：

1. 参数求值完成后读取当前 len，计算被移出元素地址。
2. 将元素内容保存到 output。
3. 计算 `remaining = len - index - 1`。
4. remaining 非零时，将后缀向前 memmove，长度为 `remaining × element_size`。
5. 写入 `len - 1`，返回；调用方通过 valueFromResultSlot 得到源结果。

保存返回元素必须早于后缀搬移。后缀源与目的区间可能重叠，应使用 memmove。移出末尾元素时应跳过搬移。首、中、尾删除和清空后重新 push 都应单独覆盖。

第一版不插入释放旧缓冲区或退出作用域清理的逻辑。增长和 Clone 仍应使用正确的字节数，并通过运行测试确认既定内存限制内的行为。

## 15. Clone 与相等比较

### 15.1 按源类型决定操作

Clone 和 Equal 应根据源 TypeId 及已验证的 capabilities 选择实现：

| 源类型 | Clone 的计划行为 | Equal 的计划行为 |
| --- | --- | --- |
| Unit | 无数据操作 | true |
| 整数、bool | 复制标量 | 比较标量 |
| 共享引用 | 只复制引用地址 | 递归比较所指目标 |
| 可变引用 | 使用 Semantic 的不可 Clone 约束 | 只读比较所指目标 |
| 数组 | 按元素 Clone | 顺序比较，首次不同即结束 |
| 派生结构体 | 按字段 Clone | 按字段比较，忽略 padding |
| Box | 新分配并 Clone 元素 | 比较元素内容 |
| Vec | 为有效元素建立独立缓冲区并 Clone | 先比 len，再比有效元素 |

共享引用的 Clone 不要求目标具有 Clone。Vec 的相等比较应忽略 cap 和缓冲区地址，结构体相等不应比较 padding。仅比较指针地址或整体 memcmp 都不能表达这些源语义。

显式源方法调用应遵循 call_target；内部派生操作递归应遵循 capabilities 和成员源类型。不能在生成内部 Clone 时再次按方法名查找，以免选中用户定义的同名方法。

Equal 可使用 PartialEq 能力表决定是否有对应实现；Eq 不需要另一套运行时函数。`!=` 可在 Equal 返回的 i1 上取反。

### 15.2 固定的存储参数协议

按类型生成的函数建议使用以下签名，独立于源函数的聚合 ABI：

| 操作 | 签名形状 |
| --- | --- |
| Clone<T> | `void clone(ptr destination, ptr source)` |
| Equal<T> | `i1 equal(ptr left, ptr right)` |
| VecPush<Vec<T>> | `void push(ptr vector, ptr element_snapshot)` |
| VecRemove<Vec<T>> | `void remove(ptr output, ptr vector, i32 index)` |

Clone 对标量和聚合都写入 destination，调用点再按源结果类型取 Value。Equal 的两个地址都应指向 Storage(T)。当 T 为引用时，helper 先从存储中 load 引用，再按 Clone 或 Equal 的规则处理；传入的地址不能少一层间接访问。

### 15.3 函数体生成

Clone 的标量与共享引用分支可复用 loadScalar/storeValue；结构体按声明字段生成递归调用；数组和 Vec 通过 emitIndexLoop 生成运行时遍历；Box 应分配后递归 Clone 元素。

Vec Clone 可令新 cap 等于 len。空 Vec 应得到 null/0/0；非空 Vec 分配 `len × element_size`，逐个 Clone 有效元素。新容器与原容器的拥有数据应独立，元素中的共享引用仍保留原指向。

Equal 的标量分支可直接返回 icmp；Reference 和 Box 先读取指针，再请求元素类型的 Equal。结构体每次字段比较成功后进入下一段，失败跳到统一的 false 返回块。数组和 Vec 应生成循环，并在第一次元素不同处结束；Vec 在循环前先比较长度。

运行时函数体生成器应自行读取本函数 IR 参数并终结所有保留路径。`emitRuntimeDefinition` 只负责开始函数、按 RuntimeOp 分派和结束函数，避免把上下文切换分散到各个递归调用点。

## 16. 按类型运行时函数的登记与延迟生成

### 16.1 登记键与符号

建议显式定义：

```cpp
enum class RuntimeOp { Clone, Equal, VecPush, VecRemove };
enum class VecField { Data, Length, Capacity };
```

运行时函数的键应为 `(RuntimeOp, SourceTypeId)`，符号可采用 `rx.rt.<operation>.<SourceTypeId>`。多个源类型虽然都降低为 ptr，深 Clone 与内容比较的含义仍可能不同，所以不能按 IR TypeId 去重。

`ensureRuntimeFunction` 建议按以下顺序工作：

1. 已存在键时，直接返回登记的符号。
2. 读取 Clone 或 PartialEq 所需能力，选择该操作的固定签名。
3. 向 Module 登记函数签名。
4. 先将键与符号写入 runtime_symbols_，再将键加入 pending_runtime_ops_。
5. 返回符号，让调用方继续生成当前函数。

### 16.2 队列展开

源函数和机器 main 全部完成后，`emitPendingRuntimeDefinitions` 应按索引遍历队列。每次生成前，应按值复制当前键；函数体可能继续登记依赖并扩容队列，不能持有指向队列元素的引用。

循环条件应读取当前队列长度，从而处理新追加的依赖。每个键只登记和生成一次，所有函数体仍使用独立 FunctionContext。

例如 `Node → Vec<Node> → Node` 的 Clone 依赖可按以下顺序解决：

```text
请求 Clone<Node>       → 登记签名并入队
生成 Clone<Node>       → 请求 Clone<Vec<Node>>，登记并追加
生成 Clone<Vec<Node>>  → 再请求 Clone<Node>，取得已有符号
队列排空
```

应在编译时通过登记表结束类型依赖展开，在运行时通过递归调用处理实际值。数组/Vec 元素遍历应保持为运行时循环。此设计同时应支持自递归与互递归类型，不需要在生成一个源函数时保存并切换多个活动上下文。

## 17. LLVM 文本输出与驱动接入

### 17.1 printer 的职责

writeLLVM 应只读取 Module，依次输出目标信息、命名结构体、全局字节数据和函数。建议拆分类型名、操作数、参数表、指令、函数等打印辅助函数。

| 内容 | 输出约定 |
| --- | --- |
| 参数、寄存器、块 | 分别使用 `%pN`、`%vN`、`bN` 等确定名称 |
| 指针常量 | 首版仅需 null |
| 整数常量 | 保留原位模式 |
| Global | 按字节转义，保留格式串尾部的零字节 |
| external 函数 | 输出 declare，不输出函数体 |
| 内部函数 | 输出 define internal |
| 机器 main | 输出导出的 define |
| alloca | 只在入口块普通指令之前输出 |
| 可变参数调用 | 输出明确的函数类型，如 `call i32 (ptr, ...) @printf(...)` |

printer 应打印模型中已经决定的类型、callee 和块目标，不重新推断源类型、不插默认返回，也不接受绕过模型的原始 LLVM 指令字符串。调用和函数定义应共用 Signature。

可在输出边界检查缺少终结指令或输出流失败；这不应演变为完整的内部 verifier。指令类型、SSA 支配和 CFG 的 LLVM 合法性应通过外部工具检查；求值顺序、快照和合流初始化还需运行测试。

### 17.2 阶段调度

驱动接入应明确每个 stage 的停止位置：

```text
解析选项 → Lexer / Parser → AST
                            ├─ ast：结束
                            ↓
                    semantic::analyze
                            ├─ semantic：结束
                            ↓
                         ir::lower
                            ↓
                       ir::writeLLVM
                            ↓
                        发布 .ll 文件
```

计划支持的 IR 调用形式为：

```sh
./.antlr/rx-compiler --stage ir input.rx -o output.ll
```

IR 模式应要求输出路径。参数解析应先识别选项及其值，再识别输入；未知 stage、缺少选项值、多余参数、源输出指向同一文件应作为调用错误处理。尚未提供的阶段应明确报告，而非静默停在前一阶段。

建议维持以下退出码协议：

| 退出码 | 含义 |
| --- | --- |
| 0 | 请求阶段成功；IR 阶段已完整发布输出 |
| 1 | 源程序的语法或语义拒绝 |
| 2 | 参数、文件或编译器实现错误，包括尚未支持的 lowering |

诊断应写 stderr，LLVM 文本应写指定文件。调试用 AST 输出应由显式选项控制，并与 IR 文件分开。

### 17.3 输出发布

驱动应先在内存中完成 lower 与 LLVM 序列化，再写目标目录内的临时文件。写入并关闭成功后，通过 rename 发布目标文件。

发生编译或写入失败时，应清理本次临时文件并保留原输出。调用者必须根据退出码判断本次结果，不能仅因旧文件仍存在就认定成功。路径、创建、写入和发布失败应在 CLI 测试中覆盖。

### 17.4 构建与运行器配置

实施时应将 IR 的源文件和头文件纳入构建依赖，并沿用工程的头文件搜索规则。源文件与依赖集合可扩展为：

```make
CPP_SOURCES := src/main.cpp $(wildcard src/ast/*.cpp src/semantic/*.cpp src/ir/*.cpp)
CPP_HEADERS := $(wildcard include/ast/*.hpp include/semantic/*.hpp include/ir/*.hpp)
```

若内部包含使用 `"lower.hpp"` 等短名称，应同时增加 `-Iinclude/ir`；使用目录前缀的包含应能通过 `-Iinclude` 找到。核心 IR 头文件应自行包含所需标准库，不借用前端头文件的间接包含。

在[运行配置](../config.mk)中，IR 命令计划配置为：

```make
IR = ./.antlr/rx-compiler --stage ir {source} -o {output}
```

该命令应使用可独立解析的编译器路径，不依赖仅在根 Makefile 定义的变量，以便[独立 OJ 入口](../tests/custom/oj/Makefile)加载。BUILD 与 SEMANTIC 应继续保持各自职责。

当 I/O 包装和按类型运行时函数均随 Module 输出时，无需另外生成共享 runtime 定义。以后若规划独立 C/汇编 runtime，应通过 BUILD 提供运行器所需文件，并避免与模块内符号重复定义或在单次 IR 编译中修改共享文件。

## 18. 第一个可运行闭环

### 18.1 先验证核心模型可以独立使用

最小核心 IR 建立后，应先构造一个与前端无关的函数。下面的片段约定放在 `rx::ir` 命名空间，ostream 按工程惯例引入：

```cpp
void writeSeven(ostream& out) {
    Module module;
    auto& function = module.declare("seven", {I32, {}});
    Builder builder(module);
    builder.begin(function);
    auto slot = builder.slot(I32);
    builder.store(constant(I32, 7), slot);
    auto value = builder.load(I32, slot);
    builder.ret(value);
    writeLLVM(module, out);
}
```

该例应只需要核心 IR 与 printer。它可先验证类型、编号、入口 alloca、load/store、ret 和序列化，再接入源语言。

### 18.2 打印 42

第一个源程序建议为：

```rust
fn main() {
    println_i32(42);
}
```

可以将以下完整 LLVM 示例作为目标形状。NodeId 17 仅为示意，最终符号应依据真实 AST 编号生成：

```llvm
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@rx.rt.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @rx.rt.println(i32 %p0) {
b0:
  %v0 = call i32 (ptr, ...) @printf(ptr @rx.rt.line, i32 %p0)
  ret void
}

define internal void @rx.fn.17() {
b0:
  call void @rx.rt.println(i32 42)
  ret void
}

define i32 @main() {
b0:
  call void @rx.fn.17()
  ret i32 0
}
```

实现时应从 AST 的 CallExpr 与 call_target 构造这些对象，并通过 Builder 输出真实指令。源 main、I/O 包装与机器入口从第一步就应使用统一 FunctionContext。

最小闭环的验收应包括空 main、打印 42、改成 43 和连续打印两行。这样可以确认常量、调用次数、换行和源码输入确实影响生成结果。

### 18.3 目标工具链预检

以下命令应在相关工具安装及生成文件准备完成后执行。带版本后缀的命令名可替换为同版本工具的绝对路径。

先确认实际 RV32 datalayout：

```sh
clang-22 --target=riscv32-unknown-none-elf -march=rv32im -mabi=ilp32 \
  -S -emit-llvm -x c /dev/null -o -
```

再检查 LLVM 示例或本次生成的 output.ll：

```sh
llvm-as-22 output.ll -o /tmp/rx-ir-check.bc
opt-22 -passes=verify -disable-output /tmp/rx-ir-check.bc
clang-22 -S -x ir output.ll -o output.s \
  --target=riscv32-unknown-none-elf -march=rv32im -mabi=ilp32 \
  -O0 -mllvm -riscv-no-aliases
python3 scripts/strip_asm_debug.py output.s
xmake run -P vendor/REIMU reimu --memory=256M --stack=1M \
  -f output.s -o output.out -p output.profile
```

打印示例的输出应精确为 `42\n`。LLVM 解析和校验成功只能证明 IR 结构满足工具要求，REIMU 输出还应验证实际行为。

### 18.4 建立 smoke 测试

建议在 `tests/custom/ir-smoke/` 建立 print42.rx、print42.out 和 manifest.json。输出文件应包含 42 和一个换行，manifest 可使用：

```json
[
  {
    "source": "print42.rx",
    "stage": "codegen",
    "compilation_success": true,
    "io": [{ "input": null, "output": "print42.out" }]
  }
]
```

[测试清单解析器](../scripts/test.py)以 codegen 条目派生 IR 用例，manifest 的 stage 不填写 ir。目录和驱动接入后，应运行：

```sh
make test STAGE=ir FILTER=custom:ir-smoke CLANG=clang-22
```

## 19. 测试分层与验收方法

### 19.1 接口、文件与执行分开验证

建议建立以下测试层次：

| 层次 | 计划验证的内容 |
| --- | --- |
| 核心 IR 接口 | 无前端依赖的构造与打印；终结后的插入约束 |
| lowering API | Value/Place/NoReturn、语义计划消费、模块所有权和会话隔离 |
| Visitor 接入 | 表达式与语句分类覆盖；递归经过约定入口；结果彼此独立 |
| CLI | 参数、阶段停止位置、退出码和输出文件发布 |
| LLVM | 每个生成模块可 assemble/verify，并能生成 RV32 汇编 |
| REIMU | 输出字节、状态变化、求值顺序和资源行为 |
| 前端回归 | IR 接入不改变 AST 与 Semantic 的既定结果 |

可在 `tests/custom/ir/` 规划 ir_only.cpp、api.cpp、cli.py 等接口测试文件。它们应单独构建或执行，不作为 Rx manifest 中的源程序条目。

核心 IR 测试应只包含 `ir/ir.hpp`，只链接 ir.cpp 和 printer.cpp，不引入 AST、Semantic 或 ANTLR。可以检查第 18 节的直线函数以及终结后禁止继续插入指令的边界。

### 19.2 lowering 与 Visitor 的关键检查

建议重点覆盖：

- Unit 正常完成，NoReturn 无正常值，never 结果槽本身不改变控制流。
- 引用 Direct、聚合 StoredAggregate 和 Place 即使都涉及 ptr，也保持各自含义。
- 销毁 AST 与 SemanticResult 后仍可打印 Module；连续编译不同程序后再次编译同一程序，输出保持一致。
- lowerAndWriteLLVM 与分别调用 lower、writeLLVM 的文本一致。
- 源函数、I/O 包装、机器入口和按类型运行时函数顺序生成时，局部槽、循环表及返回地址不残留。
- 22 个表达式 visit 和 3 个语句 visit 显式覆盖，分类 dispatch 不要求类型或声明 visit。
- 语义表中的本节点类型、use、operation、place、call_target 等信息按适用入口读取，不被生成过程改写。
- 投影根、索引及借用调整只执行一次，较早递归结果不会被较晚访问覆盖。

对必要语义记录缺失的人工构造输入，可检查其被识别为内部接口错误；不应将这些测试扩大为对每个 Semantic 规则的重复实现。错误 IR 的完整结构检查应交给 LLVM，不要求先建立内部 verifier 测试体系。

### 19.3 CLI 与发布行为

CLI 测试应覆盖正常 IR 输出、重复生成、ast/semantic 提前停止，以及缺少 source、未知选项、缺少 stage 值、缺少 -o 值、未知阶段、源输出同路径、不可写目标等情况。

还应先生成有效输出，再编译一个语法或语义错误的源程序，检查退出码为 1 且旧输出保持。参数或写入失败应返回 2。所有失败都应避免留下被误认作成功输出的半成品。

### 19.4 官方用例规模

以下数量来自编写本文时对测试清单的核对，用于安排未来验收，不代表阶段完成情况。实施时应重新读取 manifest，数量以实际清单为准：

| 类别 | 目录组数 | 程序数 | I/O 组数 | 计划用途 |
| --- | ---: | ---: | ---: | --- |
| semantic | 45 | 239 | 0 | 前端行为回归 |
| codegen | 38 | 60 | 115 | 自动派生的 IR 完整运行基线 |
| optimization | 13 | 13 | 39 | 可另行适配为 IR 功能压力测试 |

普通 `STAGE=ir FILTER=official:codegen` 应覆盖 codegen 派生用例，不包含 optimization 条目。若额外适配 optimization 用例，应另记为 IR 功能压力测试，仍使用 Clang -O0，不能据此宣称 Optimize 阶段完成。

建议按以下能力逐步定位官方用例：

| 能力 | 验收关注点 |
| --- | --- |
| 标量与 I/O | 有符号/无符号运算、移位屏蔽、cast、输入与换行 |
| 控制流 | 短路、never 分支、循环目标、跨分支与循环结果 |
| 调用 | 递归、大量参数、聚合 ABI、源 main 调用、大栈帧 |
| 引用 | 重借用、引用合流、标量引用操作、遮蔽后的地址身份 |
| 聚合 | 布局、初始化顺序、参数快照、临时投影、返回存储 |
| 容器 | Box 移动、Vec 增长/remove、嵌套容器和索引 |
| 派生操作 | 深 Clone、共享引用 Clone、内容比较和递归类型 |

同一用例可能同时使用多项能力。阶段性失败应定位缺失行为，不应通过删减或放宽用例改变验收要求。

### 19.5 自定义行为测试

补充测试应优先区分容易混淆的执行行为：

| 主题 | 建议场景与应观察的结果 |
| --- | --- |
| 实参快照 | 第一个聚合参数求值后，后一个参数修改原对象；第一个参数保留旧值 |
| 赋值顺序 | RHS 与目标索引都有副作用；RHS 先执行，目标求值一次 |
| 引用读取 | 两侧先取得引用，后一侧修改目标；外层按计划读取最终内容 |
| 遮蔽与递归 | 旧引用继续指向旧绑定，递归调用的局部槽互不覆盖 |
| 正常/终止合流 | 一条分支 return，另一条正常；标量、引用和聚合结果均正确 |
| 短路 | rhs 打印或跳转只在需要时执行；非短路 bool 运算执行两侧 |
| bool 布局 | bool/i32 交错字段及数组元素互不破坏 |
| 临时求值 | 返回聚合的投影与借用，函数与索引均只执行一次 |
| 聚合初始化 | 数组重复值只求一次，字段按源码顺序执行，return 前的副作用保留 |
| Vec | 多次增长、首中尾 remove、清空后重填、receiver 与参数副作用 |
| 表示搬运 | 非 Copy 元素的合法移动不触发深 Clone |
| Clone/equality | Box/Vec 拥有数据独立，共享引用保留指向，比较不依赖地址或 padding |
| 递归类型 | 自递归及互递归登记能结束，有限运行时值得到正确结果 |

测试程序应遵守有效执行范围，不能用非法借用、失效引用或 use-after-move 推导预期输出。日常回归宜检查结构和执行结果；只为少量教学示例固定完整 LLVM 文本，避免将寄存器编号变成大量脆弱的黄金文件。

### 19.6 接入后的完整验收

相关实现与测试目录建立后，计划在仓库根目录执行：

```sh
make build
python3 scripts/test_ast.py
make test STAGE=semantic FILTER=official:semantic
make test STAGE=ir FILTER=custom:ir-smoke CLANG=clang-22
make test STAGE=ir FILTER=official:codegen CLANG=clang-22
CLANG=clang-22 make -f tests/custom/oj/Makefile test STAGE=ir FILTER=official:codegen
```

还应运行独立接口/CLI 测试、全部自定义行为回归，并对本次生成的所有 .ll 执行 llvm-as 与 opt verify。批量 LLVM 检查应只选本次测试目录，避免把历史产物重复计数。

运行器会在 `target/tests/run-*` 保存分阶段产物。排查顺序建议为：编译退出码与语义信息 → LLVM 指令/签名和目标选项 → REIMU 符号与汇编 → 第一次输出差异 → 循环、分配和资源行为。

验收记录应保存工具版本、测试清单版本、目标参数、实际命令、程序/I/O 数量及失败产物位置。完成标准应包括实际 RV32 执行结果；仅通过 Semantic、成功写出 .ll 或通过 LLVM 校验都不足以完成整个阶段。

## 20. 实施顺序与完成标准

### 20.1 建议里程碑

| 阶段 | 计划实现 | 验收重点 |
| --- | --- | --- |
| M0：最小闭环 | 核心 IR、Builder、printer、基础 Value/FunctionContext、I/O、双 main、驱动 | 空 main、42、43、两行输出，真实 RV32 执行 |
| M1：标量与绑定 | 标量布局、Visitor 入口、let、读写、snapshot、运算、cast、赋值 | bool 存储、整数符号性、移位、求值顺序 |
| M2：控制流 | if、短路、while/loop、跳转、结果槽 | 正常/终止分支、嵌套目标、短路副作用 |
| M3：函数与引用 | 统一 ABI、参数槽、递归、借用/解引用/重借用 | 大量参数、递归隔离、源 main 调用、地址身份 |
| M4：聚合与方法 | 数组/结构体布局、构造、投影、聚合 ABI、临时物化、方法调用 | 参数快照、返回存储、字段顺序、临时单次求值 |
| M5：Box 与 Vec | 堆分配、容器 builtin、VecPush/VecRemove 登记与生成 | 增长、移出结果、嵌套容器、元数据读取时机 |
| M6：Clone 与 Equal | 能力消费、按源类型生成、递归依赖队列 | 深复制、内容比较、共享引用、递归类型 |
| M7：完整接入 | 接口/CLI、LLVM、执行、前端回归、独立 OJ 入口 | 官方 60 个程序、115 组 I/O 及关键自定义用例 |

布局、值表示、存储操作和函数上下文应在早期确定，再逐步扩充支持的类型。实施期间尚未支持的节点应作为实现未完成报告，不能默认返回 Unit 或输出不完整函数。

### 20.2 审阅时应能解释的关键点

- 为什么源变量可重新赋值，而 SSA 寄存器只定义一次？
- 为什么 Lowerer 只需要一个 Visitor 基类，却仍要区分表达式与语句 dispatch？
- lowerExpr、lowerExprCore、lowerPlaceExpr 各自跳过或保留哪些步骤？
- 为什么递归返回值放在局部变量中，而语义结果表保持只读？
- 为什么 snapshot 与 valueFromResultSlot 对聚合存储的前提不同？
- Unit、无物理结果槽与 NoReturn 各表示什么？
- 引用值、保存引用的槽、解引用后的 Place 分别指向哪一层？
- 为什么较早实参需要快照，而相等比较有时需要保留观察位置？
- 为什么聚合赋值必须写原槽，聚合返回必须使用调用方存储？
- 为什么运行时函数要按源 TypeId 登记，并在生成函数体前登记？
- 哪些错误由 Semantic、Builder、LLVM 校验和运行测试分别发现？

### 20.3 完成清单

- [ ] 核心 IR 与 printer 可脱离前端构建；Module 独立拥有全部 IR 数据。
- [ ] 三个头文件和十一个实现文件职责清楚，源类型与 IR 类型身份分开。
- [ ] Lowerer 使用分类 ASTvisitor，显式覆盖表达式与语句，递归经过约定入口。
- [ ] SemanticResult 只读消费，use 应用一次，完整投影与临时根不重复求值。
- [ ] Place、Value、StorageSlot、NoReturn 的边界落实到存储和控制流算法。
- [ ] 模块状态与函数状态分离，所有函数种类使用统一上下文生命周期。
- [ ] RV32 布局、bool 的 i1/i8 转换、聚合 ABI 与源 main 包装一致。
- [ ] 实参、赋值、引用读取、字段初始化与短路保持规定顺序。
- [ ] 所有保留块正确终结，合流结果只在具有正常来源的路径读取。
- [ ] Box/Vec 的表示搬运、Clone 与内容比较遵循各自语义。
- [ ] 按类型运行时函数的登记去重、延迟生成和递归依赖均能正确结束。
- [ ] 驱动、构建、输出发布与独立运行器配置完成，错误退出码正确。
- [ ] C++17 构建、接口/CLI、LLVM、RV32 执行及前端回归全部通过并保存记录。
