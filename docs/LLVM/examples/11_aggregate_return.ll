target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

define void @make_pair(ptr %out) {
entry:
  %first = getelementptr [2 x i32], ptr %out, i32 0, i32 0
  %second = getelementptr [2 x i32], ptr %out, i32 0, i32 1
  store i32 20, ptr %first, align 4
  store i32 22, ptr %second, align 4
  ret void
}

define i32 @main() {
entry:
  %pair = alloca [2 x i32], align 4
  call void @make_pair(ptr %pair)
  %first = getelementptr [2 x i32], ptr %pair, i32 0, i32 0
  %second = getelementptr [2 x i32], ptr %pair, i32 0, i32 1
  %a = load i32, ptr %first, align 4
  %b = load i32, ptr %second, align 4
  %answer = add i32 %a, %b
  call void @println_i32(i32 %answer)
  ret i32 0
}
