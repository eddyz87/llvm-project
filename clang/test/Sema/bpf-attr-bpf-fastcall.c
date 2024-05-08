// REQUIRES: bpf-registered-target
// RUN: %clang_cc1 %s -triple bpf -verify

__attribute__((bpf_fastcall)) int var; // expected-warning {{'bpf_fastcall' only applies to function types; type here is 'int'}}

__attribute__((bpf_fastcall)) void func();
__attribute__((bpf_fastcall(1))) void func_invalid(); // expected-error {{'bpf_fastcall' attribute takes no arguments}}

void test_no_attribute(int); // expected-note {{previous declaration is here}}
void __attribute__((bpf_fastcall)) test_no_attribute(int x) { } // expected-error {{function declared with 'bpf_fastcall' attribute was previously declared without the 'bpf_fastcall' attribute}}

void (*ptr1)(void) __attribute__((bpf_fastcall));
void (*ptr2)(void);
void bad_assign(void) {
  ptr2 = ptr1; // expected-error {{incompatible function pointer types assigning to 'void (*)(void)' from 'void (*)(void) __attribute__((bpf_fastcall))'}}
}
