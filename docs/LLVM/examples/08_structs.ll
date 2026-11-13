target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

%Record = type { i8, i32, i8 }

define i32 @main() {
entry:
  %record = alloca %Record, align 4
  %flag = getelementptr %Record, ptr %record, i32 0, i32 0
  %value = getelementptr %Record, ptr %record, i32 0, i32 1
  %tail = getelementptr %Record, ptr %record, i32 0, i32 2
  store i8 1, ptr %flag, align 1
  store i32 42, ptr %value, align 4
  store i8 0, ptr %tail, align 1
  %answer = load i32, ptr %value, align 4
  call void @println_i32(i32 %answer)
  %flag.byte = load i8, ptr %flag, align 1
  %flag.bit = trunc i8 %flag.byte to i1
  %flag.int = zext i1 %flag.bit to i32
  call void @println_i32(i32 %flag.int)
  ret i32 0
}
