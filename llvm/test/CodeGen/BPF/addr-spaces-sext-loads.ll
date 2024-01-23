; RUN: llc --asm-show-inst -mtriple=bpfel -mcpu=v4 -filetype=obj < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s

; Generated from the following C code:
;
; #define __uptr __attribute__((address_space(272)))
;
; long test_load(void __uptr *foo) {
;   long b = *((volatile int   __uptr *)(foo + 16));
;   long c = *((volatile short __uptr *)(foo + 24));
;   long d = *((volatile char  __uptr *)(foo + 32));
;   return b + c + d;
; }
;
; Using the following command:
;
;   clang --target=bpf -O2 -S -emit-llvm -o t.ll t.c

; Function Attrs: mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define dso_local i64 @test_load(ptr addrspace(272) noundef %foo) local_unnamed_addr #0 {
entry:
  %add.ptr = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 16
  %0 = load volatile i32, ptr addrspace(272) %add.ptr, align 4, !tbaa !3
  %conv = sext i32 %0 to i64
  %add.ptr1 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 24
  %1 = load volatile i16, ptr addrspace(272) %add.ptr1, align 2, !tbaa !7
  %conv2 = sext i16 %1 to i64
  %add.ptr3 = getelementptr inbounds i8, ptr addrspace(272) %foo, i64 32
  %2 = load volatile i8, ptr addrspace(272) %add.ptr3, align 1, !tbaa !9
  %conv4 = sext i8 %2 to i64
  %add = add nsw i64 %conv2, %conv
  %add5 = add nsw i64 %add, %conv4
  ret i64 %add5
}
; CHECK: <test_load>
; CHECK:  81 [[#]] 10 00 10 01 00 00	r[[#]] = *(s32 *)(r[[#]] + 0x10)
; CHECK:  89 [[#]] 18 00 10 01 00 00	r[[#]] = *(s16 *)(r[[#]] + 0x18)
; CHECK:  91 [[#]] 20 00 10 01 00 00	r[[#]] = *(s8  *)(r[[#]] + 0x20)

attributes #0 = { mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"frame-pointer", i32 2}
!2 = !{!"clang version 18.0.0git (git@github.com:eddyz87/llvm-project.git 750ccccde67bef6c44daa3fd9d822b57373a3e45)"}
!3 = !{!4, !4, i64 0}
!4 = !{!"int", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!8, !8, i64 0}
!8 = !{!"short", !5, i64 0}
!9 = !{!5, !5, i64 0}
