target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

define i32 @main() {
entry:
  %a = alloca [3 x i32], align 4
  %p0 = getelementptr [3 x i32], ptr %a, i32 0, i32 0
  %p1 = getelementptr [3 x i32], ptr %a, i32 0, i32 1
  %p2 = getelementptr [3 x i32], ptr %a, i32 0, i32 2
  store i32 10, ptr %p0, align 4
  store i32 42, ptr %p1, align 4
  store i32 30, ptr %p2, align 4
  %index = add i32 0, 1
  %element = getelementptr [3 x i32], ptr %a, i32 0, i32 %index
  %answer = load i32, ptr %element, align 4
  call void @println_i32(i32 %answer)
  ret i32 0
}
