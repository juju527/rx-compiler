#!/usr/bin/env python3
"""Compile and run the guide's examples; write all results below this folder."""
from __future__ import annotations

import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
EXAMPLES = ROOT / "examples"
TARGET_LINES = (
    'target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"',
    'target triple = "riscv32-unknown-none-elf"',
)


def resolve_tool(name: str) -> str | None:
    found = shutil.which(name)
    return str(Path(found).resolve()) if found else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", required=True, choices=("native", "target"))
    parser.add_argument("--clang", help="Compiler executable; default: clang or clang-22")
    parser.add_argument("--reimu", help="Existing REIMU executable, required in target mode")
    args = parser.parse_args()

    out_dir = BUILD / args.mode
    out_dir.mkdir(parents=True, exist_ok=True)
    (BUILD / "tmp").mkdir(exist_ok=True)
    env = os.environ.copy()
    env["TMPDIR"] = str(BUILD / "tmp")
    report = {
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "mode": args.mode,
        "host": platform.platform(),
        "source_target": "riscv32-unknown-none-elf / RV32IM / ILP32",
        "source_language_reference": "LLVM 22.1.0",
        "native_target_adjustment": args.mode == "native",
        "target_execution_verified": False,
        "scope": (
            "Native copies remove only the two target lines. This checks example "
            "behavior on the host, not the RV32 layout, ABI, or REIMU runtime."
            if args.mode == "native" else
            "Original RV32 modules are compiled with Clang 22 and executed in REIMU."
        ),
        "results": [],
    }

    def save() -> None:
        payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        (BUILD / f"validation-report.{args.mode}.json").write_text(payload)
        (BUILD / "validation-report.json").write_text(payload)

    compiler_name = args.clang or ("clang" if args.mode == "native" else "clang-22")
    compiler = resolve_tool(compiler_name)
    reimu = resolve_tool(args.reimu) if args.reimu else None
    if not compiler or (args.mode == "target" and not reimu):
        report["status"] = "blocked"
        report["reason"] = "Compiler or required REIMU executable was not found."
        save()
        print(report["reason"], file=sys.stderr)
        return 2

    version = subprocess.run(
        [compiler, "--version"], capture_output=True, text=True, env=env, check=True
    ).stdout.strip()
    report["compiler"] = compiler
    report["compiler_version"] = version
    if args.mode == "target" and not re.search(r"\bclang version 22(?:\.|\s)", version):
        report["status"] = "blocked"
        report["reason"] = "Target mode requires Clang 22; no native fallback is performed."
        save()
        print(report["reason"], file=sys.stderr)
        return 2
    if args.mode == "native":
        report["target_mode_not_run"] = (
            "This invocation explicitly selected native mode. Run target mode separately "
            "with Clang 22 and an existing REIMU executable."
        )

    manifest = json.loads((EXAMPLES / "manifest.json").read_text())
    for name, spec in manifest.items():
        source = EXAMPLES / spec["source"]
        expected = (EXAMPLES / spec["expected"]).read_bytes()
        original = source.read_text()
        item = {"name": name, "source": str(source.relative_to(ROOT)), "commands": []}
        report["results"].append(item)
        try:
            if not all(line in original.splitlines() for line in TARGET_LINES):
                raise ValueError("The original example is missing the expected target lines.")
            item["source_target_lines"] = "pass"
            if args.mode == "native":
                input_ir = out_dir / f"{name}.ll"
                input_ir.write_text(
                    "\n".join(line for line in original.splitlines()
                              if line not in TARGET_LINES) + "\n"
                )
                executable = out_dir / name
                compile_cmd = [compiler, "-x", "ir", str(input_ir), "-O0",
                               "-o", str(executable)]
                run_cmd = [str(executable)]
            else:
                assembly = out_dir / f"{name}.s"
                compile_cmd = [
                    compiler, "-S", "-x", "ir", str(source), "-o", str(assembly),
                    "--target=riscv32-unknown-none-elf", "-march=rv32im", "-mabi=ilp32",
                    "-O0", "-mllvm", "-riscv-no-aliases",
                ]
                run_cmd = [
                    reimu, "--memory=256M", "--stack=1M", "-f", str(assembly),
                    "-o", str(out_dir / f"{name}.stdout"),
                    "-p", str(out_dir / f"{name}.profile"),
                ]
            item["commands"].append(compile_cmd)
            compiled = subprocess.run(
                compile_cmd, cwd=out_dir, env=env, capture_output=True, timeout=30
            )
            (out_dir / f"{name}.compile.log").write_bytes(
                compiled.stdout + compiled.stderr
            )
            item["compile_exit_code"] = compiled.returncode
            if compiled.returncode:
                raise RuntimeError("Compilation failed; see the compile log.")
            if args.mode == "target":
                strip_script = ROOT.parent.parent / "scripts" / "strip_asm_debug.py"
                strip_cmd = [sys.executable, str(strip_script), str(assembly)]
                item["commands"].append(strip_cmd)
                subprocess.run(strip_cmd, cwd=out_dir, env=env, check=True, timeout=10)
                (out_dir / f"{name}.stdout").unlink(missing_ok=True)
            item["commands"].append(run_cmd)
            ran = subprocess.run(
                run_cmd, cwd=out_dir, env=env, input=b"", capture_output=True, timeout=10
            )
            (out_dir / f"{name}.run.log").write_bytes(ran.stdout + ran.stderr)
            actual = (
                ran.stdout if args.mode == "native"
                else (out_dir / f"{name}.stdout").read_bytes()
            )
            if args.mode == "native":
                (out_dir / f"{name}.stdout").write_bytes(actual)
            item["run_exit_code"] = ran.returncode
            item["expected_stdout"] = expected.decode("utf-8")
            item["actual_stdout"] = actual.decode("utf-8", errors="replace")
            item["status"] = "pass" if ran.returncode == 0 and actual == expected else "fail"
        except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as error:
            item["status"] = "fail"
            item["error"] = str(error)
        print(f"{item['status'].upper():4} {name}")

    report["passed"] = sum(item["status"] == "pass" for item in report["results"])
    report["total"] = len(report["results"])
    report["status"] = "pass" if report["passed"] == report["total"] else "fail"
    report["target_execution_verified"] = (
        args.mode == "target" and report["status"] == "pass"
    )
    save()
    print(f"{report['passed']}/{report['total']} passed ({args.mode})")
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
