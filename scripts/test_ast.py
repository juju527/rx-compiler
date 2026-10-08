#!/usr/bin/env python3
"""Test the C++ ASTbuilder using official parser cases and exact AST expectations."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from collections import Counter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "target/ast-tests"


def n(kind, **fields):
    return {"kind": kind, **fields}


def segment(name, args=(), kind="Identifier"):
    return {"kind": kind, "name": name, "args": list(args)}


def path(name, args=(), kind="Identifier"):
    return n("PathExpr", path=[segment(name, args, kind)])


def typ(name, args=()):
    return n("PathType", path=[segment(name, args)])


def integer(text, base="Decimal", suffix="None"):
    return n("IntLiteral", text=str(text), base=base, suffix=suffix)


def unary(op, value):
    return n("UnaryExpr", op=op, operand=value)


def binary(op, left, right):
    return n("BinaryExpr", op=op, left=left, right=right)


def block(statements=(), tail=None):
    return n("BlockExpr", statements=list(statements), tail=tail)


def statement(value, semi=True):
    return n("ExprStmt", expression=value, has_semicolon=semi)


def function(name, body, receiver="Null", parameters=(), return_type=None):
    return n("FunctionItem", name=name, body=body, receiver=receiver,
             parameters=list(parameters), return_type=return_type)


def shape_cases():
    cases = []

    def add(name, source, expected, entry="expression", tree=False):
        cases.append(dict(name=name, source=source, expected_ast=expected,
                          entry=entry, accept=True, group="structure", tree=tree))

    add("integer-decimal", "1_000", integer("1_000"))
    for text, base, suffix in [("0b10_01u32", "Binary", "U32"),
                               ("0o7_1usize", "Octal", "USize"),
                               ("0xF_Fi32", "Hex", "I32"),
                               ("42isize", "Decimal", "ISize")]:
        add("integer-" + base + "-" + suffix, text, integer(text, base, suffix))
    add("boolean-true", "true", n("BoolLiteral", value=True))
    add("boolean-false", "false", n("BoolLiteral", value=False))
    add("unit-expression", "()", n("UnitExpr"))
    add("parentheses-no-extra-node", "(((a)))", path("a"))
    add("self-value-path", "self", path("self", kind="SelfValue"))
    add("self-type-path", "Self::N", n("PathExpr", path=[segment("Self", kind="SelfType"), segment("N")]))
    add("contextual-identifier", "Copy", path("Copy"))
    add("generic-expression-path", "Vec::<'a, i32>", path("Vec", [typ("i32")]))

    operators = {"+": "Add", "-": "Subtract", "*": "Multiply", "/": "Divide",
                 "%": "Remainder", "<<": "ShiftLeft", ">>": "ShiftRight",
                 "&": "BitAnd", "|": "BitOr", "^": "BitXor", "==": "Equal",
                 "!=": "NotEqual", "<": "Less", "<=": "LessEqual", ">": "Greater",
                 ">=": "GreaterEqual", "&&": "LogicalAnd", "||": "LogicalOr"}
    for spelling, op in operators.items():
        source, expected = f"a {spelling} b", binary(op, path("a"), path("b"))
        add("ordinary-" + op, source, expected)
        add("condition-" + op, f"if {source} {{}}",
            n("IfExpr", condition=expected, then_branch=block(), else_branch=None))
        # Current grammar's first conditionBreakShift alternative ends at `a`;
        # an unparenthesized shift belongs to the enclosing condition tree.
        # This expectation is confirmed by the raw ANTLR tree, not AST output.
        condition = n("BreakExpr", value=expected)
        is_shift = op in ("ShiftLeft", "ShiftRight")
        if is_shift:
            condition = binary(op, n("BreakExpr", value=path("a")), path("b"))
            add("condition-break-parenthesized-" + op, f"if break ({source}) {{}}",
                n("IfExpr", condition=n("BreakExpr", value=expected), then_branch=block(), else_branch=None))
        add("condition-break-" + op, f"if break {source} {{}}",
            n("IfExpr", condition=condition, then_branch=block(), else_branch=None), tree=is_shift)
        add("statement-" + op, "{" + source + "}", block(tail=expected))

    add("left-associative-subtraction", "a-b-c",
        binary("Subtract", binary("Subtract", path("a"), path("b")), path("c")))
    add("multiplication-before-addition", "a+b*c",
        binary("Add", path("a"), binary("Multiply", path("b"), path("c"))))
    add("mixed-shifts-source-order", "a << b >> c << d",
        binary("ShiftLeft", binary("ShiftRight", binary("ShiftLeft", path("a"), path("b")), path("c")), path("d")))
    add("bitwise-precedence", "a | b ^ c & d",
        binary("BitOr", path("a"), binary("BitXor", path("b"), binary("BitAnd", path("c"), path("d")))))
    add("logical-precedence", "a || b && c",
        binary("LogicalOr", path("a"), binary("LogicalAnd", path("b"), path("c"))))
    add("comments-do-not-create-nodes", "a /* ignored */ + // ignored\nb", binary("Add", path("a"), path("b")))

    assignments = {"=": "Assign", "+=": "Add", "-=": "Subtract", "*=": "Multiply",
                   "/=": "Divide", "%=": "Remainder", "&=": "BitAnd", "|=": "BitOr",
                   "^=": "BitXor", "<<=": "ShiftLeft", ">>=": "ShiftRight"}
    for spelling, op in assignments.items():
        add("assignment-" + op, f"a {spelling} b", n("AssignExpr", op=op, target=path("a"), value=path("b")))
    add("right-associative-assignment", "a=b=c", n("AssignExpr", op="Assign", target=path("a"),
        value=n("AssignExpr", op="Assign", target=path("b"), value=path("c"))))

    for spelling, plan in [("-", ["Negate"]), ("!", ["Not"]), ("*", ["Dereference"]),
                           ("&", ["BorrowShared"]), ("&mut ", ["BorrowMutable"]),
                           ("&&", ["BorrowShared", "BorrowShared"]),
                           ("&&mut ", ["BorrowShared", "BorrowMutable"])]:
        value = path("x")
        for op in reversed(plan):
            value = unary(op, value)
        add("unary-" + "-".join(plan), spelling + "x", value)
    add("condition-break-prefix-greediness", "if break -x+y {}",
        n("IfExpr", condition=n("BreakExpr", value=binary("Add", unary("Negate", path("x")), path("y"))),
          then_branch=block(), else_branch=None))

    def cast(value, target):
        return n("CastExpr", value=value, target_type=target)
    add("chained-casts", "x as i32 as u32", cast(cast(path("x"), typ("i32")), typ("u32")))
    add("closed-cast-before-less", "x as (T) < y", binary("Less", cast(path("x"), typ("T")), path("y")))
    add("generic-cast-before-shift", "x as Vec<i32> << y",
        binary("ShiftLeft", cast(path("x"), typ("Vec", [typ("i32")])), path("y")))
    add("qualified-generic-cast-before-less", "x as a::B<i32> < y",
        binary("Less", cast(path("x"), n("PathType", path=[segment("a"), segment("B", [typ("i32")])])), path("y")))
    add("closed-expression-last-operand", "a + b as (T) < c",
        binary("Less", binary("Add", path("a"), cast(path("b"), typ("T"))), path("c")))

    def call(callee, args=()):
        return n("CallExpr", callee=callee, arguments=list(args))
    def method(receiver, name, args=(), types=()):
        return n("MethodCallExpr", receiver=receiver, method=segment(name, types), arguments=list(args))
    add("empty-call", "f()", call(path("f")))
    add("call-order", "f(1,2,)", call(path("f"), [integer(1), integer(2)]))
    add("method-call", "a.b()", method(path("a"), "b"))
    add("call-through-field", "(a.b)()", call(n("FieldExpr", base=path("a"), field="b")))
    add("mixed-postfix-chain", "a.b::<T>(1)[i].c",
        n("FieldExpr", base=n("IndexExpr", base=method(path("a"), "b", [integer(1)], [typ("T")]), index=path("i")), field="c"))
    add("recursive-postfix-base", "f(g())[h()]", n("IndexExpr", base=call(path("f"), [call(path("g"))]), index=call(path("h"))))
    add("block-leading-dot-suffix", "{{1}.m()[i]}", block(tail=n("IndexExpr", base=method(block(tail=integer(1)), "m"), index=path("i"))))

    add("empty-array", "[]", n("ArrayExpr", elements=[]))
    add("array-order", "[1,2,3,]", n("ArrayExpr", elements=[integer(1), integer(2), integer(3)]))
    add("array-repeat-no-expansion", "[f(); 3usize]", n("ArrayRepeatExpr", value=call(path("f")), count=integer("3usize", suffix="USize")))
    add("array-repeat-negative-path", "[x; -N]", n("ArrayRepeatExpr", value=path("x"), count=unary("Negate", path("N"))))
    add("empty-struct-distinct-from-path", "S{}", n("StructExpr", path=[segment("S")], fields=[]))
    add("struct-fields-and-generics", "S::<i32>{x:1,y:2,}", n("StructExpr", path=[segment("S", [typ("i32")])],
        fields=[{"name": "x", "value": integer(1)}, {"name": "y", "value": integer(2)}]))

    empty_stmt = n("EmptyStmt")
    add("empty-block", "{}", block())
    add("tail-integer", "{1}", block(tail=integer(1)))
    add("semicolon-removes-tail", "{1;}", block([statement(integer(1))]))
    add("unit-tail-is-present", "{()}", block(tail=n("UnitExpr")))
    add("bare-block-tail", "{{1}}", block(tail=block(tail=integer(1))))
    add("block-semicolon", "{{1};}", block([statement(block(tail=integer(1)))]))
    add("empty-statement-not-skipped", "{{1}; ;}", block([statement(block(tail=integer(1))), empty_stmt]))
    add("final-empty-statement-prevents-tail", "{{1} ;}", block([statement(block(tail=integer(1)))]))
    add("non-final-block-statement", "{{1} 2}", block([statement(block(tail=integer(1)), False)], integer(2)))
    if_expr = n("IfExpr", condition=path("p"), then_branch=block(tail=integer(1)), else_branch=block(tail=integer(2)))
    add("if-else", "if p {1} else {2}", if_expr)
    add("if-is-block-tail", "{if p {1} else {2}}", block(tail=if_expr))
    add("else-if", "if p {} else if q {} else {}", n("IfExpr", condition=path("p"), then_branch=block(),
        else_branch=n("IfExpr", condition=path("q"), then_branch=block(), else_branch=block())))
    add("loop", "loop {break 1;}", n("LoopExpr", body=block([statement(n("BreakExpr", value=integer(1)))])))
    add("while", "while a && b {continue;}", n("WhileExpr", condition=binary("LogicalAnd", path("a"), path("b")),
        body=block([statement(n("ContinueExpr"))])))
    for keyword, kind in [("break", "BreakExpr"), ("return", "ReturnExpr")]:
        add(keyword + "-empty", keyword, n(kind, value=None))
        add(keyword + "-value", keyword + " 1", n(kind, value=integer(1)))
    add("continue", "continue", n("ContinueExpr"))

    def ref(inner, mutable=False):
        return n("RefType", inner=inner, mutability="Mutable" if mutable else "Immutable")
    for name, source, expected in [
        ("unit-type", "()", n("UnitType")),
        ("parenthesized-type", "((T))", typ("T")),
        ("shared-reference", "&T", ref(typ("T"))),
        ("mutable-reference", "&mut T", ref(typ("T"), True)),
        ("double-reference", "&&T", ref(ref(typ("T")))),
        ("double-mutable-lifetime-reference", "&&'a mut T", ref(ref(typ("T"), True))),
        ("array-type", "[&mut i32; 2usize]", n("ArrayType", element=ref(typ("i32"), True), length=integer("2usize", suffix="USize"))),
        ("nested-generics", "Vec<Vec<i32>>", typ("Vec", [typ("Vec", [typ("i32")])])),
        ("filtered-lifetime-argument", "Vec<'a, i32>", typ("Vec", [typ("i32")]))]:
        add(name, source, expected, "typeRef")

    add("typed-mutable-let", "let mut x: i32 = 1;", n("LetStmt", name="x", mutability="Mutable", annotation=typ("i32"), initializer=integer(1)), "letStatement")
    add("untyped-let", "let x = ();", n("LetStmt", name="x", mutability="Immutable", annotation=None, initializer=n("UnitExpr")), "letStatement")
    add("empty-crate", "", n("Crate", items=[]), "crate")
    add("use-is-filtered", "use foo::{bar as baz, *};", n("Crate", items=[]), "crate")
    add("use-item-is-null", "use foo::bar;", None, "item")
    add("empty-function", "fn main() {}", function("main", block()), "item")
    for source, receiver in [("self", "Value"), ("mut self", "MutableValue"), ("&self", "SharedReference"), ("&'a mut self", "MutableReference")]:
        add("receiver-" + receiver, f"fn f({source}) {{}}", function("f", block(), receiver=receiver), "item")
    add("signature-and-lifetimes", "fn f<'a>(mut x: &'a mut T) -> () where T: 'a { () }",
        function("f", block(tail=n("UnitExpr")), parameters=[{"name": "x", "mutability": "Mutable", "type": ref(typ("T"), True)}], return_type=n("UnitType")), "item")
    constant = n("ConstItem", name="N", type=typ("usize"), value=integer("1usize", suffix="USize"))
    add("constant", "const N: usize = 1usize;", constant, "item")
    add("negative-constant", "const N:i32 = -(1);", n("ConstItem", name="N", type=typ("i32"), value=unary("Negate", integer(1))), "item")
    add("derive-groups-and-fields", "#[derive(Copy,Clone)] #[derive(PartialEq,Eq)] struct S {x:i32, y:&mut bool,}",
        n("StructItem", name="S", fields=[{"name": "x", "type": typ("i32")}, {"name": "y", "type": ref(typ("bool"), True)}],
          derives=[["Copy", "Clone"], ["PartialEq", "Eq"]]), "item")
    add("impl-associated-items", "impl<'a> &'a mut S where S:'a { const N:usize=1usize; fn f(&'a self) {} }",
        n("ImplItem", target_type=ref(typ("S"), True), items=[constant, function("f", block(), receiver="SharedReference")]), "item")
    # A remaining '=' fragment after a generic close is initialization, not >=.
    add("generic-close-then-equals", "let x:Vec<i32>=y;",
        n("LetStmt", name="x", mutability="Immutable", annotation=typ("Vec", [typ("i32")]), initializer=path("y")), "letStatement")
    add("double-generic-close-then-equals", "let x:Vec<Vec<i32>>=y;",
        n("LetStmt", name="x", mutability="Immutable", annotation=typ("Vec", [typ("Vec", [typ("i32")])]), initializer=path("y")), "letStatement")
    return cases


def strip_ids(value):
    if isinstance(value, dict):
        return {k: strip_ids(v) for k, v in value.items() if k != "id"}
    if isinstance(value, list):
        return [strip_ids(v) for v in value]
    return value


def source_hashes():
    files = [ROOT / "src/main.cpp", *sorted((ROOT / "src/ast").glob("*.cpp")),
             *sorted((ROOT / "include/ast").glob("*.hpp")), *sorted((ROOT / "grammar").glob("*.g4"))]
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}


def run_case(case):
    result = {k: v for k, v in case.items() if k != "expected_ast"}
    try:
        command = [str(OUT / "ast-probe"), case["entry"], str(case["file"])]
        if case.get("tree"):
            command.append("--tree")
        process = subprocess.run(command,
                                 capture_output=True, text=True, timeout=10, cwd=ROOT)
        result.update(returncode=process.returncode, stderr=process.stderr)
        try:
            payload = json.loads(process.stdout)
        except json.JSONDecodeError:
            payload = {"status": "invalid-output", "output": process.stdout}
        result["payload"] = payload
        expected_code = 0 if case["accept"] else 1
        result["passed"] = process.returncode == expected_code and payload.get("status") == ("ok" if case["accept"] else "parse-error")
        if result["passed"] and "expected_ast" in case:
            result["passed"] = strip_ids(payload["ast"]) == case["expected_ast"]
            if not result["passed"]:
                result["expected_ast"] = case["expected_ast"]
                result["actual_ast"] = strip_ids(payload["ast"])
    except subprocess.TimeoutExpired:
        result.update(passed=False, returncode=None, payload={"status": "timeout"})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    OUT.mkdir(parents=True, exist_ok=True)
    before = source_hashes()
    with (OUT / "build.log").open("w") as log:
        built = subprocess.run(["make", "-f", "Makefile", "-f", "tests/custom/ast/Makefile",
                                "target/ast-tests/ast-probe"], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    if built.returncode:
        print("AST test probe build failed:", OUT / "build.log", file=sys.stderr)
        return 2
    cases = []
    manifest = ROOT / "tests/official/parser/manifest.json"
    for case in json.loads(manifest.read_text()):
        cases.append(dict(name=case["source"], group="official-parser",
                          file=str(manifest.parent / case["source"]),
                          entry=case.get("metadata", {}).get("entry", "crate"),
                          accept=case["compilation_success"]))
    # Accepted later-stage programs are additional AST inputs, not semantic/I/O assertions.
    seen_programs = set()
    for stage in ("semantic", "codegen", "optimization"):
        for manifest in sorted((ROOT / "tests/official" / stage).rglob("manifest.json")):
            for case in json.loads(manifest.read_text()):
                if not case["compilation_success"]:
                    continue
                file = manifest.parent / case["source"]
                fingerprint = hashlib.sha256(file.read_bytes()).digest()
                if fingerprint in seen_programs:
                    continue
                seen_programs.add(fingerprint)
                cases.append(dict(name=str(file.relative_to(ROOT)), group="official-program",
                                  file=str(file), entry="crate", accept=True))
    for case in shape_cases():
        file = OUT / "cases" / (case["name"] + ".rx")
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(case["source"])
        cases.append({**case, "file": str(file)})
    print(f"Running {len(cases)} AST checks…", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(run_case, cases))
    counts = Counter((r["group"], r["passed"]) for r in results)
    failed = [r for r in results if not r["passed"]]
    kinds, rules = set(), set()
    for result in results:
        if result["payload"].get("status") == "ok":
            kinds.update(result["payload"]["kinds"])
            rules.update(result["payload"]["visited_rules"])
    stable = before == source_hashes()
    summary = dict(total=len(results), passed=len(results)-len(failed), failed=len(failed),
                   source_stable=stable, source_sha256=before, node_kinds=sorted(kinds),
                   visited_rules=sorted(rules), results=results)
    (OUT / "results.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    report = ["# AST 测试结果", "", "使用当前 C++ ASTbuilder；未运行 Rust 参考编译器。", "",
              "| 检查 | 通过 | 总数 |", "|---|---:|---:|"]
    for group in ("official-parser", "official-program", "structure"):
        yes, no = counts[group, True], counts[group, False]
        print(f"{group}: {yes}/{yes+no}")
        report.append(f"| {group} | {yes} | {yes+no} |")
    report += ["", f"AST 节点种类覆盖：{len(kinds)}/34。Visitor 实际访问规则：{len(rules)}/147。",
               "忽略的语法子树不会全部进入 Visitor；此数量不表示分支覆盖率。", "",
               "每个成功 AST 均检查了必需子节点、节点 ID 唯一性、kind 与动态类型一致性，",
               "并在 ANTLR 对象销毁后读取 AST。结构用例另比较全部字段（忽略具体 ID 数值）。", "",
               "official-program 仅检验建树与结构完整性，不验证语义或程序运行输出。", "",
               "## 已核对的解析树边界", "",
               "当前 ANTLR 解析树将 `if break a << b {}` 的条件分组为 `(break a) << b`，",
               "右移同理；`if break (a << b) {}` 则将移位放在 break 的操作数内。",
               "ASTbuilder 与这些解析树一致。这四项分别检查，两个无括号用例在 results.json",
               "中附有原始 parse_tree，避免将解析树分组差异误报为 ASTbuilder 错误。", "",
               f"测试期间编译器源码是否保持一致：{'是' if stable else '否，需要重跑'}。", "",
               "复现：`python3 scripts/test_ast.py`。详细结果见同目录 results.json。"]
    if failed:
        report += ["", "## 失败用例", ""]
        for result in failed:
            report += [f"- {result['name']} ({result['entry']}): {result['payload'].get('status')}",
                       f"  {result['payload'].get('error', 'AST fields differ')}；源文件：{result['file']}"]
    (OUT / "report.md").write_text("\n".join(report) + "\n")
    print(f"Node kinds: {len(kinds)}/34; visited rules: {len(rules)}/147; source stable: {stable}")
    print("Report:", OUT / "report.md")
    for result in failed[:12]:
        print("FAIL", result["name"], result["payload"].get("status"), result["payload"].get("error", "AST fields differ"))
    return 0 if not failed and stable else 1


if __name__ == "__main__":
    sys.exit(main())
