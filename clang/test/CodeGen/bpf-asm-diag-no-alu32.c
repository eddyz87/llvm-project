// REQUIRES: bpf-registered-target
// RUN: %clang_cc1 -triple bpf -target-feature -alu32 %s -S -o /dev/null -verify

extern unsigned long bar(void);

void foo(void) {
  unsigned long v64 = bar();
  asm volatile ("%[reg] += 1"::[reg]"r"((unsigned long)  v64));
  asm volatile ("%[reg] += 1"::[reg]"r"((unsigned int)   v64)); // expected-warning {{value size does not match register size specified by the constraint and modifier}}
  asm volatile ("%[reg] += 1"::[reg]"r"((unsigned short) v64)); // expected-warning {{value size does not match register size specified by the constraint and modifier}}
  asm volatile ("%[reg] += 1"::[reg]"r"((unsigned char)  v64)); // expected-warning {{value size does not match register size specified by the constraint and modifier}}
}
