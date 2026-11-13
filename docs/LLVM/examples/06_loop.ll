target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

define i32 @sum_to(i32 %n) {
entry:
  %i.addr = alloca i32, align 4
  %sum.addr = alloca i32, align 4
  store i32 1, ptr %i.addr, align 4
  store i32 0, ptr %sum.addr, align 4
  br label %test
test:
  %i = load i32, ptr %i.addr, align 4
  %more = icmp sle i32 %i, %n
  br i1 %more, label %body, label %exit
body:
  %sum = load i32, ptr %sum.addr, align 4
  %sum.next = add i32 %sum, %i
  store i32 %sum.next, ptr %sum.addr, align 4
  %i.next = add i32 %i, 1
  store i32 %i.next, ptr %i.addr, align 4
  br label %test
exit:
  %answer = load i32, ptr %sum.addr, align 4
  ret i32 %answer
}

define i32 @main() {
entry:
  %answer = call i32 @sum_to(i32 10)
  call void @println_i32(i32 %answer)
  %empty = call i32 @sum_to(i32 0)
  call void @println_i32(i32 %empty)
  ret i32 0
}
