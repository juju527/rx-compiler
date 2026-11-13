# LLVM IR 语法与实践入门

33 页中文学习讲义，面向当前 Rx 编译器，以 LLVM 22 的 opaque pointer 语法和项目的 RV32IM / ILP32 约定为基准。

- **阅读 PDF：** [llvm-ir-guide.pdf](llvm-ir-guide.pdf)
- **LaTeX 主文件：** [llvm-ir-guide.tex](llvm-ir-guide.tex)
- **样式与字体：** [preamble.tex](preamble.tex)
- **章节源码：** [chapters/](chapters/)
- **可运行示例：** [examples/](examples/)

## 内容

12 课依次介绍完整模块与打印 42、名字和类型、函数调用、SSA 与内存、整数运算、分支和短路、循环和 phi、数组与 GEP、结构体与聚合返回、项目接入、工具链、练习与答案。附录提供指令速查、术语和官方资料阅读顺序。

讲义明确区分 LLVM 的通用规则与本项目的表示选择，例如 bool 的计算值为 i1、存储值为 i8，聚合值由稳定存储承载，基础控制流用结果槽合流。phi 是阅读拓展，不要求立刻加入当前核心 IR。

## 重新编译 PDF

在本目录运行：

```sh
sh build.sh
```

需要 Tectonic（此次使用 0.17.0），首次运行可能需要下载缺失的 TeX 包。已包含本次使用的本地缓存；资源齐全时可以离线编译：

```sh
sh build.sh --offline
```

字体使用 macOS 的 Songti SC、Heiti SC、Times New Roman、Arial 和 Menlo。其他系统需要安装可用字体，或在 `preamble.tex` 中替换字体名。编译会把日志、中间文件、缓存和临时文件保存在本目录的 `build/`，最终 PDF 同时放在本目录顶层。

## 示例验证

12 个示例分别配有精确预期输出。末尾换行也参与比对。

```sh
python3 verify_examples.py --mode native --clang /usr/bin/clang
```

本次 **12/12 通过**，使用 Apple Clang 21，在 ARM64 macOS 编译和运行。本机模式只在 `build/native/` 内的副本中去掉目标 triple 与 datalayout，原始 `examples/*.ll` 保留 RV32 目标信息。

这不等于通过 LLVM 22 / RV32 / REIMU 目标验证。当前环境没有 `clang-22`，本次没有执行目标模式。工具准备好后运行：

```sh
python3 verify_examples.py --mode target \
  --clang /absolute/path/to/clang-22 \
  --reimu /absolute/path/to/reimu
```

替换成自己的现有工具路径。目标模式检查 Clang 主版本为 22，交叉编译 RV32IM 汇编，调用仓库既有的汇编清理脚本，再通过 REIMU 核对输出；不会自动安装工具或回退到本机模式。

- [validation-report.native.json](build/validation-report.native.json) 记录本次本机版本、实际命令和输出。
- `build/validation-report.json` 是最近一次执行的报告。
- `build/native/` 和 `build/target/` 分别保存对应模式的产物与日志。

LLVM 的独立结构验证命令、手动目标编译命令及常见错误定位，见讲义第 11 课。配套脚本验证这些手写例子，不调用学生编译器，也不能替代项目的源语言回归测试。

## 排版检查

`render_qa.py` 使用 Poppler 的 `pdftoppm` 及 Python 的 `pdfplumber`、`Pillow`，将 PDF 转成逐页图片和联系表，保存在 `build/qa/`。本次已结合这些图片检查正文、表格、代码、图示和分页；自动文字边界检查用于辅助发现裁切，不能替代人工查看。

## 主要参考资料

- [LLVM 22.1.0 Language Reference](https://releases.llvm.org/22.1.0/docs/LangRef.html)
- [LLVM GEP 说明](https://releases.llvm.org/22.1.0/docs/GetElementPtr.html)
- [LLVM Opaque Pointers](https://releases.llvm.org/22.1.0/docs/OpaquePointers.html)
- [Kaleidoscope 第 3 章](https://releases.llvm.org/22.1.0/docs/tutorial/MyFirstLanguageFrontend/LangImpl03.html)、[第 5 章](https://releases.llvm.org/22.1.0/docs/tutorial/MyFirstLanguageFrontend/LangImpl05.html)、[第 7 章](https://releases.llvm.org/22.1.0/docs/tutorial/MyFirstLanguageFrontend/LangImpl07.html)
- [Rx 后端规范](https://github.com/ACMClassCourse-2025/rx-compiler-specification/blob/master/src/backend.md)
- 项目当前的 `docs/ir.md`、`docs/semantic.md` 和 `include/ir/ir.hpp`。

讲解与示例为本项目编写，所有新增文件及验证产物均位于 `docs/LLVM/`。
