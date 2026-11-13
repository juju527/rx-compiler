target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

define i1 @rhs() {
entry:
  call void @println_i32(i32 999)
  ret i1 true
}

define i1 @logical_and(i1 %left) {
entry:
  %result.addr = alloca i8, align 1
  br i1 %left, label %eval.rhs, label %skip.rhs
eval.rhs:
  %right = call i1 @rhs()
  %right.byte = zext i1 %right to i8
  store i8 %right.byte, ptr %result.addr, align 1
  br label %merge
skip.rhs:
  store i8 0, ptr %result.addr, align 1
  br label %merge
merge:
  %byte = load i8, ptr %result.addr, align 1
  %result = trunc i8 %byte to i1
  ret i1 %result
}

define i32 @main() {
entry:
  %a = call i1 @logical_and(i1 false)
  %a.int = zext i1 %a to i32
  call void @println_i32(i32 %a.int)
  %b = call i1 @logical_and(i1 true)
  %b.int = zext i1 %b to i32
  call void @println_i32(i32 %b.int)
  ret i32 0
}
