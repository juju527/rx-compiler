# ASTbuilder 测试

在项目根目录运行：

```sh
python3 scripts/test_ast.py
```

测试工具使用项目 Makefile 的编译器、头文件路径和 ANTLR 运行库配置，
编译独立的 probe，直接调用你的 ASTbuilder，不修改编译器实现。

- 读取 `tests/official/parser/manifest.json`，按 metadata.entry 的 crate、item、
  typeRef、expression 或 letStatement 入口处理源码片段，要求完整消费输入。
- 正例必须建树成功；负例必须正常报词法/语法错误。异常、崩溃和超时均算失败。
- 复用后续阶段的合法完整程序，检查 AST 建树与完整性，不作语义或运行结果判断。
- 精确比较专门设计的 AST 结构用例：优先级、结合性、四套表达式规则、后缀、
  块尾、引用层数、泛型、生命周期过滤、接收者、声明等。忽略 ID 的具体编号，
  但检查 ID 非零且不重复，以及必需子节点非空和 kind 与实际类型一致。
- 销毁 Parser、Lexer、TokenStream 后再读取 AST，验证它不依赖解析树对象的生存期。

输出位于 `target/ast-tests/`：report.md、results.json、build.log 和结构测试源码。
可单独复现一个用例：

```sh
target/ast-tests/ast-probe expression target/ast-tests/cases/left-associative-subtraction.rx
```

加 `--tree` 可在 JSON 中同时输出原始 ANTLR 解析树，用来区分 Parser 分组与
ASTbuilder 的构建结果。例如当前文法中 `if break a << b {}` 的条件是
`(break a) << b`；加括号 `if break (a << b) {}` 才将移位放入 break 的操作数。
测试同时记录两者，不将对语言语义的推测当成 AST 的预期结果。

现有 `make test` 的 runner 跳过 lex/parse 阶段，且 config.mk 配置为 Rust 参考实现，
因此本工具单独运行，不改变原有测试配置。
