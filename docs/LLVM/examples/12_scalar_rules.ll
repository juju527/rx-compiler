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
  %wrap = add i32 -1, 1
  call void @println_i32(i32 %wrap)
  %signed.less = icmp slt i32 -1, 1
  %signed.int = zext i1 %signed.less to i32
  call void @println_i32(i32 %signed.int)
  %unsigned.less = icmp ult i32 -1, 1
  %unsigned.int = zext i1 %unsigned.less to i32
  call void @println_i32(i32 %unsigned.int)
  %count = and i32 33, 31
  %signed.shift = ashr i32 -8, %count
  %unsigned.shift = lshr i32 -8, %count
  call void @println_i32(i32 %signed.shift)
  call void @println_i32(i32 %unsigned.shift)
  ret i32 0
}
