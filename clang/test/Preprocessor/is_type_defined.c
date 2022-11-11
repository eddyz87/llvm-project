// RUN: %clang -cc1 -ast-print -o - %s | FileCheck %s

struct foo {};
union bar {};
enum buz { X };
typedef int quux;

/* void foofn() {} */
#define mmmacro 42

#if __is_type_defined(foo)
const int a = 1;
#endif
#if __is_type_defined(bar)
const int b = 2;
#endif
#if __is_type_defined(buz)
const int c = 3;
#endif
#if __is_type_defined(quux)
const int d = 4;
#endif
#if __is_type_defined(foofn)
const int should_not_be_defined_func_name = 5;
#endif
#if __is_type_defined(goo)
const int should_not_be_defined_some_name = 6;
#endif
#if __is_type_defined(mmmacro)
const int should_not_be_defined_macro_name = 7;
#endif

// CHECK: const int a
// CHECK: const int b
// CHECK: const int c
// CHECK: const int d
// CHECK-NOT: should_not_be_defined
