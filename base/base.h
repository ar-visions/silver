#ifndef _BASE_
#define _BASE_

#include <stdlib.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifndef typeof
#ifdef _MSC_VER
#define typeof decltype
#else
#define typeof __typeof__
#endif
#endif

#ifdef _MSC_VER
#include <algorithm>
#define min std::min
#define max std::max
#define _thread_local_  __declspec(thread)
#else
#define _thread_local_  __thread
#ifndef max
#define max(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#endif
#ifndef min
#define min(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#endif
#endif

#ifdef __cplusplus
    #define global_construct(f) \
        static void f(void); \
        struct f##_t_ { f##_t_(void) { f(); } }; static f##_t_ f##_; \
        static void f(void)
#elif defined(_MSC_VER)
    #pragma section(".CRT$XCU",read)
    #define global_construct2_(f,p) \
        static void f(void); \
        __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f; \
        __pragma(comment(linker,"/include:" p #f "_")) \
        static void f(void)
    #ifdef _WIN64
        #define global_construct(f) global_construct2_(f,"")
    #else
        #define global_construct(f) global_construct2_(f,"_")
    #endif
#else
    #define global_construct(f) \
        static void f(void) __attribute__((constructor)); \
        static void f(void)
#endif

#define alloc(T) ((T *)alloc_bytes(sizeof(T)))

struct _Base;
typedef void *(*Method)();
typedef void (*BaseMethod)(struct _Base *);
typedef void *(*Setter)(struct _Base *, void *);
typedef void *(*Getter)(struct _Base *);

typedef struct _Class {
	struct _Class *parent;
	const char *class_name;
	unsigned int flags;
	int object_size;
	InitMethod _init;
    int *mcount;
	unsigned char *mtypes;
	const char **mnames;
	Method **m[1];
} Class;

#define CLASS_FLAG_ASSEMBLED   1
#define CLASS_FLAG_PREINIT     2
#define CLASS_FLAG_INIT        4
#define CLASS_FLAG_NO_INIT     8

#define null NULL

EXPORT void *alloc_bytes(size_t);
EXPORT Base new_object(Class, size_t);
EXPORT void free_object(Base);
EXPORT void class_assemble(Class);
EXPORT void class_init();
EXPORT bool class_inherits(Class, Class);
EXPORT Class class_find(const char *name);
EXPORT Base object_inherits(Base o, Class c);

#endif
