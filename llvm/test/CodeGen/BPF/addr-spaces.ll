; RUN: llc --asm-show-inst -mtriple=bpfel -mcpu=v2 -filetype=obj < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s
; RUN: llc --asm-show-inst -mtriple=bpfel -mcpu=v3 -filetype=obj < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s
; RUN: llc --asm-show-inst -mtriple=bpfel -mcpu=v4 -filetype=obj < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s

; Generated from the following C code:
;
; #define __uptr __attribute__((address_space(272)))
;
; void test_store(void __uptr *foo) {
;   *((volatile long  __uptr *)(foo + 8)) = 0xaa;
;   *((volatile int   __uptr *)(foo + 16)) = 0xbb;
;   *((volatile short __uptr *)(foo + 24)) = 0xcc;
;   *((volatile char  __uptr *)(foo + 32)) = 0xdd;
; }
;
; long test_load(void __uptr *foo) {
;   long a = *((volatile unsigned long  __uptr *)(foo + 8));
;   long b = *((volatile unsigned int   __uptr *)(foo + 16));
;   long c = *((volatile unsigned short __uptr *)(foo + 24));
;   long d = *((volatile unsigned char  __uptr *)(foo + 32));
;   return a + b + c + d;
; }
;
; Using the following command:
;
;   clang --target=bpf -O2 -S -emit-llvm -o t.ll t.c

; Function Attrs: nofree norecurse nounwind memory(argmem: readwrite, inaccessiblemem: readwrite)
define dso_local void @test_store(ptr addrspace(272) noundef %foo) local_unnamed_addr #0 {
entry:
  %add.ptr = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 8
  store volatile i64 170, ptr addrspace(272) %add.ptr, align 8, !tbaa !3
  %add.ptr1 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 16
  store volatile i32 187, ptr addrspace(272) %add.ptr1, align 4, !tbaa !7
  %add.ptr2 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 24
  store volatile i16 204, ptr addrspace(272) %add.ptr2, align 2, !tbaa !9
  %add.ptr3 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 32
  store volatile i8 -35, ptr addrspace(272) %add.ptr3, align 1, !tbaa !11
  ret void
}

; CHECK: <test_store>:
; CHECK: 7b [[#]] 08 00 10 01 00 00	*(u64 *)(r[[#]] + 0x8) = r[[#]]
; CHECK: 63 [[#]] 10 00 10 01 00 00	*(u32 *)(r[[#]] + 0x10) = r[[#]]
; CHECK: 6b [[#]] 18 00 10 01 00 00	*(u16 *)(r[[#]] + 0x18) = r[[#]]
; CHECK: 73 [[#]] 20 00 10 01 00 00	*(u8 *)(r[[#]] + 0x20) = r[[#]]

; Function Attrs: mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define dso_local i64 @test_load(ptr addrspace(272) noundef %foo) local_unnamed_addr #1 {
entry:
  %add.ptr = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 8
  %0 = load volatile i64, ptr addrspace(272) %add.ptr, align 8, !tbaa !3
  %add.ptr1 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 16
  %1 = load volatile i32, ptr addrspace(272) %add.ptr1, align 4, !tbaa !7
  %conv = zext i32 %1 to i64
  %add.ptr2 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 24
  %2 = load volatile i16, ptr addrspace(272) %add.ptr2, align 2, !tbaa !9
  %conv3 = zext i16 %2 to i64
  %add.ptr4 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 32
  %3 = load volatile i8, ptr addrspace(272) %add.ptr4, align 1, !tbaa !11
  %conv5 = zext i8 %3 to i64
  %add = add nsw i64 %0, %conv
  %add6 = add nsw i64 %add, %conv3
  %add7 = add nsw i64 %add6, %conv5
  ret i64 %add7
}

; CHECK: <test_load>:
; CHECK: 79 [[#]] 08 00 10 01 00 00	r[[#]] = *(u64 *)(r[[#]] + 0x8)
; CHECK: 61 [[#]] 10 00 10 01 00 00	r[[#]] = *(u32 *)(r[[#]] + 0x10)
; CHECK: 69 [[#]] 18 00 10 01 00 00	r[[#]] = *(u16 *)(r[[#]] + 0x18)
; CHECK: 71 [[#]] 20 00 10 01 00 00	r[[#]] = *(u8 *)(r[[#]] + 0x20)

attributes #0 = { nofree norecurse nounwind memory(argmem: readwrite, inaccessiblemem: readwrite) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #1 = { mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"frame-pointer", i32 2}
!2 = !{!"some clang version"}
!3 = !{!4, !4, i64 0}
!4 = !{!"long", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!8, !8, i64 0}
!8 = !{!"int", !5, i64 0}
!9 = !{!10, !10, i64 0}
!10 = !{!"short", !5, i64 0}
!11 = !{!5, !5, i64 0}
