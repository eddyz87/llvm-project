; RUN: llc < %s -march=bpfel -mattr=+alu32 -verify-machineinstrs | FileCheck %s
; RUN: llc < %s -march=bpfeb -mattr=+alu32 -verify-machineinstrs | FileCheck %s
declare dso_local i32 @bar()

define dso_local void @foo() {
entry:
  %v32 = call i32 @bar()
  tail call void asm sideeffect "$0 += 42", "r"(i32 %v32)
  ret void
}

; CHECK: w[[#]] += 42
