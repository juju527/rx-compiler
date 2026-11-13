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
  %x.addr = alloca i32, align 4
  %r.addr = alloca ptr
  store i32 40, ptr %x.addr, align 4
  store ptr %x.addr, ptr %r.addr
  %r = load ptr, ptr %r.addr
  %old = load i32, ptr %r, align 4
  %new = add i32 %old, 2
  store i32 %new, ptr %r, align 4
  %answer = load i32, ptr %x.addr, align 4
  call void @println_i32(i32 %answer)
  ret i32 0
}
