target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

define internal void @rx_source_main() {
entry:
  call void @println_i32(i32 42)
  ret void
}

define i32 @main() {
entry:
  call void @rx_source_main()
  ret i32 0
}
