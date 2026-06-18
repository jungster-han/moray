#ifndef MORAY_VEC_H
#define MORAY_VEC_H

#include <stdlib.h>

/*
 * A type-safe growable array, built with macros so it works for any type.
 * C++ equivalent: std::vector<T>
 *
 * Because C has no generics, each element type needs a concrete, named
 * struct type. vector_define(T) generates that struct (vec_T) once, and
 * vector(T) refers to it afterwards. Anonymous structs can't be used here:
 * two anonymous structs with identical members are still *incompatible*
 * types, so you couldn't assign one to another or pass them around.
 *
 * Note: T must be a single identifier (no spaces), since it is pasted into
 * the type name. For pointer elements, make a typedef first:
 *   typedef Expr *ExprPtr;
 *   vector_define(ExprPtr)
 *
 * Usage:
 *   vector_define(int)                  // once, at file scope
 *   vector(int) nums = vector_new();
 *   vector_push(&nums, 42);
 *   vector_push(&nums, 99);
 *   printf("%d\n", nums.data[0]);       // 42
 *   vector_free(&nums);
 */

#define vector(T)          vec_##T

#define vector_define(T)                                            \
    typedef struct { T *data; int len; int cap; } vec_##T;

#define vector_new()       { NULL, 0, 0 }

#define vector_free(v)     free((v)->data)

#define vector_push(v, item)                                        \
    do {                                                            \
        if ((v)->len >= (v)->cap) {                                 \
            (v)->cap = (v)->cap == 0 ? 8 : (v)->cap * 2;           \
            (v)->data = realloc((v)->data,                          \
                                (v)->cap * sizeof(*(v)->data));     \
        }                                                           \
        (v)->data[(v)->len++] = (item);                             \
    } while (0)

#endif
