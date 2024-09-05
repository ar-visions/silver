#ifndef _A_
#define _A_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define null 0L

#define emit(T, RT, N, ...) RT (*N)(__VA_ARGS__);

struct test1 {
    emit(test1, int, method, int, short)
};

/*
should emit:
struct test1 {
    int (*method)(int, short);
};
errors:

/home/kalen/src/silver/./silver/a.h:14:30: error: expected declaration specifiers or ‘...’ before ‘(’ token
   14 |     emit(test1, int, method, (int, short))
      |                              ^
/home/kalen/src/silver/./silver/a.h:11:36: note: in definition of macro ‘emit’
   11 | #define emit(T, RT, N, AR) RT (*N)(AR);
*/

#define type_def( TYPE, BASE ) \
    typedef struct TYPE { \
        struct TYPE##_t* type; \
        TYPE ## _M (TYPE, IN_I) \
    } TYPE; \
    typedef struct TYPE##_t { \
        struct BASE ## _t* parent; \
        TYPE ## _M (TYPE, IN_T) \
    } TYPE ## _t; \
    extern TYPE ## _t TYPE ## _type;

#define public__IN_T(RT, N)
#define public__IN_I(RT, N) RT n;

#define private__IN_T(RT, N)
#define private__IN_I(RT, N) RT n;

#define method__IN_T(RT, N, PARAMS, ...) RT (*N)(PARAMS)
#define method__IN_I(RT, N, PARAMS, ...)

#define public(T, ARG, RT, N)         public   ## __ ## ARG (RT, N)
#define private(T, ARG, RT, N)        private  ## __ ## ARG (RT, N)
#define method(T, ARG, RT, N, PARAMS) method   ## __ ## ARG (RT, N, PARAMS)

/// all methods are 'public'; if they were private they would not be given
/// the difference between public and private is meta exposure on member data
#define A_M(T, ARG) \
    public(T, ARG, int, refs) \
    method##__##IN_T (T, ARG, void, init, (A*)) \
    method(T, ARG, void, destructor, (A*)) \
    method(T, ARG, A*,   hold,       A*) \
    method(T, ARG, void, drop,       A*) \
    method(T, ARG, int,  compare,    A*)
type_def(A, A)

/// string type
#define string_M(T, ARG) \
    A_M(T, ARG) \
    public(T, ARG, char*, chars) \
    public(T, ARG, int32_t, alloc) \
    public(T, ARG, int32_t, len)
type_def(string, A)

#define array_M(T, ARG) \
    A_M(T, ARG) \
    public(T, ARG, A**,     elements) \
    public(T, ARG, int32_t, alloc) \
    public(T, ARG, int32_t, len) \
    method(T, ARG, void,    push, T*, A*)
type_def(array, A)


string* string_new_reserve(int);

A*      A_new(struct A_t*, size_t struct_size);

#define new(T) \
    (T*)A_new((A_t*)&T##_type,   sizeof(T))

#endif