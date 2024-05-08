// REQUIRES: bpf-registered-target
// RUN: %clang_cc1 -triple bpf -emit-llvm -disable-llvm-passes %s -o - | FileCheck %s

#define __bpf_fastcall __attribute__((bpf_fastcall))

void test(void) __bpf_fastcall;
void (*ptr)(void) __bpf_fastcall;

void foo(void) {
  test();
  (*ptr)();
}

// CHECK: @ptr = global ptr null
// CHECK: define {{.*}} @foo()
// CHECK: entry:
// CHECK:   call bpf_fastcall void @test()
// CHECK:   %[[ptr:.*]] = load ptr, ptr @ptr, align 8
// CHECK:   call bpf_fastcall void %[[ptr]]()
// CHECK:   ret void

// CHECK: declare bpf_fastcall void @test() #[[ptr_attr:[0-9]+]]
