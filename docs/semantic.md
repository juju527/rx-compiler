# Semantic 阶段实施指南

Semantic 模块使用 C++17，位于 `include/semantic/` 和 `src/semantic/`，命名空间为 `rx::semantic`。分析入口接收现有 AST，由一个 Checker 完成检查，将结果保存在 `Checker::result` 中，最终返回 `SemanticResult`。

本指南说明模块职责、数据结构、检查流程、信息的产生与使用方式，以及实现顺序和验收标准。类型等价、名字可见性、转换和访问权限属于语言语义；容器选型、类型驻留、状态编码和文件划分属于实现设计。

## 1. 实现基础与输入契约

| 基础设施 | 约定 |
| --- | --- |
| [AST 定义](../include/ast/ast.hpp) | 提供节点 ID、类型语法、receiver、`has_lifetime` 和表达式树；semantic 接收只读 AST |
| [AST builder](../src/ast/builder.cpp) | 完成语法归一化，保留源码顺序、尾表达式和整数原始拼写；成功 AST 的必需子节点非空 |
| 类型设施 | Types 管理类型身份、驻留和显示；语法类型通过 resolveType 转为 TypeId |
| 语义结果 | SemanticResult 拥有类型、声明、局部绑定和全部表达式的检查结果 |
| 检查状态 | Checker 管理名字环境、当前函数、常量依赖和循环上下文；每次分析独立创建 |
| [Driver](../src/main.cpp) | 管理阶段选择、源码定位、analyze 调用、诊断和 dump |
| [Makefile](../Makefile) | 使用 C++17，编译 AST 与全部 semantic 源文件，跟踪相应头文件依赖 |
| [测试配置](../config.mk) | BUILD 生成项目编译器，SEMANTIC 调用其语义检查入口；各阶段分别接入 |

AST 子节点沿用 shared_ptr 所有权和现有 Visitor 接口。源码位置由前端适配层保存，semantic 通过 NodeId 关联诊断，不依赖 ANTLR 对象。参数、字段和路径段等没有独立 NodeId 的辅助结构，通过所属节点及下标定位。

## 2. 总体结构：一个 Checker，按职责分文件

### 2.1 对外入口与边界

```cpp
namespace rx::semantic {
SemanticResult analyze(const AST::Crate& crate);
void verify(const AST::Crate& crate, const SemanticResult& result);
}
```

`analyze` 每次创建新的 `Checker`，调用 `run`，成功时返回独立拥有数据的结果。语义拒绝抛 `Diagnostic`；未实现节点、失效 ID、缺失内部记录等属于实现错误。检查在首个语义错误处终止，不需要 ErrorType、多错误恢复或全局错误状态。

输入 AST 的 NodeId 必须非零且唯一，同一个节点不能被挂到多个语法位置。虽然子节点用 shared_ptr 保存，当前语义契约仍是树。结果对象不保存 AST 指针、ANTLR 指针或 Checker 内部容器的指针；后续 IR 同时接收 AST 和 SemanticResult。

本阶段负责名称解析、类型、受限常量、位置权限、能力、调用选择和使用计划。不实现借用/生命周期数据流、use-after-move 证明、一般泛型、用户 trait 系统、通用常量折叠或 LLVM IR 生成。

### 2.2 文件职责

各实现文件定义同一个 Checker 的成员函数，共享结果对象、名字环境和检查上下文。跨文件调用直接在本次检查会话中进行。Types 提供类型设施，verify 独立读取成功结果并检查内部不变量。

| 文件 | 负责的函数或结果 |
| --- | --- |
| `include/semantic/types.hpp`、`src/semantic/types.cpp` | 类型身份、驻留、构造、格式化；不处理作用域 |
| `include/semantic/semantic.hpp` | 对外结果结构、analyze、verify、Diagnostic |
| `include/semantic/checker.hpp` | 检查期间的共享状态和内部函数声明 |
| `src/semantic/checker.cpp` | analyze、run、fail、入口检查、最终能力补全和 verify |
| `src/semantic/declaration.cpp` | collect、addMember、typeSegment、resolveType、lookupValue、resolveName、bind、declarations、checkLayouts |
| `src/semantic/constants.cpp` | integerLiteral、evaluateConstant、constExpr |
| `src/semantic/capabilities.cpp` | capable、checkDerives |
| `src/semantic/body.cpp` | 函数环境、表达式分派、语句、作用域、聚合和控制流；value、sequence、alternatives |
| `src/semantic/places.cpp` | materialize、dereference、requireMutable、field、index |
| `src/semantic/conversions.cpp` | coercion、use、consume、merge |
| `src/semantic/operators.cpp` | 一元、二元、赋值、cast 与 OperatorInfo |
| `src/semantic/calls.cpp` | callable、associated、methods、call、methodCall |
| `src/semantic/verify.cpp` | 成功结果的节点覆盖、字段归属和引用完整性 |
| 前端适配层和 `src/main.cpp` | 建树与定位、CLI、异常转换、语义结果显示；dump 可独立成文件 |

analyze 和 run 统一在 checker.cpp 中定义。新增文件按上表分配职责，并同步纳入构建。

### 2.3 共享状态的生命周期

| Checker 成员 | 由谁建立 | 何时读取或更新 | 是否进入返回结果 |
| --- | --- | --- | --- |
| `result` | Checker 创建时 | 各模块填充，verify 检查 | 是 |
| `type_names` | collect | typeSegment 查基础类型和结构体 | 否 |
| `scopes` | 初始有全局层；collect 填充 | 名字查询、函数参数、块和 let；常量求值暂时隔离局部层 | 否 |
| `functions` / `constants` / `structures` | collect / addMember | declarations、常量依赖、functionBody 使用 AST 与 Self 上下文 | 否 |
| `ConstSyntax::state` | 默认未访问 | 常量 DFS 的未访问、求值中、已完成三态 | 否 |
| `current_function`、`self_type`、`return_type` | functionBody | bind、类型解析、return 检查 | 否；必要身份写入 Local / ExprInfo |
| `next_local` | 从 1 开始 | 每次 bind 分配，切换函数也不复位 | 否；分配结果成为结果表的键 |
| `loops`、`loop_barrier` | 循环和 while 条件检查 | 定位跳转、收集 break 值和阻止越过条件边界 | 否 |

`Checker::functions` 保存检查所需的语法，`result.functions` 保存可供调用和 IR 使用的签名与参数绑定；二者用途不同，不是重复的函数结果表。constants 也有同样区分。

## 3. 数据模型：确定类型身份，再确定结果归属

### 3.1 Types 与 ID

使用 `NodeId`、`TypeId`、`LocalId` 三种逻辑身份：函数、结构体、常量和循环使用 AST 定义节点的 NodeId；局部变量单独分配 LocalId；字段号是所属结构体 fields 的下标。底层均为 size_t 是实现选择，使用时仍须区分含义。

| TypeKind | 类型身份需要的信息 |
| --- | --- |
| Unit、Never、Bool、I32、U32、ISize、USize | 固定 TypeId；四种整数互不相等，源语言整数宽度均为 32 位 |
| Struct | declaration NodeId；同字段的两个结构体仍是不同类型 |
| Reference | element TypeId、mutable_reference；生命周期不参与驻留 |
| Array | element TypeId、已求出的 uint32_t length |
| Box、Vec | element TypeId |
| Unknown | 内部未指定标记，不是源程序可存储的类型 |

复合类型通过 `Types::intern` 的键 `(kind, element, length, declaration, mutable_reference)` 去重。结构体先驻留身份，字段保存在 StructInfo；创建 `Vec<Node>` 时不展开 Node 字段。`format` 只显示类型，不参与名称解析。

成功结果中的普通表达式、局部变量、签名和已解析类型节点不能残留 unknown。合法例外要精确表达：Callable 表达式没有运行时函数类型，`ExprInfo::type` 可以是 unknown；顶层函数的 `CallableInfo::self` 也可以是 unknown。不要为消除这两个标记而发明一等函数类型。

### 3.2 SemanticResult 的结果表

Checker 将各阶段产生的信息写入 result，分析完成后将整个 SemanticResult 交给调用方。

| 字段 | 键 / 内容 | 生产者 | 使用者 |
| --- | --- | --- | --- |
| `types` | TypeId → Type | Types 构造和各模块的类型请求 | 所有检查、dump、verify、IR |
| `entry` | 顶层 main 的 NodeId | run | driver / IR 入口 |
| `structs` | 声明 NodeId → StructInfo | collect 建身份；declarations 补字段和 derives；addMember 补 members | 类型、字段、方法、布局、能力检查和 IR |
| `functions` | 声明 NodeId → FunctionInfo | declarations 写 signature；functionBody 补 parameters | 调用检查、入口、IR |
| `constants` | ConstItem NodeId → Constant | evaluateConstant | 常量引用、长度查询、IR |
| `locals` | LocalId → Local | bind | 局部路径、存储权限、IR 分配局部存储 |
| `bindings` | LetStmt NodeId → LocalId | statement 调 bind 后登记 | verify、IR 定位 let 的新绑定 |
| `resolved_types` | Typenode NodeId → TypeId | resolveType | 再次类型查询、verify、IR |
| `expressions` | **所有 Exprnode** 的 NodeId → ExprInfo | expression、constExpr、负字面量特殊路径；父节点补 use / temporary_type | 所有后续表达式检查、dump、verify、IR |
| `capabilities` | 每项能力的 TypeId → bool | capable；run 最后补全 | Copy/Move、重复数组、clone、相等比较、IR |

参数和 receiver 没有单独的 LetStmt：它们的 LocalId 按顺序放在 `FunctionInfo::parameters`，receiver 在最前；Local.declaration 使用函数 NodeId。不能把所有参数塞到以函数 NodeId 为单一键的 bindings 中。

`field_indices` 是从 fields 推导出的名字索引，属于查找便利性；`capabilities` 是能力结论，不包含已生成的 clone/equality 函数。后续 IR 可用类型结构、字段和能力结论生成 helper，不要求 semantic 再建立一套重复的派生操作表。

## 4. ExprInfo 的写入协议

### 4.1 每个字段表示什么

| 字段 | 含义与有效范围 | 主要写入位置 | 后续用途 |
| --- | --- | --- | --- |
| `type`、`category`、`flow` | 本节点检查得到的类型、Value/Place/Callable、正常及跳转出口 | expressionImpl 对应处理函数，或 constExpr | 父节点检查、控制流和 IR |
| `place` | 仅 Category::Place 有效；根、投影和权限 | 局部 PathExpr、dereference、field、index | 赋值、借用、receiver、IR 地址计算 |
| `callable` | 表达式自身代表的可调用对象，仅 Callable 类别有值 | 函数 / builtin 的 PathExpr | call 读取目标签名 |
| `call_target` | 本调用实际选中的目标；CallExpr / MethodCallExpr 有值 | call / methodCall | IR 生成调用 |
| `use` | 直接父节点如何使用本表达式；目标类型、方式、调整链 | use 或 borrow、comparison、methodCall 等专用路径 | IR 应用使用计划 |
| `operation` | 本节点的操作数类型、引用读取标志和排序引用深度 | operators；constExpr 的受限负号 | 运算实现、verify |
| `resolved_name` | PathExpr 解析出的局部、常量、函数或 builtin 身份 | expressionImpl / constExpr | 稳定目标查询，避免 IR 重新查名 |
| `constant_value` | 当前实现确实求出的常量值 | 整数字面量、常量引用、受限 const、负整数字面量 | 长度、常量使用和 IR |
| `field_order` | StructExpr 的源码初始化顺序 → 声明字段下标 | structure | 按源码顺序求值，按声明字段存储 |
| `jump_target` | return 的函数，break/continue 的循环 | 对应跳转表达式 | IR 找到目标控制流块 |
| `temporary_type` | 父节点要求物化本值时的原始存储类型 | materialize | 为该表达式建立临时存储 |

附加信息使用 std::optional。空结构体的 `field_order` 是**有值的空 vector**；没有 field_order 的其他表达式用 nullopt。`UseInfo` 不重复保存表达式 ID，ID 已在 expressions 的键中。

`Category::Callable` 是类别名，`CallableInfo` 是结构体名。`f(1)` 中 `f` 的记录有 callable，整个调用的记录有 call_target；这两份信息属于不同节点，不能合并成一个含义不明确的字段。

### 4.2 检查与使用分两步

普通消费路径：

```text
父节点调用 consume(child, expected)
  → expression(child, expected)
      → expressionImpl 分派并返回该节点自己的 ExprInfo
      → 清除本节点 use，保存 expressions[child.id]
  → use(info, child.id, expected)
      → coercion + 位置权限 + Copy 能力
      → 补 expressions.at(child.id).use
  → 返回表中已补全的记录副本
```

`expected` 参与允许的局部定型；`type` 是完成该节点检查后得到的源类型，`use->target` 是父节点最终使用的目标类型。前者不是“完全不考虑上下文的类型”，但不能被后续 coercion 覆盖。

父节点可以直接补子节点，是因为 AST 是树，每个表达式只有一个直接使用位置。使用计划直接保存在该子节点的 ExprInfo.use 中，无需独立的使用记录表或使用位置键。检查子节点应先保存自身结果，再补 use / temporary_type；补完后不要用旧副本覆盖整条记录。

例如：

```rust
fn main() {
    let mut x = 1;
    let p = &mut x;
    let q: &i32 = p;
}
```

最后一行的 `p` 是独立 PathExpr。路径检查写入 `type = &mut i32`、局部 resolved_name 和 place；statement 读取注解后经 consume/use 写入 `target = &i32`、BorrowShared，以及 Dereference → BorrowShared 调整。原 type 仍是 `&mut i32`。

没有固定目标的 `let q = p;` 按源类型决定 Copy/Move；对于 `&mut i32` 是 Move。固定目标为相同 `&mut i32` 时，普通转换路径记录可变重借用。两者不能因 TypeId 相同而混为一谈。

### 4.3 统一结果存储，不等于统一检查入口

- constExpr 直接写 expressions，包括 ArrayType.length 和 ArrayRepeatExpr.count；这些表达式不经过普通 expression 分派。
- 负整数字面量的父节点和 magnitude 子节点都要有记录。最小有符号数的 magnitude 允许保留相应位模式，不可再按独立正数字面量范围错误拒绝。
- 普通 BoolLiteral 当前只产生 bool 类型；受限常量路径中的 BoolLiteral 才由 constExpr 记录常量值。不能据此扩展为“所有能算出的表达式都折叠”。
- Callable、仅用于投影的 base、受限常量等可以没有普通 use。verify 不要求每个表达式都有 use。
- 裸 break 的隐含 unit 需要参与 loop 公共类型计算时，只用 `PendingUse{0, value(unit)}` 表示，不产生 expressions[0]；break 节点自身仍有 never 类型、跳转目标及需要时的使用计划。

### 4.4 新表达式不得继承来源的专属信息

解引用、字段和下标访问创建自己的 ExprInfo，只保留必要的类型、控制流与 place。不能复制 base 的 resolved_name、constant_value、callable、call_target、operation、field_order 等。

```rust
struct Pair { a: i32, b: bool }
fn main() {
    let n = (Pair { b: true, a: 7 }).a;
}
```

结构体构造节点保存 `field_order = {1, 0}`；field 检查需要地址时，在该构造节点补 `temporary_type = Pair`。构造节点仍是 `type = Pair`、Category::Value。外层 `.a` 是 i32 Place，根指向该临时表达式，path 含字段 0；它没有 field_order。

`materialize` 返回的工作副本可以转为 Place，但持久表中的原表达式不能被改成 Place，也不能把 temporary_type 写成 use->target。`make().a` 中的 call_target 同样只留在 make() 节点。

## 5. run 的阶段顺序与每阶段保证

```text
analyze → 新 Checker → run
  collect
  declarations
  checkLayouts
  checkDerives
  检查顶层 main 并写 entry
  逐个 functionBody
  为全部已驻留类型补齐四种能力结果
  verify(crate, result)
  返回 result
```

| 阶段 | 可以依赖什么 | 完成后保证什么 |
| --- | --- | --- |
| collect | AST、基础类型设施 | 所有结构体身份、顶层函数/常量、impl 成员已登记 |
| declarations | 已登记的所有名字；可递归请求常量 | 常量已求值，字段、derive 请求和函数签名完整 |
| checkLayouts | 全部字段类型 | 所有结构体内嵌布局无环 |
| checkDerives | 全部字段与 derive 请求 | 请求的能力满足依赖，能力查询可以缓存 |
| main 检查 | 全局值层和完整签名 | entry 指向顶层函数，参数为空、返回 unit、没有生命周期参数 |
| functionBody | 全部声明、能力和入口 | 所有函数体均检查；局部、表达式和使用计划完整 |
| 能力补全 | 函数体可能驻留的新引用/数组等类型 | 每个有效 TypeId 的四项能力可查 |
| verify | 成功语义结果 | 约定的内部完整性检查通过 |

入口直接从 `scopes.front()` 查询，不能让局部遮蔽影响 main 选择。源码 main 仍是普通可调用函数，不在名称解析或普通调用中禁止它。

这些是一次会话里的阶段，不是只能单向调用的模块流水线。例如 resolveType 请求 constExpr，constExpr 请求 evaluateConstant，后者又可能解析声明类型；函数体也会继续驻留新类型和解析局部注解。

## 6. declaration.cpp：从名字登记到完整声明

### 6.1 collect / addMember：先登记身份

按以下三个遍历组织 collect：

1. 注册基础类型、I/O builtin；遍历全部 StructItem，建立名义 TypeId、type_names、structures 和初始 StructInfo。
2. 遍历顶层函数、常量，由 addMember 保存 FunctionSyntax / ConstSyntax，并登记到全局值层。此时不检查函数体、不要求字段和签名已完成。
3. 遍历 ImplItem，解析目标并确认是用户结构体；将所有成员登记到该 StructInfo.members。多个 impl 共用同一个关联值空间。

类型空间、值空间、字段空间、关联值空间分别处理重名。顶层函数和常量共用值空间；字段与方法可以同名。结构体声明的类型名冲突检查使用 type_names 和 protected_names；顶层 I/O 名字通过预先登记到全局值层受到保护。保护名检查必须限定所在名字空间，不能扩大成“所有位置都不能使用这些字符串”。规范列为 UB 的输入不作为必拒绝规则的证明。

addMember 的 self 参数是所属结构体 TypeId；unknown 表示顶层。FunctionSyntax.self 和 ConstSyntax.self 固定记录声明环境，后续解析 `Self` 时使用它。顶层函数带 receiver 应在登记阶段拒绝。

### 6.2 typeSegment / resolveType：把语法类型变成 TypeId

- typeSegment 解析一个路径段：Box/Vec 必须恰有一个具体类型参数；其他受支持类型不接受具体类型参数；Self 从参数 self 取得；普通名字查 type_names。
- resolveType 处理 UnitType、单段 PathType、RefType、ArrayType，递归解析子类型并调用 Types 构造函数，最后写 `result.resolved_types[node.id]`。
- ArrayType.length 调 `constExpr(length, usize, self)`，得到 bits 后驻留数组类型。长度表达式结果同时写入 expressions。
- resolved_types 按 NodeId 缓存，是因为一个语法类型节点只属于固定声明或表达式位置；不能把同一节点拿到不同 Self 环境重复使用。
- 值路径中的具体类型实参也是 Typenode，必须通过 resolveType 记录，例如 `Vec::<i32>::new()` 的 i32。

依赖示例：

```rust
struct Buffer { data: [i32; Self::N] }
impl Buffer { const N: usize = LIMIT; }
const LIMIT: usize = 2;
fn take(b: Buffer) {}
fn main() { take(Buffer { data: [0; 2] }); }
```

collect 先建立 Buffer 身份，再登记 N 和 LIMIT。declarations 请求常量值并解析字段；字段长度的 Self 使用 Buffer 的 TypeId，解析到成员 N。N/LIMIT 的声明值进入 constants，各自初始化表达式及 `Self::N` 长度路径进入 expressions，整个数组类型节点进入 resolved_types。后续 structure 检查字段值，call 检查 take 的参数，无须再查字符串字段类型。

### 6.3 lookupValue / resolveName：查询与记录分开

lookupValue 从 scopes 的最内层向全局层查询单段名字。找到后立即返回；若名字是局部变量，调用失败或常量求值失败都不能回退搜索同名全局函数/常量。

resolveName 处理单段值名，或 `类型::成员` 两段路径。两段路径先经 typeSegment 确定 owner，再查 StructInfo.members；不把路径拼接成一个全局字符串键。

resolveName **返回 ResolvedName，不直接写 ExprInfo**。调用它的 PathExpr 检查或 constExpr 决定如何使用结果，并在该节点记录 resolved_name。Box/Vec 等内建关联操作由 calls.cpp 的 associated 实例化；普通表达式路径会先尝试 associated，再进入普通名称解析，常量路径只接受常量声明。

### 6.4 bind：可见名字与存储身份分别保存

bind 分配新 LocalId，写 `result.locals[id] = Local{name, type, mut, current_function, declaration}`，再让 `scopes.back()[name]` 指向新 LocalId。它不自行写 bindings，也不追加 FunctionInfo.parameters；这两项分别由 statement 和 functionBody 完成。

```rust
fn main() {
    let x = 1;
    let x = x + 1;
}
```

第二个 initializer 先在旧环境中检查，内部 PathExpr 指向第一个 LocalId；之后才 bind 第二个 x。当前作用域覆盖名字映射，但 locals 中两条记录都保留，IR 据此区分存储。退出块只弹作用域，不删除已产生的语义结果。

### 6.5 declarations / checkLayouts：补全结构与签名

declarations 主动求所有常量，再完成结构体字段及 derive 请求，最后解析所有函数签名。必须检查未使用声明。数组长度查询仍可按需递归求常量，不能另加“类型解析期间禁止常量查询”的限制。

字段按声明顺序 push 到 fields，field_indices 记录名字对应下标并发现重复。derives 先记录请求并检查重复，其可行性由 checkDerives 决定。

| receiver 写法 | signature.parameters 首项 | functionBody 建立的 self 绑定是否可重绑 |
| --- | --- | --- |
| 无 receiver | 不添加首项 | 不建立 self |
| `self` | Self | 否 |
| `mut self` | Self | 是 |
| `&self` | `&Self` | 否 |
| `&mut self` | `&mut Self` | 否；解引用后的写权限由 Place 规则决定 |

其余参数解析为 TypeId 并检查名字重复；省略返回类型等价于 unit。declarations 此时只写 signature，FunctionInfo.parameters 留空，等 functionBody 分配 LocalId。源函数 body 缺失属于 AST 契约错误。

checkLayouts 对结构体定义做 DFS：沿 fields 和数组 element 递归，遇到引用、Box、Vec 停止。未访问/访问中/已完成三态足够发现内嵌环；状态容器和整数编码是实现选择。`struct Bad { next: Bad }` 拒绝，`struct Node { next: Box<Node> }` 布局可接受。即使某个坏结构体只被 Box 引用，它自己的定义仍需单独检查。

## 7. constants.cpp 与 capabilities.cpp：两类不同的递归

### 7.1 字面量和受限常量

integerLiteral 根据后缀、允许的 expected type 或默认 i32 定型，去后缀与下划线，按进制读取 magnitude，先检查范围再形成 uint32_t 位模式。有符号最小值通过负字面量专用路径处理，不能先把 magnitude 放进宿主 int32_t。宿主 size_t 的位宽不决定 Rx usize 的位宽。

constExpr 只接受当前受限范围：整数、布尔、常量路径，以及作用于整数字面量或常量路径的受限负号。括号已由 AST 消去；不增加一般算术、调用、局部值或任意表达式求值。已有常量的类型由其声明固定，用作数组长度时必须是 usize，不能把 i32 常量重新推断成 usize。

evaluateConstant 的工作顺序：

1. 已完成则读取 result.constants；访问中则报告依赖环。
2. **在解析声明类型之前**标记访问中，以捕获经数组长度回到自身的依赖。
3. 暂时只保留全局值作用域，用 ConstSyntax.self 解析声明类型和 initializer。
4. 正常完成后恢复调用方作用域，写 result.constants，标记完成。

切换到常量的声明环境，和“所有常量形式都只查全局”不是同一回事：

```rust
const N: usize = 2;
const M: usize = N;
fn main() {
    let N = 3usize;
    let a = [0; M];
}
```

M 的定义使用全局 N，因此长度为 2。如果把最后一行改成 `[0; N]`，该长度路径在当前局部环境中解析到 local，受限常量检查应拒绝，不能忽略遮蔽寻找全局 N。

当前首错终止设计下，异常会销毁整个 Checker，无须恢复后继续检查。若未来增加错误恢复，才需要保证作用域和求值状态在异常路径也恢复；不能在现有实现中吞掉异常后继续使用半成品状态。

### 7.2 能力图与 derive

能力查询必须发生在结构体字段和 derive 请求完整之后。对每项 Capability 分别建依赖，缓存到 result.capabilities。

| 类型 | Copy | Clone | PartialEq / Eq |
| --- | --- | --- | --- |
| 整数、bool、unit | 有 | 有 | 有 |
| `&T` | 有 | 有，不依赖 T 的 Clone | 依赖 T 的对应能力 |
| `&mut T` | 无 | 无 | 依赖 T 的对应能力 |
| `[T; N]` | 依赖 T | 依赖 T | 依赖 T |
| `Box<T>`、`Vec<T>` | 无 | 依赖 T | 依赖 T |
| struct | 显式 derive 且所有字段支持 | 同左 | 同左 |

能力查询也覆盖内部类型：never 的四项能力均记为 true，unknown 均记为 false。checkDerives 检查 Copy 必须同时显式 derive Clone、Eq 必须同时显式 derive PartialEq；同名用户方法不等于能力实现。

递归能力按最大不动点计算：先标出结构上可能成立的节点，再把任何失败依赖向父节点传播，直到不再变化。合法递归 `Node { children: Vec<Node> }` 的 Clone 可以成立；环上附加一个不支持 Clone 的字段时，失败必须传遍整个依赖环。不能遇到回边就永久缓存 true。

布局环、常量环、能力递归有不同含义：布局环拒绝，常量环拒绝，能力递归本身允许。三者不要复用一个含义模糊的 visited 状态。

## 8. body.cpp：表达式、作用域与控制流

### 8.1 函数环境和分派

functionBody 从 FunctionSyntax 和 FunctionInfo.signature 设置 current_function、self_type、return_type；保留 scopes[0]，建立参数层，按签名顺序 bind receiver/参数并记录 LocalId。函数体作为 block，再建立自己的块作用域。完成后移除局部层，下一函数不继承局部名字。

expression 按第 4 节统一保存结果；expressionImpl 只负责分派和简单节点。value 构造干净的 Value 结果，不能靠复制任意子节点来生成 block、if、borrow 或 call 的结果。

statement 对 let 先解析可选注解，再 consume initializer，最后 bind 和写 bindings。带分号表达式语句将用途标为 Discard；无分号的非尾表达式语句要求 unit。tail 由 AST 明确提供，不重新从最后一个语句猜测。

### 8.2 expected type 的传播边界

| 位置 | 是否传入目标类型 |
| --- | --- |
| 有注解 let、普通赋值 RHS、实参、return、函数体结果、结构体字段 | 使用已知目标 |
| block tail、if 结果分支、loop break 值、数组字面量元素 | 继续传递外层允许的目标 |
| if / while 条件、下标 | 分别以 bool、usize 检查 |
| 一元/二元运算的操作数、borrow 的操作数、cast 的操作数 | 独立定型，不透传外层目标 |
| 复合赋值 RHS | 不按 LHS 传播整数期望类型 |
| 无固定目标的分支、数组元素和 break 值 | 各自定型后交给 merge |

例如 `let n: u32 = 1;` 可由注解给字面量定型，`let n: u32 = 1i32;` 不接受隐式整数转换；`let n: u32 = 1 + 2;` 不把外层 u32 透传到加法操作数。

### 8.3 Flow 不等于类型

Flow.normal 表示能正常完成；returns、breaks、continues 记录可达出口。sequence 只在前一项能正常完成时并入后一项出口；alternatives 合并分支可能性。**即使不并入出口，后面的 AST 仍要检查。**

```rust
fn f() -> i32 { return 1; false }
fn main() {}
```

false 不能因不可达而跳过返回目标检查。单独考虑 `{ return 1; false }` 的结构，其 tail 类型是 bool，normal 为 false；不能把所有发散块都覆盖成 never。反过来，never 类型的表达式不能正常完成。

| 结构 | 类型与控制流处理 |
| --- | --- |
| 有 tail 的 block | 检查 tail 及使用计划；返回新值，不能泄漏 tail 的 Place 身份 |
| 无 tail 的 block | 能正常结束为 unit，否则为 never |
| if 有 else | 检查条件；两分支按目标或 merge 定型；分支 flow 用 alternatives |
| if 无 else | then 以 unit 检查，结果 unit，保留条件为假时的出口 |
| while | 条件 bool、body unit；结果 unit，不因字面量 true 推断永不退出 |
| loop | body unit；只用指向本循环的 break 值确定结果类型；可达出口另由 Flow 决定 |
| return / break / continue | 本身为 never，并保存 jump_target；操作数若先发散，跳转出口不能被伪造 |
| 短路逻辑 | RHS 仍检查，但不是必执行；不能像普通二元运算那样无条件顺序合并 |

loop 的 PendingUse 收集包括不可达 break 值，以检查类型一致性；它们不会因此成为可达出口。没有 break 或 break 值全为 never 时，loop 结果为 never。裸 break 参加 unit 合并但不生成额外节点。

while 条件检查前令 loop_barrier 等于现有循环栈深度；该条件不能跳向外层循环或当前 while，但可在条件内部创建并跳向新的 loop。检查完条件后恢复 barrier，再压入 while 的 body 上下文。

递归检查 break operand 可能创建嵌套循环，使 loops 扩容；保存下标和目标 ID，递归返回后重新索引，不能持有 loops.back() 的引用跨递归调用。

### 8.4 聚合

array 按源码顺序检查元素，再 merge 并驻留数组类型；已知数组目标可向元素传递类型，长度仍通过最终数组类型匹配。没有目标的空数组定型为 `[(); 0]`。repeat 用 constExpr 检查次数，元素表达式只对应一次求值；次数大于 1 要求元素 Copy。

structure 从类型名找到 StructInfo，逐项检查未知、重复、缺失字段；每个 initializer 使用对应声明字段类型，field_order 保留源码顺序到字段下标的映射。不要为查重先排序字段初始化器，丢失副作用顺序。

## 9. places.cpp 与 conversions.cpp：地址、权限和使用方式

### 9.1 Place 保留什么

| 字段 | 含义 |
| --- | --- |
| root + temporary | 普通根是 LocalId；temporary 为 true 时根是被物化表达式的 NodeId |
| writable | 当前投影位置能否写入 |
| shared_barrier | 路径是否已经穿过共享引用，后续不能恢复可变访问 |
| path | Dereference、DereferenceBox、Field、ArrayIndex、VecIndex 的有序路径；字段携带声明下标 |
| vec_gates | 每次 Vec 索引时该 Vec 是否允许可变访问及对应索引节点 |

不可变绑定和共享屏障不同：`let p = &mut x; *p = 1;` 中 p 不可重绑，但引用目标可写。owned 数组、结构体或 Box 中的 `&mut T` 也可以恢复目标写权限；穿过 `&T` 后则不能恢复。

VecGate 是另一项独立约束：

```rust
fn bad(v: Vec<&mut i32>) { *v[0] = 1; }
fn good(mut v: Vec<&mut i32>) { *v[0] = 1; }
fn main() {}
```

bad 应被拒绝。即使解引用元素里的 &mut 后 writable 变为 true，索引时不可变 Vec 留下的 gate 仍阻止写入。后续字段、数组、Box 和引用投影都要保留 gate。共享读取不要求通过可变 gate；可变借用、重借用和 &mut self 需要检查。

requireMutable 集中检查 Category::Place、writable、shared_barrier 和全部 vec_gates。field/index 对 base 自动解引用到结构体或数组/Vec，必要时物化，构造新结果；显式一元 `*` 只解引用一层。

### 9.2 materialize 的边界

value 需要地址时才补临时存储：借用 value、投影 value 的字段/下标、借用方法 receiver。新临时位置具有可写性，因此临时数组元素赋值、临时 Vec.push 和 `&mut 3` 可以走合法位置路径。

普通赋值不能用 materialize 把任意左值“修成位置”：`1 = 2` 必须拒绝。block 是 Value，`({x}) = 2` 也不能因 tail 为局部路径而通过。

### 9.3 coercion / use / consume / merge 的分工

| 函数 | 输入与工作 | 是否写语义结果 |
| --- | --- | --- |
| coercion | source TypeId、target TypeId → 可选调整链；只判断类型层的转换 | 否 |
| use | ExprInfo、节点 ID、可选 target；应用转换、检查权限、决定 Copy/Move/借用 | 补该表达式 use；解引用过程中可能补 temporary_type |
| consume | expression 后立即 use，返回表中最新副本 | 间接写自身结果和 use |
| merge | 一组已检查 PendingUse 和可选目标；确定公共类型 | 最终统一给各项写 use |

允许的转换范围是同型、never 到目标，以及受支持的引用解引用/Box 解引用后借用或重借用。一般引用 coercion 的源和目标都必须是引用；owned Box 不会仅因目标是引用就自动借用。点方法 receiver 的 autoref 属于另一条专用路径。

不允许隐式整数转换，不递归改变内层引用可变性，不逐元素转换一个已有数组或容器。数组字面量可以逐个检查元素，是因为这些元素本身就是独立使用位置。

merge 在没有 expected 时按源码顺序忽略 never：新类型能转当前目标则保持；否则仅当此前各项都能转到新类型才更新目标。它不搜索第三种类型，也不拿前一元素的类型推断后一字面量。**先确定最终目标，再统一写入 use**，无需在每次候选变化时反复改写旧计划。

## 10. operators.cpp 与 calls.cpp：明确操作与目标

### 10.1 运算检查和 OperatorInfo

| 运算 | 检查重点 | 记录 |
| --- | --- | --- |
| 算术、一元负号、位运算、取反 | 受支持的标量或一层共享标量引用；负号要求有符号整数 | operand_type、左右引用读取标志 |
| 移位 | 两侧都为整数，左右整数类型可不同，结果按左侧 | 左操作数的标量类型和两侧读取方式；右侧类型仍在其 ExprInfo |
| 逻辑与/或 | bool 值及短路流，不自动读引用 | bool 操作类型 |
| 相等比较 | 完全同型且具备 PartialEq，不消耗操作数 | 原操作数类型；子节点 UseMode::BorrowShared |
| 排序比较 | 匹配标量或相同引用层次；仅允许规定的右侧外层 &mut → & 调整 | 最终标量类型、ordering_reference_depth、右侧 use 调整 |
| as | 整数到整数或 bool 到整数；不向操作数传目标 | operation 保存源类型，ExprInfo.type 保存转换结果类型 |
| 普通赋值 | 可变 Place，RHS 按目标类型 consume | target 的 InspectPlace，RHS 的 use；本节点无 operation |
| 复合赋值 | 可变 primitive Place，RHS 独立定型并按对应二元规则检查 | 本节点 operation；目标地址只需计算一次 |

不要用通用 autoderef 放行 `&&i32`、`&mut i32` 或 Box 的标量算术。相等、排序与普通参数 coercion 也不能混成同一条“转到同型”的规则。

`OperatorInfo::operand_type` 不是表达式结果类型：整数 `<` 的结果是 bool，operand_type 是整数；cast 则保存转换前类型。operation 属于运算节点，use 属于父节点对该结果的使用，两者可以同时存在。

静态检查可以先分析赋值 target 来获得目标类型；普通赋值和复合赋值的 Flow 都按运行时的 RHS → target 顺序合并。后续 IR 先计算 RHS，再计算目标地址；复合赋值随后读取旧值、执行运算并写回。测试应覆盖操作数发生控制转移的情况，保证检查次序不影响运行时出口判断。

### 10.2 callable / associated / methods

callable 把已解析的函数名变为 FunctionInfo.signature，或实例化全局 I/O 签名。associated 处理某类型上的源关联函数及内建操作；关联常量继续由普通名称解析处理。methods 针对某一个 receiver 候选返回匹配方法，methodCall 决定候选顺序。

| 内建操作 | 实例化签名 |
| --- | --- |
| get_i32 / print_i32 / println_i32 | `() → i32`、`(i32) → ()`、`(i32) → ()` |
| Box::<T>::new | `(T) → Box<T>` |
| Vec::<T>::new | `() → Vec<T>` |
| Vec len / is_empty | `(&Vec<T>) → usize / bool` |
| Vec push / remove | `(&mut Vec<T>, T) → ()`、`(&mut Vec<T>, usize) → T` |
| 数组 len | `(&[T; N]) → usize` |
| builtin clone | 类型支持 Clone 时 `(&T) → T` |

CallableInfo.function 非零表示源函数 NodeId，此时 builtin 为 None；内建操作 function 为 0，builtin 标明种类，self 和 parameters/result 给出具体类型。function=0 是目标编码，不能据此产生 NodeId=0 的 AST 记录。

### 10.3 普通调用与点方法调用

普通 call 先检查 callee 的 Callable 类别，再检查参数数量；每个实参按签名对应类型 consume，结果保存 call_target。`Type::method(receiver, ...)` 的 receiver 是普通首参，不额外做点调用 autoref。

点调用依次检查 `T, &T, &mut T`，再解引用一层，重复下一组候选。在首个有匹配的候选处停止；该处有多个匹配则报告歧义。**选择之后**才检查权限、实参数量和类型，不因这些检查失败或外层期望返回类型不符而向后试探。

methodCall 给 receiver 表达式写 use，包含已走过的解引用和最后的借用/重借用；其余参数正常 consume。最终调用节点保存 call_target。这样 `b.clone()` 可选择 Box 本身的 Clone，而不会因外层想要元素类型就改选元素 clone。

显式 `S::clone` 的 inherent 优先，与点调用在同一候选处检测 inherent/builtin 竞争，是不同处理。associated 负责关联路径的优先级，methods 负责候选中的匹配和竞争，不能只凭方法名跳到固定 builtin。

## 11. verify、诊断与构建接入

### 11.1 verify 的职责和限制

verify 在成功返回之前调用，发现问题抛内部错误。它读取 AST 结构与结果，检查已经约定的不变量；不重新做名称查找、方法搜索、完整 coercion、能力求解或类型检查。

首先检查以下不变量：

- AST NodeId 唯一且非零；所有 Exprnode（含类型中的长度）有 expressions，所有 Typenode 有 resolved_types，let 有 bindings。
- TypeId 有效，普通结果不为 unknown；Callable 类别与 callable 有值相对应。
- 只有调用节点有 call_target，调用结果类型与目标 result 一致；目标身份及签名中的类型有效。
- operation、resolved_name、field_order、jump_target 的有无与节点角色相符；常量值存在时类型匹配，不要求通用折叠。
- StructExpr 字段映射覆盖全部字段且与名字对应，含空结构体情况。
- temporary_type 与原 Value 的 type 一致；临时 Place 根能找到被物化表达式。
- 跳转目标存在且种类正确；never 不能 normal=true。

在节点覆盖和字段归属检查之上，可逐步增加 bindings→locals、参数顺序、普通 Place 根和源函数签名一致性等直接引用检查。这些检查只核对现有记录之间的关系。verify 通过说明所检查的内部约定成立，语言规则的正确性仍由独立的语义测试验证。

用故意删掉一条表达式记录、破坏字段归属或目标 ID 的测试验证 verifier；与用于检验语言规则的正负例分开。

### 11.2 诊断与前端适配

Diagnostic 保存 node 和 message，可增加稳定错误类别以便定位和测试。fail 标记为 `[[noreturn]]` 并抛出 Diagnostic；driver 分别处理正常拒绝与内部实现故障。

源码定位放在 driver/builder 适配层。SourceBuilder 在建树时保存节点起点，CompleteSourceMap 为缺少独立位置的折叠中间节点补最近祖先位置。这种设计提供节点级诊断定位；需要精确范围时，可在建树时进一步记录 SourceSpan。

driver 在词法/语法成功后建 AST，保存源码映射，并在调用 analyze 前销毁 ANTLR 对象，保证语义检查只依赖独立的 AST 和节点信息。

| 退出码 | 含义 |
| --- | --- |
| 0 | 所请求阶段成功，普通 semantic 检查保持静默 |
| 1 | 正常词法、语法或语义拒绝，诊断走 stderr |
| 2 | 参数、文件或内部实现错误；不得当作语义负例通过 |

CLI 提供 `--stage semantic`、`--dump-ast`、`--dump-semantic`。dump 按节点显示类型、类别、Flow、Place 以及有值的附加信息，禁止输出进程地址作为身份。

### 11.3 构建与 include

Makefile 应纳入 `src/semantic/*.cpp` 和 `include/semantic/*.hpp` 的依赖，使用 C++17。头文件优先使用 `"ast/ast.hpp"`、`"semantic/types.hpp"`、`"semantic/checker.hpp"` 等明确路径，补齐直接使用的标准库头文件。不要依赖 include/ast 的搜索路径偶然找到同名文件，也不要在 semantic 公共头中包含 ANTLR。

测试配置按已完成的阶段接入项目编译器，各阶段分别记录实际执行命令和测试结果。

## 12. 实施顺序：每轮都能解释结果

模块依赖交叉，不能等全部文件写完才验证，也不能为了连通而让未实现分支返回假的成功类型。未支持功能在开发期应明确暴露为实现未完成；只有实现了相应规则，才参与该组正负例验收。

| 轮次 | 实现内容 | 验收重点 |
| --- | --- | --- |
| M0：建立骨架 | 定义类型设施、结果结构和 Checker 接口，实现 Types::format 和诊断出口；接入构建和阶段 CLI | semantic 文件确实参与编译；失败类别明确；空 main 的最小 block/声明/入口链可运行 |
| M1：完成声明层 | collect、类型/名字查询、常量、字段/签名、布局、能力和 derive | 前向/互递归身份可查；未使用坏声明拒绝；常量/布局/能力三类递归分开 |
| M2：最小函数闭环 | functionBody、value/expression、字面量/路径、let/block/return；基本 use/coercion；普通函数与 I/O 调用 | `println_i32(1)`、前向函数调用；遮蔽绑定、参数 LocalId、call_target 和 use 可查 |
| M3：操作与控制流 | 标量运算、cast、赋值、if/loop/while、Flow、PendingUse/merge | 不可达代码仍检查；类型与可达性分离；跳转目标与裸 break 无零记录 |
| M4：引用和聚合 | 完整 Place、物化、借用/解引用、字段/索引、数组/结构体、引用 coercion | VecGate/共享屏障、原始类型与 target、临时存储、field_order；重跑 M3 的引用合流用例 |
| M5：方法和容器 | associated、methods、点调用、完整 Box/Vec/Clone 内建路径 | 首个候选决定后不回退；receiver 调整完整；嵌套调用和投影不复制 call_target |
| M6：完整交付 | 完善 verify、dump 和结构测试；全量官方 semantic 与后续合法源码检查 | 返回结果完整且无悬空身份；连续 analyze 隔离；配置确实运行项目编译器 |

M0 的空 main 闭环只需实现支持范围内的真实逻辑；M1 之后 use、数组重复、比较和 clone 才能依赖完整能力查询。verify 从最早可运行子集开始接入，每新增节点同步增加对应不变量，不留到最后一次性补救。

第一轮实现任务：

1. 在 checker.hpp 中定义共享状态和内部接口：result 保存结果，scopes 管理值名字，typeSegment 解析类型路径段；保持声明与定义一致。
2. 实现 Types 的驻留、结构体名字保存和 format，明确 analyze、fail、verify 的归属与调用关系。
3. 让 Makefile 编译全部 semantic 源文件，实现空 main 所需路径并接入诊断出口。
4. 对 collect → declarations → main 检查逐项验证；使用结构体前向字段、关联常量长度、同名遮蔽和重名声明解释各张表。
5. 再沿 M1–M6 扩展；每次新增一个结果字段的生产路径，同时检查它的归属和后续使用者。

## 13. 测试与验收

### 13.1 三层 semantic 验证

| 层次 | 内容 | 不能替代什么 |
| --- | --- | --- |
| AST 回归 | NodeId、必需字段、语法归一化、脱离 parser 的生命周期 | 不能证明 semantic 正确 |
| 黑盒语义测试 | 接受 0、拒绝 1；崩溃、退出 2、超时都失败 | 不能证明结果足以供 IR 使用 |
| 结果结构与 verify 测试 | 绑定、TypeId、use、调用、投影、常量、字段顺序、跳转和覆盖 | 不能代替后续 IR 的真实运行效果测试 |

结构测试链接 rx::semantic，通过 AST 的名字和结构定位节点，不硬编码某次运行分配出的数字 ID。每个断言检查语义含义，例如绑定身份、转换目标或投影权限，而不依赖容器遍历顺序。

黑盒用例放在 `tests/custom/semantic/`，结果结构检查使用独立的 API 测试目标；`scripts/test_semantic.py` 负责批量语义扫描。

### 13.2 优先保留的结果断言

- 同块遮蔽：initializer 指向旧 LocalId，bindings 指向新 LocalId，退出块和切换函数恢复正确环境。
- 常量：声明结果与 initializer/引用/类型长度表达式各有记录；局部遮蔽不回退；负号和 magnitude 分别保存。
- 使用：源 type 与 use->target 分离；&mut 重借用和无注解 Move 区分；merge 最终目标一致；consume 返回新计划。
- 归属：嵌套运算/调用各有 operation / call_target；block、借用和投影不误带子节点信息。
- 聚合：空结构体有值的空 field_order；乱序初始化映射正确；数组长度表达式覆盖完整。
- 位置：materialize 保留原 Value/type；临时根可查；共享屏障和 VecGate 经过后续投影不丢失。
- 控制流：不可达 tail 的类型仍检查，break 值仍参与定型；目标属于正确循环/函数；没有 expressions[0]。
- 方法：首个候选选中后不因权限、参数或目标返回类型回退；Box/Vec clone 不误选成元素 clone。
- 能力：合法递归通过，坏字段失败沿依赖环传播；inherent clone 不冒充 Clone 能力。
- 会话与内部错误：连续 analyze 不串状态；损坏结果被 verify 拒绝；内部错误不以退出 1 冒充正常拒绝。

### 13.3 官方测试基线与分组

当前 `tests/official` 提交为 `c1e81964c8f7813d4b4de68cc0cbc4332dca57c7`。本地统计：semantic 45 组、236 例，其中 69 接受、167 拒绝；codegen 60 例、optimization 13 例，后两类源程序在 semantic-only 扫描中均预期接受。数量是版本快照，更新子模块后重新统计。

| 工作主题 | 主要官方组 |
| --- | --- |
| 声明、名字、入口 | entry、namespace-errors、names-and-shadowing、protected-names、invalid-impls-and-generics、lifetimes-and-use |
| 常量和布局 | constant-errors、constants-and-paths、recursive-layout |
| 运算 | integer-arithmetic、casts-and-literals、boolean-and-short-circuit、shifts、compound-assignment、scalar-reference-operators |
| 定型与控制流 | expected-types、reference-coercions、reference-lub、blocks-if-and-never、loops-and-jumps、unreachable-checks、loop-state-merges、runtime-value-merges |
| 聚合和位置 | arrays、structs-and-fields、references-and-mutability、aggregate-arguments-and-reference-fields、evaluation-order-and-temporaries |
| 调用 | builtin-io、calls-recursion-and-abi、methods-and-self、source-main-calls、live-state-calls |
| 容器和能力 | box-and-moves、vec-operations、vec-index-mutability、nested-containers、copy-clone-and-equality、trait-dispatch-and-reference-equality、recursive-traits |
| 综合 | comprehensive-large-frame、comprehensive-owned-tree、comprehensive-quicksort、comprehensive-shortest-path、comprehensive-stack-machine |

自定义用例补充官方测试难以直接观察的结果契约，尤其是父子节点信息归属、使用计划、临时位置和内部完整性。测试预期应由具体语义规则和数据约定确定。

### 13.4 实际运行入口

在项目根目录运行现有 AST 回归：

```sh
python3 scripts/test_ast.py
```

构建和 `--stage semantic` 接入后，显式指定项目编译器：

```sh
make test STAGE=semantic FILTER=official:semantic \
  BUILD='make build' \
  SEMANTIC='./.antlr/rx-compiler --stage semantic {source}'

make test STAGE=semantic FILTER=official:semantic:vec-index-mutability \
  BUILD='make build' \
  SEMANTIC='./.antlr/rx-compiler --stage semantic {source}'
```

现有 runner 的 STAGE=semantic 不会自动把 codegen/optimization 的全部源程序当作 semantic 输入，需另加扫描。扫描脚本读取三个阶段的 manifest，对每个源文件运行项目编译器的 `--stage semantic`，检查精确退出码、成功时静默、拒绝时有诊断，并把超时和内部错误计为失败。

最终把已验证的 BUILD/SEMANTIC 配置固化；IR/CODEGEN 的切换等对应阶段实现后再做。

## 14. 交给 IR 的内容与本阶段完成标准

IR 接收 AST 与 SemanticResult：AST 决定语法结构、子表达式和源码顺序，语义结果决定身份、类型、位置路径、目标与调整。IR 不重新按字符串查方法、猜字段号或推断隐式转换。

| IR 需要回答的问题 | 从哪里读取 |
| --- | --- |
| 某个名字对应哪个存储或定义 | expressions[id].resolved_name；locals / constants / functions |
| 某个值的原类型、实际使用类型与方式 | expressions[id].type 和可选 use |
| 如何找到字段、数组/Vec 元素或引用目标 | ExprInfo.place、Projection、原 AST 索引表达式 |
| 需要哪个临时存储 | 被物化表达式的 temporary_type；Place.root |
| 调用哪个函数或 builtin、用什么签名 | 调用节点 call_target；receiver/参数各自的 use |
| 比较/运算时如何读取操作数 | operation 和子表达式 use；具体操作符仍来自 AST |
| 按什么顺序初始化哪个字段 | AST 初始化顺序和 field_order |
| return / break / continue 跳往哪里 | jump_target |
| 是否支持 Copy/Clone/PartialEq/Eq | capabilities，结合 types 和 structs 的结构信息 |

后续 lowering 可区分 emitPlace、emitValue、emitUse：前者计算位置，第二个产生值，第三个应用本表达式已记录的使用计划。缺记录是内部契约错误，不能默认 i32 或字段 0。无 use 的 Callable、投影 base、受限常量按各自角色处理。

运行时需保持单次求值、短路和源码顺序：结构体初始化按写出顺序，数组重复的值只计算一次，receiver/base 不重复执行。引用运算中的目标读取由 operation 指示在外层运算处完成。静态检查的函数调用次序不代替这些执行规则。

字段偏移、大小对齐、LLVM 类型、ABI、分配运行时和递归 clone/equality helper 留给 IR。Copy/Move 记录是值使用语义，不是所有权数据流证明；按值传递仍需保持正确的独立值行为。SemanticResult 是语义旁表，不替代后续要求的 LLVM IR。

Semantic 阶段完成时应满足：

- [ ] 全部 semantic 源文件参与项目构建；CLI 和测试明确调用项目编译器。
- [ ] 类型、名字、全部声明与函数体均检查，未使用和不可达部分不被遗漏。
- [ ] 统一 expressions 覆盖全部 Exprnode，包括类型中的长度；没有 NodeId=0 的伪记录。
- [ ] use、operation、callable、call_target 及其余 optional 字段遵守第 4 节归属；声明、局部、绑定和类型节点保留各自结果表。
- [ ] 原类型、使用目标和临时存储分开；投影保留权限但不复制来源专属信息。
- [ ] 常量依赖、内嵌布局和递归能力分别处理；函数体新增类型的能力已补全。
- [ ] 成功返回前调用 verify；错误分为正常拒绝与内部故障，连续分析互不污染。
- [ ] 官方 semantic 和后续合法源码的 semantic-only 扫描通过，并有针对关键结果的结构断言。
- [ ] 能用具体 Rx 程序解释各字段由谁生成、何时写入、谁来使用；关键语义规则和设计约定均有独立测试。

学习和实现的最小推进单位是“一条规则 → 一个检查函数 → 一份可观察结果 → 一组有区分力的用例”。保持这个闭环，再逐步完成其余模块。
