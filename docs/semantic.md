# Semantic 阶段实现说明

Semantic 模块使用 C++17，位于 `include/semantic/` 和 `src/semantic/`，命名空间为 `rx::semantic`。它接收只读 AST，由一个 Checker 完成名称解析和语义检查，返回独立拥有数据的 SemanticResult。

本文按当前实现说明检查顺序、三个 ASTvisitor 基类、递归返回值及结果表的写入协议，再按文件说明具体规则。AST 的节点与分派机制见 [AST 阶段实现说明](ast.md)；后续 IR 的接口设想集中在 [第 14 节](#ir-plan)。

## 1. 输入契约与阶段边界

| 输入或设施 | 当前约定 |
| --- | --- |
| [AST 节点](../include/ast/ast.hpp) | NodeId 非零且在本树内唯一，kind 与实际类型一致，必需子节点存在 |
| [ASTbuilder](../src/ast/builder.cpp) | 已完成语法归一化，保留源码顺序、尾表达式、整数拼写、receiver 和 has_lifetime |
| [ASTvisitor](../include/ast/dispatch.hpp) | 按表达式、语句、类型等类别分派；返回类型和参数由 Checker 选择 |
| Types | 驻留类型并分配 TypeId；语法类型由 resolveType 解析 |
| SemanticResult | 保存声明、类型、局部绑定、表达式结果及能力信息 |
| Checker | 保存本次检查的名字环境、当前函数、常量依赖和循环上下文 |

AST 的 shared_ptr 管理所有权，语义上仍要求树结构；同一个节点不能挂到两个使用位置。这个约定使每个表达式只有一个直接父节点，也使 expressions[id].use 可以表达唯一的使用计划。

结果对象不保存 AST、ANTLR 或 Checker 内部容器的指针。参数、字段、路径段等辅助结构没有独立 NodeId，通过所属节点及下标识别。当前 AST 也没有源码范围或 NodeId 到源码位置的映射；Diagnostic 只保存节点身份和消息。

本阶段检查名字、类型、受限常量、位置权限、能力、调用选择和使用计划。Copy/Move 是值使用方式，不表示已经进行借用、生命周期或 use-after-move 数据流证明；一般类型泛型、用户 trait 系统和通用常量折叠也不在当前实现范围内。

## 2. Checker 的组织与 Visitor 接口

### 2.1 对外入口和会话

[semantic.hpp](../include/semantic/semantic.hpp) 提供：

```cpp
namespace rx::semantic {
SemanticResult analyze(const AST::Crate& crate);
}
```

[checker.cpp](../src/semantic/checker.cpp) 中的 analyze 使用 `Checker{}.run(crate)`，每次建立独立会话。也可以像当前 driver 一样构造 Checker，再调用一次 run。run 最后移出 result，不是清空状态后可重复使用的接口；分析另一个 AST 时应新建 Checker。

语义错误经 fail 抛出 Diagnostic，并在首个错误处终止。成功才交付完整结果，失败会话中的部分记录不作为分析结果使用。

### 2.2 文件职责

所有实现文件定义同一个 Checker 的成员函数，共享 result 与检查上下文。节点处理函数是 visit 重载，辅助算法仍按各自职责命名。

| 文件 | 职责 |
| --- | --- |
| [types.hpp](../include/semantic/types.hpp)、[types.cpp](../src/semantic/types.cpp) | NodeId / TypeId / LocalId、类型结构、驻留与构造 |
| [semantic.hpp](../include/semantic/semantic.hpp) | 对外结果结构、analyze 和 Diagnostic |
| [checker.hpp](../include/semantic/checker.hpp) | 三个 Visitor 基类、共享状态、visit 与辅助函数声明 |
| [checker.cpp](../src/semantic/checker.cpp) | analyze、run、fail、入口检查、最终能力补全 |
| [declaration.cpp](../src/semantic/declaration.cpp) | 声明登记、resolveSegment、resolveType、四种类型的 visit、名字查询、绑定、签名与布局检查 |
| [constants.cpp](../src/semantic/constants.cpp) | integerLiteral、evaluateConstant、constExpr |
| [capabilities.cpp](../src/semantic/capabilities.cpp) | supportsDeclared、capable、checkDerives |
| [body.cpp](../src/semantic/body.cpp) | 函数环境、expression / statement 入口、字面量与路径、三种语句、聚合和控制流的 visit；Flow 组合 |
| [places.cpp](../src/semantic/places.cpp) | materialize、dereference、requireMutable，以及字段和下标的 visit |
| [conversions.cpp](../src/semantic/conversions.cpp) | coercion、use、consume、merge |
| [operators.cpp](../src/semantic/operators.cpp) | 一元、二元、赋值、cast 的 visit 和 checkBinaryOperands |
| [calls.cpp](../src/semantic/calls.cpp) | callable、associated、methods，以及普通调用和点调用的 visit |

<a id="checker-visitors"></a>

### 2.3 三个 Visitor 基类

Checker 的基类及别名如下，其余成员见头文件：

```cpp
class Checker : private ASTvisitor<Checker, ExprInfo, TypeId>,
                private ASTvisitor<Checker, Flow>,
                private ASTvisitor<Checker, TypeId, TypeId> {
    using ExpressionVisitor = ASTvisitor<Checker, ExprInfo, TypeId>;
    using StatementVisitor = ASTvisitor<Checker, Flow>;
    using TypeVisitor = ASTvisitor<Checker, TypeId, TypeId>;
    friend ExpressionVisitor;
    friend StatementVisitor;
    friend TypeVisitor;
    // 各类别的 visit 和本次分析状态。
};
```

| 基类别名 | 使用的分类入口 | visit 返回值 | 本次调用参数 | 进入分派的 Checker 函数 |
| --- | --- | --- | --- | --- |
| ExpressionVisitor | Exprnode，22 种表达式 | ExprInfo | TypeId expected | expression |
| StatementVisitor | Stmtnode，3 种语句 | Flow | 无 | statement |
| TypeVisitor | Typenode，4 种类型语法 | TypeId | TypeId self | resolveType |

三个基类只是同一个 Checker 的三套分派接口，没有各自的 result 或作用域。friend 使模板基类可以转换到私有派生类，并调用 Checker 的私有 visit。入口使用 `ExpressionVisitor::dispatch` 等限定名，避免多个基类的同名 dispatch 产生歧义。

每次只实例化实际使用的分类分派，因此表达式处理不需要返回 TypeId，类型处理也不需要返回 ExprInfo。各类 visit 都接收本类别规定的参数；不传播 expected 的表达式在定义中省略参数名，而不是读取或修改共享的“当前期望类型”。

声明收集与受限常量检查仍有各自的扫描或 switch。collect 需要先登记身份再补全声明，constExpr 只允许受限表达式集合，二者不由通用 Walker 决定顺序。

### 2.4 共享状态的生命周期

| Checker 成员 | 建立与使用方式 | 是否进入返回结果 |
| --- | --- | --- |
| result | 各阶段填充，run 最后移出 | 是 |
| type_names | collect 登记基础类型和结构体；resolveSegment 查询 | 否 |
| scopes | 初始只有全局层；函数、块建立局部层；bind 更新最内层 | 否 |
| functions / constants / structures | collect / addMember 保存 AST 指针及声明环境，供后续检查使用 | 否 |
| ConstSyntax::state | 0 未访问、1 求值中、2 完成；用于常量依赖检查 | 否 |
| current_function、self_type、return_type | functionBody 为当前函数设置 | 仅必要身份写入 Local / ExprInfo |
| next_local | 从 1 开始；每次 bind 递增，切换函数不复位 | 分配结果成为 LocalId |
| loops、loop_barrier | 循环检查时压入、弹出和恢复；限制跳转并收集 break 值 | 目标和 Flow 写入对应结果 |

Checker::functions 保存检查期间所需的语法及 Self；result.functions 保存签名与参数绑定。前者在检查结束后失效，后者随结果保留。constants 的语法表与常量值表也有相同分工。

## 3. 类型身份与结果表

### 3.1 Types 和三种 ID

NodeId、TypeId、LocalId 底层均为 size_t，但含义不同：函数、结构体、常量和循环使用 AST 节点的 NodeId；局部变量另分配 LocalId；字段编号是所属 StructInfo.fields 的下标。

| TypeKind | 类型身份包含的信息 |
| --- | --- |
| Unit、Never、Bool、I32、U32、ISize、USize | 固定 TypeId；四种整数彼此不同，位宽均为 32 位 |
| Struct | 声明 NodeId；字段相同的两个结构体仍是不同类型 |
| Reference | element TypeId、mutable_reference；生命周期不参与驻留 |
| Array | element TypeId、求值后的 uint32_t length |
| Box、Vec | element TypeId |
| Unknown | 内部未指定标记，不是普通运行时值类型 |

固定编号为 unknown=0、unit=1、never=2、boolean=3、i32=4、u32=5、isize=6、usize=7。Types::intern 以 `(kind, element, length, declaration, mutable_reference)` 为键去重。结构体先驻留身份，字段和名字保存在 StructInfo；创建 `Vec<Node>` 不展开 Node 的字段。

成功结果中，普通表达式、局部变量、签名参数/返回类型和已解析类型节点应有确定类型。Callable 没有对应的一等函数类型，其 ExprInfo.type 可以是 unknown；顶层函数 CallableInfo.self 也使用 unknown 表示没有所属类型。

### 3.2 SemanticResult 的结果表

| 字段 | 键 / 内容 | 生产者 | 主要读取位置或含义 |
| --- | --- | --- | --- |
| types | TypeId → Type | Types 构造和各模块的类型请求 | 全部类型相关检查 |
| entry | 顶层 main 的 NodeId | run | 程序入口身份 |
| structs | 声明 NodeId → StructInfo | collect、addMember、declarations | 字段、方法、布局和能力检查 |
| functions | 声明 NodeId → FunctionInfo | declarations 写 signature；functionBody 补 parameters | 调用和函数体检查 |
| constants | ConstItem NodeId → Constant | evaluateConstant | 常量引用与数组长度 |
| locals | LocalId → Local | bind | 局部路径及位置权限 |
| bindings | LetStmt NodeId → LocalId | LetStmt 的 visit | let 语法与新绑定的对应关系 |
| resolved_types | Typenode NodeId → TypeId | resolveType | 语法类型的解析结果与缓存 |
| expressions | Exprnode NodeId → ExprInfo | expression、constExpr、负字面量专用路径；父节点补 use / temporary_type | 本节点结果和直接父节点的使用计划 |
| capabilities | 四项能力各自的 TypeId → bool | checkDerives 成功后由 capable 查询并缓存；run 最后补全 | Copy/Move、重复数组、clone、相等比较 |

参数和 receiver 没有 LetStmt。它们的 LocalId 按签名顺序放入 FunctionInfo.parameters，receiver 在最前；Local.declaration 使用函数 NodeId，不写入 bindings。

field_indices 是 fields 的名字索引；derives 保存源码显式请求，只有 checkDerives 全部通过后，capable 才把相应结论写入 capabilities。结果表不保存验证期间的临时假设。

<a id="result-protocol"></a>

## 4. 返回值、递归与语义结果的写入协议

### 4.1 ExprInfo 各字段的归属

| 字段 | 含义与有效范围 | 主要写入位置 |
| --- | --- | --- |
| type、category、flow | 本节点的类型、Value/Place/Callable 类别和出口 | 对应 visit 或 constExpr |
| place | 仅 Place 类别有效；根、投影及访问权限 | 局部路径、dereference、字段和下标 visit |
| callable | 表达式本身表示的可调用对象 | 函数或 builtin 的 PathExpr |
| call_target | 这次调用选中的目标及签名 | CallExpr / MethodCallExpr 的 visit |
| use | 直接父节点使用该表达式的目标类型、方式和调整链 | use，以及借用、比较、方法 receiver 的专用路径 |
| operation | 运算所用类型、引用读取标志、排序引用层数 | operators.cpp；constExpr 的受限负号 |
| resolved_name | PathExpr 解析出的局部、常量、函数或 builtin 身份 | PathExpr 的 visit 或 constExpr |
| constant_value | 实际求出的常量类型和位模式 | 整数字面量、常量引用、受限常量及负整数字面量 |
| field_order | 源码初始化顺序 → 声明字段下标 | StructExpr 的 visit |
| jump_target | return 对应函数，break/continue 对应循环 | 对应跳转表达式的 visit |
| temporary_type | 父节点要求物化本值时的原始存储类型 | materialize |

附加信息使用 optional。空结构体的 field_order 是有值的空 vector；其他节点没有 field_order 时为 nullopt。`f(1)` 中，路径 f 的记录有 callable，整个调用的记录有 call_target，两份信息属于不同节点。

### 4.2 expression 保存本节点，consume 补使用计划

普通递归沿以下入口运行：

```text
父节点 consume(child, expected)
  expression(child, expected)
    ExpressionVisitor::dispatch(child, expected)
      visit(具体表达式, expected)
        通过语义入口检查各个子节点
        返回本节点自己的 ExprInfo
    清除本节点 use，保存 expressions[child.id]
  use(info, child.id, expected)
    检查转换、位置权限和 Copy 能力
    补 expressions.at(child.id).use
  返回表中已补全的记录副本
```

对应实现保持很短：

```cpp
ExprInfo Checker::expression(const Exprnode& node, TypeId expected) {
    auto res = ExpressionVisitor::dispatch(node, expected);
    res.use.reset();
    result.expressions[node.id] = res;
    return res;
}

ExprInfo Checker::consume(const Exprnode& node, TypeId expected) {
    auto info = expression(node, expected);
    use(info, node.id, expected);
    return result.expressions.at(node.id);
}
```

visit 返回的 ExprInfo 是这次调用的值，各层递归用局部变量接收。SemanticResult 是所有节点的持久记录，两者职责不同：返回值方便父节点组合结果，表让已确定的信息在分析结束后仍可查询。没有共享的“最后一次访问结果”字段。

父节点可以补子节点的 use 或 temporary_type，但不要再用旧副本覆盖整条记录。consume 因此返回表中的最新副本；只需要观察类型或位置的路径可以先调用 expression，随后按具体规则补计划。

expected 参与允许的局部定型；ExprInfo.type 是该节点检查后的源类型，use.target 是父节点的使用目标。转换不会把前者覆盖成后者。例如：

```rust
fn main() {
    let mut x = 1;
    let p = &mut x;
    let q: &i32 = p;
}
```

最后一个 p 的路径记录仍为 `type = &mut i32`，并关联原 LocalId 和 Place；consume 补 `target = &i32`、BorrowShared 以及 Dereference → BorrowShared 调整。无注解的 `let q = p;` 按源类型选择 Move；有固定目标且目标同为 `&mut i32` 时，则走可变重借用。

### 4.3 语句、类型与特殊表达式入口

statement 只通过 StatementVisitor 返回 Flow，没有独立的语句结果表。LetStmt 的 LocalId 写 bindings，initializer 的 ExprInfo 仍由 consume 保存。

resolveType 先查 resolved_types；未命中时通过 TypeVisitor 求 TypeId，随后登记。RefType 和 ArrayType 的子类型继续调用 resolveType，以保留缓存和各个类型节点的记录。ArrayType.length 调 constExpr，长度表达式的结果另外进入 expressions。

普通表达式递归使用 expression 或 consume，语句使用 statement，类型使用 resolveType。直接对语义子节点调用 visit/dispatch/accept 会绕过相应的登记或使用计划。以下专用路径明确负责自行保存记录：

- constExpr 处理受限常量，包括数组类型长度和重复数组次数；不进入普通表达式 Visitor。
- 负整数字面量同时保存负号节点和 magnitude 子节点。最小有符号值的幅值允许保留对应位模式，不按独立正字面量重复拒绝。
- 普通 BoolLiteral 的 visit 只产生 bool 类型；受限常量路径才保存其 constant_value。
- Callable、仅作投影 base 的节点及受限常量不一定有普通 use，不能把“全部表达式有记录”等同于“全部表达式有 use”。
- 裸 break 的隐含 unit 以 PendingUse 的 node=0 参与合流，不产生 expressions[0]；break 节点本身仍有记录。

### 4.4 新结果与来源节点的字段分开

value 构造新的 Value 结果。解引用、字段和下标访问只继承必要的类型、Flow 和 Place，不复制来源节点的 resolved_name、constant_value、call_target 或 field_order。

```rust
struct Pair { a: i32, b: bool }
fn main() {
    let n = (Pair { b: true, a: 7 }).a;
}
```

构造节点保存 `field_order = {1, 0}`；字段 visit 需要地址时，materialize 在该构造节点补 `temporary_type = Pair`。持久记录仍是 Pair 类型的 Value。外层 `.a` 是 i32 Place，根指向临时表达式，投影含字段 0；它不具有 field_order。

materialize 返回的工作副本可以变为 Place，但不能把原表达式的持久记录整体改成 Place，也不能把临时存储类型写成 use.target。`make().a` 的 call_target 同样只属于 make() 调用节点。

## 5. run 的阶段顺序

```text
analyze → 新 Checker → run
  collect
  declarations
  checkLayouts
  checkDerives
  检查顶层 main，写 entry
  逐个 functionBody
  为已驻留类型补齐四种能力结果
  return move(result)
```

| 阶段 | 完成后可依赖的信息 |
| --- | --- |
| collect | 结构体身份、顶层函数/常量和 impl 成员已登记 |
| declarations | 全部常量值、字段、derive 请求与函数签名已确定 |
| checkLayouts | 全部结构体的内嵌布局无环 |
| checkDerives | 全部显式前置要求和字段能力验证成功，可查询并缓存能力 |
| main 检查 | entry 指向顶层函数，参数为空、结果 unit、没有生命周期形参 |
| functionBody | 所有函数体经过检查，局部、表达式及使用计划已记录 |
| 能力补全 | 从 unit 到最后一个 TypeId 的四项能力均可查询，包含函数体中新驻留的类型 |

入口从 scopes[0] 查询，不受局部遮蔽影响。源 main 仍是普通可调用函数。run 在能力补全后直接返回结果。

阶段内可以递归请求其他模块：resolveType 可能通过数组长度请求 constExpr，constExpr 又请求 evaluateConstant；函数体也会驻留新的引用或数组类型。因此这些模块共享一个会话，而不是互不调用的独立检查器。

## 6. declaration.cpp：从名字登记到完整声明

### 6.1 collect / addMember：先登记身份

按以下三个遍历组织 collect：

1. 注册基础类型、I/O builtin；遍历全部 StructItem，建立名义 TypeId、type_names、structures 和初始 StructInfo。
2. 遍历顶层函数、常量，由 addMember 保存 FunctionSyntax / ConstSyntax，并登记到全局值层。此时不检查函数体、不要求字段和签名已完成。
3. 遍历 ImplItem，解析目标并确认是用户结构体；将所有成员登记到该 StructInfo.members。多个 impl 共用同一个关联值空间。

类型空间、值空间、字段空间、关联值空间分别处理重名。顶层函数和常量共用值空间；字段与方法可以同名。结构体声明的类型名冲突检查使用 type_names 和 protected_names；顶层 I/O 名字通过预先登记到全局值层受到保护。保护名检查必须限定所在名字空间，不能扩大成“所有位置都不能使用这些字符串”。规范列为 UB 的输入不作为必拒绝规则的证明。

addMember 的 self 参数是所属结构体 TypeId；unknown 表示顶层。FunctionSyntax.self 和 ConstSyntax.self 固定记录声明环境，后续解析 `Self` 时使用它。顶层函数带 receiver 应在登记阶段拒绝。

### 6.2 resolveSegment / resolveType：把语法类型变成 TypeId

- resolveSegment 解析一个路径段：Box/Vec 必须恰有一个具体类型参数；其他受支持类型不接受具体类型参数；Self 从参数 self 取得；普通名字查 type_names。
- resolveType 先查缓存，再通过 TypeVisitor 分派到 UnitType、PathType、RefType 或 ArrayType 的 visit。各处理函数递归解析子类型并调用 Types 构造函数，外层 resolveType 最后写 `result.resolved_types[node.id]`。
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

collect 先建立 Buffer 身份，再登记 N 和 LIMIT。declarations 请求常量值并解析字段；字段长度的 Self 使用 Buffer 的 TypeId，解析到成员 N。N/LIMIT 的声明值进入 constants，各自初始化表达式及 `Self::N` 长度路径进入 expressions，整个数组类型节点进入 resolved_types。后续 StructExpr 的 visit 检查字段值，CallExpr 的 visit 检查 take 的参数，无须再查字符串字段类型。

### 6.3 lookupValue / resolveName：查询与记录分开

lookupValue 从 scopes 的最内层向全局层查询单段名字。找到后立即返回；若名字是局部变量，调用失败或常量求值失败都不能回退搜索同名全局函数/常量。

resolveName 处理单段值名，或 `类型::成员` 两段路径。两段路径先经 resolveSegment 确定 owner，再查 StructInfo.members；不把路径拼接成一个全局字符串键。

resolveName **返回 ResolvedName，不直接写 ExprInfo**。调用它的 PathExpr 检查或 constExpr 决定如何使用结果，并在该节点记录 resolved_name。Box/Vec 等内建关联操作由 calls.cpp 的 associated 实例化；普通表达式路径会先尝试 associated，再进入普通名称解析，常量路径只接受常量声明。

### 6.4 bind：可见名字与存储身份分别保存

bind 分配新 LocalId，写 `result.locals[id] = Local{name, type, mut, current_function, declaration}`，再让 `scopes.back()[name]` 指向新 LocalId。它不自行写 bindings，也不追加 FunctionInfo.parameters；这两项分别由 LetStmt 的 visit 和 functionBody 完成。

```rust
fn main() {
    let x = 1;
    let x = x + 1;
}
```

第二个 initializer 先在旧环境中检查，内部 PathExpr 指向第一个 LocalId；之后才 bind 第二个 x。当前作用域覆盖名字映射，但 locals 中两条记录都保留，各个路径表达式仍能关联到原来的绑定身份。退出块只弹作用域，不删除已产生的语义结果。

### 6.5 declarations / checkLayouts：补全结构与签名

declarations 主动求所有常量，再完成结构体字段及 derive 请求，最后解析所有函数签名。必须检查未使用声明。数组长度查询仍可按需递归求常量，不能另加“类型解析期间禁止常量查询”的限制。

字段按声明顺序 push 到 fields，field_indices 记录名字对应下标并发现重复。derives 只记录显式请求并检查重复，包括同组和跨属性的重复；此处不验证字段能力，也不自动补入其他 derive。全部结构体的字段和请求登记完成后，由 checkDerives 逐个验证。

| receiver 写法 | signature.parameters 首项 | functionBody 建立的 self 绑定是否可重绑 |
| --- | --- | --- |
| 无 receiver | 不添加首项 | 不建立 self |
| `self` | Self | 否 |
| `mut self` | Self | 是 |
| `&self` | `&Self` | 否 |
| `&mut self` | `&mut Self` | 否；解引用后的写权限由 Place 规则决定 |

其余参数解析为 TypeId 并检查名字重复；省略返回类型等价于 unit。declarations 此时只写 signature，FunctionInfo.parameters 留空，等 functionBody 分配 LocalId。源函数 body 缺失属于 AST 契约错误。

checkLayouts 对结构体定义做 DFS：沿 fields 和数组 element 递归，遇到引用、Box、Vec 停止。未访问/访问中/已完成三态足够发现内嵌环；状态容器和整数编码是实现选择。`struct Bad { next: Bad }` 拒绝，`struct Node { next: Box<Node> }` 布局可接受。即使某个坏结构体只被 Box 引用，它自己的定义仍需单独检查。

## 7. constants.cpp 与 capabilities.cpp：常量求值与派生能力验证

### 7.1 字面量和受限常量

integerLiteral 根据后缀、允许的 expected type 或默认 i32 定型，去后缀与下划线，按进制读取 magnitude，先检查范围再形成 uint32_t 位模式。有符号最小值通过负字面量专用路径处理，不能先把 magnitude 放进宿主 int32_t。宿主 size_t 的位宽不决定 Rx usize 的位宽。

constExpr 只接受当前受限范围：整数、布尔、常量路径，以及作用于整数字面量或常量路径的受限负号。括号已由 AST 消去；不增加一般算术、调用、局部值或任意表达式求值。已有常量的类型由其声明固定，用作数组长度时必须是 usize，不能把 i32 常量重新推断成 usize。

evaluateConstant 的工作顺序：

1. 已完成则读取 result.constants；访问中则报告依赖环。
2. **在解析声明类型之前**标记访问中，以捕获经数组长度回到自身的依赖。
3. 用 ConstSyntax.self 解析声明类型，再以该类型检查 initializer。
4. 写 result.constants，标记完成；后续引用读取缓存。

declarations 在任何函数体之前主动求值全部常量，此时 scopes 只有全局层；evaluateConstant 自身不切换作用域。函数体里的长度表达式仍从当前局部环境查名：

```rust
const N: usize = 2;
const M: usize = N;
fn main() {
    let N = 3usize;
    let a = [0; M];
}
```

M 的定义使用全局 N，因此长度为 2。如果把最后一行改成 `[0; N]`，该长度路径在当前局部环境中解析到 local，受限常量检查应拒绝，不能忽略遮蔽寻找全局 N。

当前分析在首个错误处终止，失败后不继续使用这次 Checker 的部分结果。

### 7.2 derive：先登记声明，再逐个验证

declarations 先登记全部结构体的字段和 derive 请求，checkDerives 再逐个验证。查询某个结构体的能力时，只读取它是否显式请求了对应 derive，不展开它的字段；每个结构体的字段要求由自己的 checkDerives 检查负责。

capabilities.cpp 定义辅助函数 supportsDeclared 统一以下类型规则。它只读取类型和已登记的请求，不写 result.capabilities；checkDerives 和验证成功后的 capable 共用这份规则。

| 类型 | Copy | Clone | PartialEq / Eq |
| --- | --- | --- | --- |
| 整数、bool、unit | 有 | 有 | 有 |
| `&T` | 有 | 有，不依赖 T 的 Clone | 依赖 T 的对应能力 |
| `&mut T` | 无 | 无 | 依赖 T 的对应能力 |
| `[T; N]` | 依赖 T | 依赖 T | 依赖 T |
| `Box<T>`、`Vec<T>` | 无 | 依赖 T | 依赖 T |
| struct | 读取对应 derives 项，不展开字段 | 同左 | 同左 |

查询也覆盖内部类型：never 的四项能力均为 true，unknown 均为 false。同名用户方法不等于能力实现。结构体是查询的终点；引用、数组和容器按表中规则查询元素类型，因此查询不会沿结构体字段形成递归环。

checkDerives 遍历全部结构体，包括未使用的声明，对每个结构体执行：

1. 检查 Copy 必须同时显式 derive Clone、Eq 必须同时显式 derive PartialEq，不能自动补齐。
2. 对每项已请求的能力，逐个字段调用 supportsDeclared；任一字段不满足就报告该结构体、字段和能力并终止分析。
3. 检查完当前结构体后继续检查其他结构体；只有全部请求通过，才允许进入函数体检查并使用 capable。

核心流程如下，结构体自身的 derive 位不能代替字段验证：

```text
for 每个结构体 S:
    检查 S 的显式前置要求
    for S 请求的每项能力 C:
        for S 的每个字段 F:
            若 supportsDeclared(F.type, C) 为 false，则报错并终止
```

例如，以下互递归声明可以通过 Clone 验证：

```rust
#[derive(Clone)]
struct A { children: Vec<B> }

#[derive(Clone)]
struct B { children: Vec<A> }
```

检查 A 的字段时，`Vec<B>` 要求 B 声明 Clone，查询在 B 处结束；检查 B 时同理。自递归的 `#[derive(Clone)] struct Node { children: Vec<Node> }` 也按同一规则通过。若 B 没有声明 Clone，检查 A 时拒绝；若 B 增加 `bad: &mut i32` 字段，检查 B 自己的字段时拒绝，即使 A、B 都未使用也一样。此时整个分析终止，不返回语义结果，因此无需再把失败传播回 A。

capable 只在 checkDerives 全部成功后调用：先查询 result.capabilities，未命中时调用 supportsDeclared 并缓存结果。此时所有结构体请求均已验证，读取 derives 得到的就是有效能力；函数体新增的引用、数组和容器类型也使用同一规则。run 最后为 unknown 之外的全部已驻留类型补齐四项能力结果。

这个方案依赖于全部 derive 都被验证、字段要求必须全部满足且首错终止的约定。常量依赖和内嵌布局仍分别检查并拒绝环；能力查询在结构体处停止，不需要能力环的 visited 状态。

## 8. body.cpp：表达式、作用域与控制流

### 8.1 函数环境和分派

functionBody 从 FunctionSyntax 和 FunctionInfo.signature 设置 current_function、self_type、return_type；保留 scopes[0]，建立参数层，按签名顺序 bind receiver/参数并记录 LocalId。函数体作为 block，再建立自己的块作用域。完成后移除局部层，下一函数不继承局部名字。

expression 通过 `ExpressionVisitor::dispatch` 取得具体 visit 的 ExprInfo，然后保存本节点记录。简单字面量、路径、语句、聚合和控制流的 visit 位于 body.cpp；操作符、投影和调用的 visit 分别位于对应实现文件。value 构造新的 Value 结果，block、if、borrow 和 call 不复制来源节点的专属字段。

statement 通过 `StatementVisitor::dispatch` 返回 Flow。LetStmt 的 visit 按“解析注解 → consume initializer → bind → 写 bindings”处理；ExprStmt 的 visit 对有分号表达式补 Discard，无分号的非尾表达式要求 unit。BlockExpr 的 tail 直接来自 AST，不重新从最后一个语句猜测。

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

数组注解展示了三个 Visitor 的协作：

```rust
fn main() {
    let a: [u32; 2] = [1, 2];
    let b = 3;
}
```

StatementVisitor 进入 LetStmt 的 visit，先由 resolveType / TypeVisitor 得到 `[u32; 2]`。随后 consume 将该 TypeId 传给数组的 ExpressionVisitor；ArrayExpr 的 visit 再把 u32 作为 expected 传给两个元素，并为元素写入使用计划。数组本身的结果及 use 由外层 expression / consume 保存，最后才登记 a。下一条 let 没有注解，新的 expected 为 unknown，因此 b 仍按默认 i32 定型。

### 8.3 Flow 不等于类型

Flow.normal 表示能正常完成；returns、breaks、continues 记录可达出口。sequence 只在前一项能正常完成时并入后一项出口；alternatives 合并分支可能性。**即使不并入出口，后面的 AST 仍要检查。**

```rust
fn f() -> i32 { return 1; false }
fn main() {}
```

这里的 false 仍按函数返回目标 i32 检查并被拒绝。有 tail 的块根据 tail 和 expected 确定类型，同时用 Flow 合并可达出口；无 tail 的块才根据能否正常完成选择 unit 或 never。不能只根据 Flow.normal 推断所有块的类型。

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

ArrayExpr 的 visit 按源码顺序检查元素，再 merge 并驻留数组类型；已知数组目标可向元素传递类型，长度仍通过最终数组类型匹配。没有目标的空数组定型为 `[(); 0]`。ArrayRepeatExpr 的 visit 用 constExpr 检查次数，元素表达式只对应一次求值；次数大于 1 要求元素 Copy。

StructExpr 的 visit 从类型名找到 StructInfo，逐项检查未知、重复、缺失字段；每个 initializer 使用对应声明字段类型，field_order 保留源码顺序到字段下标的映射。不要为查重先排序字段初始化器，丢失副作用顺序。

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

requireMutable 集中检查 Category::Place、writable、shared_barrier 和全部 vec_gates。FieldExpr / IndexExpr 的 visit 对 base 自动解引用到结构体或数组/Vec，必要时物化，构造新结果；显式一元 `*` 只解引用一层。

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

静态检查先分析赋值 target 来获得目标类型；普通赋值和复合赋值的 Flow 则按运行时的 RHS → target 顺序合并。复合赋值在目标求值后读取旧值、执行运算并写回。操作数发生控制转移时，也按这个执行顺序组合出口。

### 10.2 callable / associated / methods

callable 把已解析的函数名变为 FunctionInfo.signature，或实例化全局 I/O 签名。associated 处理某类型上的源关联函数及内建操作；关联常量继续由普通名称解析处理。methods 针对某一个 receiver 候选返回匹配方法，MethodCallExpr 的 visit 决定候选顺序。

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

CallExpr 的 visit 先检查 callee 的 Callable 类别，再检查参数数量；每个实参按签名对应类型 consume，结果保存 call_target。`Type::method(receiver, ...)` 的 receiver 是普通首参，不额外做点调用 autoref。

点调用依次检查 `T, &T, &mut T`，再解引用一层，重复下一组候选。在首个有匹配的候选处停止；该处有多个匹配则报告歧义。**选择之后**才检查权限、实参数量和类型，不因这些检查失败或外层期望返回类型不符而向后试探。

MethodCallExpr 的 visit 给 receiver 表达式写 use，包含已走过的解引用和最后的借用/重借用；其余参数正常 consume。最终调用节点保存 call_target。这样 `b.clone()` 可选择 Box 本身的 Clone，而不会因外层想要元素类型就改选元素 clone。

显式 `S::clone` 的 inherent 优先，与点调用在同一候选处检测 inherent/builtin 竞争，是不同处理。associated 负责关联路径的优先级，methods 负责候选中的匹配和竞争，不能只凭方法名跳到固定 builtin。

## 11. 诊断、driver 与构建

### 11.1 诊断的实际边界

Diagnostic 继承 runtime_error，附带 NodeId；Checker::fail 直接抛出该异常。它没有单独的稳定错误代码。当前 driver 捕获 Diagnostic 后输出 `error: ...`，没有把节点 ID 转成源码行列。

[main.cpp](../src/main.cpp) 读取文件，执行 parser.crate，检查 Lexer/Parser 错误数，建树，再构造 Checker 并调用 run。ANTLR 对象在整个 main 作用域中仍然存在，但语义接口只接收 AST，不保存它们的指针。

| 退出码 | 当前 driver 的含义 |
| --- | --- |
| 0 | 语义分析成功；正常检查不输出结果 |
| 1 | 词法/语法拒绝，或捕获到 Diagnostic |
| 2 | 缺少源文件、无法打开文件，或语义调用中捕获到其他标准异常 |

CLI 识别 `--stage` 并记录阶段值，但实际执行路径统一到语义检查，不据此跳过或继续其他阶段。ASTprinter 的调用仍被注释；没有 `--dump-ast` 和 `--dump-semantic` 功能。若需要检查结果，可在 API 调用处读取 SemanticResult。

### 11.2 构建与头文件

[Makefile](../Makefile) 以 C++17 编译 `src/main.cpp`、`src/ast/*.cpp`、`src/semantic/*.cpp` 和已生成的解析器。头文件依赖包含 `include/ast/*.hpp` 与 `include/semantic/*.hpp`，因此也覆盖 dispatch.hpp。

现有 include 搜索路径同时包含 `include`、`include/ast`、`include/semantic`；源码沿用 `"checker.hpp"`、`"ast.hpp"` 等写法。semantic 公共接口不包含 ANTLR 头文件。

[config.mk](../config.mk) 已配置 BUILD 和 SEMANTIC，指向项目的 `.antlr/rx-compiler --stage semantic`。普通构建复用仓库生成文件和运行库，不需要重新生成 Parser。

## 12. 运行与验证

### 12.1 当前可复现入口

在仓库根目录执行：

```sh
make -j2 build
./.antlr/rx-compiler --stage semantic path/to/program.rx

make test STAGE=semantic FILTER=official:semantic
make test STAGE=semantic FILTER=official:semantic:vec-index-mutability
python3 scripts/test_ast.py
```

已经构建且源码未改时，可在 make test 后加 `BUILD=` 省去构建步骤。结果写入 `target/tests/run-*/`；AST 回归报告写入 `target/ast-tests/`。

[scripts/test.py](../scripts/test.py) 根据 manifest 的 compilation_success 要求接受退出 0、拒绝退出 1，其他退出状态或超时算失败。AST 回归则检查结构和节点契约，两者覆盖不同层次。

### 12.2 官方输入范围

按当前仓库 manifest 统计，semantic 有 45 组、239 例，其中 71 例接受、168 例拒绝。codegen 的 60 个程序和 optimization 的 13 个程序也可作为合法语义输入，但 `STAGE=semantic` 只筛选本来属于 semantic 的用例，不自动重分类其他阶段。

需要扩大语义输入时，可读取相应 manifest，逐个调用同一语义入口。这里只验证接受、拒绝及语义结果，不据此声称程序运行输出已经通过。数量随用例更新变化，应以本地 manifest 为准。

| 检查主题 | 典型官方分组 |
| --- | --- |
| 声明、名字、入口 | entry、namespace-errors、names-and-shadowing、invalid-impls-and-generics |
| 常量、布局、能力 | constants-and-paths、constant-errors、recursive-layout、recursive-traits |
| 运算与定型 | expected-types、casts-and-literals、scalar-reference-operators、reference-coercions、reference-lub |
| 控制流 | blocks-if-and-never、loops-and-jumps、unreachable-checks |
| 聚合与位置 | arrays、structs-and-fields、references-and-mutability、vec-index-mutability |
| 调用与容器 | methods-and-self、builtin-io、box-and-moves、vec-operations |

### 12.3 结果级检查的维护要点

黑盒退出码不能单独证明结果表完整。扩展或调整规则时，可直接调用 analyze，通过 AST 结构定位节点，检查以下语义含义，避免硬编码某次分配出的 ID：

- 同块遮蔽：initializer 指向旧 LocalId，bindings 指向新 LocalId；参数绑定保存在函数参数表。
- 完整记录：类型长度、常量 initializer、负字面量 magnitude 各有对应记录，无 expressions[0]。
- 使用计划：源 type、use.target、temporary_type 分开；consume 返回补全后的副本，merge 最终统一计划。
- 信息归属：嵌套调用、运算、投影与聚合只保存各自适用的附加字段。
- 位置权限：共享屏障、VecGate 和临时根经过后续投影仍可追溯。
- 控制流：不可达代码仍检查，类型与可达出口分开；return/break/continue 指向正确目标。
- 能力与方法：全部 derive 验证后才缓存结论；首个匹配的点调用候选不会因后续检查失败而回退。
- Visitor：分类入口只要求所属类别的处理函数；递归返回值互不覆盖，expected 按次传递。
- 会话：连续调用 analyze 创建独立 Checker，不复用上一轮作用域、ID 或结果。

## 13. 扩展时保持的边界

新增表达式、语句或类型规则时，先确定它的返回类型、允许传播的上下文，以及应产生的结果字段，再更新具体 visit 和对应的分类分派。节点枚举、accept 和分派的维护要求见 [AST 文档](ast.md#visitor-redesign)。

需要保留的入口职责如下：

| 入口 | 职责 |
| --- | --- |
| expression | 分派并保存当前表达式的基础结果 |
| consume | 在 expression 之后应用普通使用计划 |
| statement | 分派并返回语句 Flow；具体语句登记必要绑定 |
| resolveType | 类型缓存、分派和 resolved_types 登记 |
| constExpr | 按受限常量规则求值并自行保存表达式记录 |

单个 visit 负责本节点规则，不把整套登记协议复制进去。使用 value 构造新结果，父节点补子节点字段时使用表中的目标记录；名称、调用目标、字段下标和跳转目标分别在对应节点确定。

声明收集、常量依赖、布局和能力验证的先后关系也应保留。尤其是未使用声明与不可达表达式仍需检查，全部 derive 验证成功后才能把能力缓存当作有效结论。

<a id="ir-plan"></a>

## 14. 后续 IR 阶段的接口规划

后续 IR lowering 可以同时接收 AST 与 SemanticResult：由 AST 提供语法结构、子表达式和源码顺序，由语义结果提供已经确定的身份、类型、位置、目标及调整计划。规划中应复用这些信息，避免重新按字符串查找方法、推断字段下标或隐式转换。

| 后续生成需要的信息 | 计划读取的位置 |
| --- | --- |
| 名字对应的绑定或定义 | resolved_name，以及 locals / constants / functions |
| 值的源类型、使用目标及方式 | ExprInfo.type 与可选 use |
| 字段、数组/Vec 元素或引用目标的位置 | Place、Projection，以及 AST 中的索引表达式 |
| 需要物化的临时值 | temporary_type 与 Place.root |
| 函数或 builtin 目标与签名 | 调用节点 call_target；receiver / 参数各自的 use |
| 运算类型与引用读取方式 | operation、子表达式 use 和 AST 操作符 |
| 字段初始化的顺序及目标下标 | AST 初始化顺序与 field_order |
| return / break / continue 的目标 | jump_target |
| Copy、Clone、PartialEq、Eq 能力 | 已验证并补全的 capabilities |

Visitor 方面，可以为 lowering 选择专用返回类型，并根据需要通过参数或成员保存函数上下文。位置计算、值生成和使用计划应用宜保留各自入口；子节点应继续经过相应入口，避免重复应用转换或重复求值。

执行顺序应在后续生成设计中明确：结构体初始化按源码顺序，重复数组的元素只求值一次，短路右侧按条件执行，赋值先求 RHS 再求目标地址。Flow 可提供语义出口信息，但不能代替基本块与跳转的实际构造。

字段偏移、大小对齐、目标类型、ABI、存储分配及递归 clone/equality 的运行时实现，应由后续阶段规划。SemanticResult 保持为语义结果表；这些代码生成决定无需反向写入 AST 或 Checker。
