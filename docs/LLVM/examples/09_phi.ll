target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-none-elf"

@.line = private constant [4 x i8] c"%d\0A\00"
declare i32 @printf(ptr, ...)

define internal void @println_i32(i32 %x) {
entry:
  %written = call i32 (ptr, ...) @printf(ptr @.line, i32 %x)
  ret void
}

define i32 @max_phi(i32 %a, i32 %b) {
entry:
  %choose.a = icmp sgt i32 %a, %b
  br i1 %choose.a, label %then, label %else
then:
  br label %merge
else:
  br label %merge
merge:
  %result = phi i32 [ %a, %then ], [ %b, %else ]
  ret i32 %result
}

define i32 @main() {
entry:
  %answer = call i32 @max_phi(i32 17, i32 42)
  call void @println_i32(i32 %answer)
  ret i32 0
}
