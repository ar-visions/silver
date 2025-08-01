#ifndef _macros_h
#define _macros_h

#define   enum_value_DECL(E, T, N, VAL)             static const E E##_##N = VAL;
#define   enum_value_COUNT(E, T, N, VAL)            1,
#define   enum_value_METHOD(E, T, N, VAL)
#define   enum_value_IMPL(E, T, N, VAL) \
    E##_i.type.members[E## _i.type.member_count].name     = #N; \
    E##_i.type.members[E## _i.type.member_count].offset   = (i32)(E##_##N);\
    E##_i.type.members[E## _i.type.member_count].type     = &T ## _i.type; \
    static T static_##N = VAL; \
    E##_i.type.members[E## _i.type.member_count].ptr      = &static_##N;\
    E##_i.type.members[E## _i.type.member_count].member_type = A_FLAG_ENUMV; \
    E##_i.type.member_count++;

#define   enum_value(E,T,Y, N, VAL)                enum_value_##Y(E, T, N, VAL)

#define   enum_method_DECL(E, T, R, N, ...)
#define   enum_method_COUNT(E, T, R, N, ...)
#define   enum_method_IMPL(E, T, R, N, ...) \
    E##_i.type . N = & E## _ ## N; \
    E##_i.type.members[E##_i.type.member_count].name    = #N; \
    E##_i.type.members[E##_i.type.member_count].args    = (meta_t) { emit_types(__VA_ARGS__) }; \
    E##_i.type.members[E##_i.type.member_count].type    = (AType)&R##_i.type; \
    E##_i.type.members[E##_i.type.member_count].offset  = offsetof(E##_f, N); \
    E##_i.type.members[E##_i.type.member_count].ptr     = (void*)& E##_##N; \
    E##_i.type.members[E##_i.type.member_count].member_type = A_FLAG_SMETHOD; \
    E##_i.type.member_count++; 
#define   enum_method_METHOD(E, T, R, N, ...)    R (*N)(E value __VA_OPT__(,) __VA_ARGS__);
#define   enum_method(E,T,Y,R,N,...)            enum_method_##Y(E,T, R,N __VA_OPT__(,) __VA_ARGS__)



#define   enum_value_v_DECL(E, T, N, VAL)             static const E E##_##N = VAL;
#define   enum_value_v_COUNT(E, T, N, VAL)            1,
#define   enum_value_v_METHOD(E, T, N, VAL)
#define   enum_value_v_IMPL(E, T, N, VAL) \
    E##_i.type.members[E## _i.type.member_count].name     = #N; \
    E##_i.type.members[E## _i.type.member_count].offset   = (i64)E##_##N;\
    E##_i.type.members[E## _i.type.member_count].type     = (AType)&T ## _i.type; \
    static T static_##N = VAL; \
    E##_i.type.members[E## _i.type.member_count].ptr      = &static_##N;\
    E##_i.type.members[E## _i.type.member_count].member_type = A_FLAG_ENUMV; \
    E##_i.type.member_count++;

#define   enum_value_vargs_DECL(E, T, N, VAL,...)             static const E E##_##N = VAL;
#define   enum_value_vargs_COUNT(E, T, N, VAL,...)            1,
#define   enum_value_vargs_METHOD(E, T, N, VAL,...)
#define   enum_value_vargs_IMPL(E, T, N, VAL,...) \
    E##_i.type.members[E## _i.type.member_count].name     = #N; \
    E##_i.type.members[E## _i.type.member_count].offset   = (i64)E##_##N;\
    E##_i.type.members[E## _i.type.member_count].type     = (AType)&T ## _i.type; \
    E##_i.type.members[E## _i.type.member_count].member_type = A_FLAG_ENUMV; \
    static T static_##N = VAL; \
    E##_i.type.members[E## _i.type.member_count].ptr      = &static_##N;\
    E##_i.type.members[E## _i.type.member_count].args     = (meta_t) { emit_types(__VA_ARGS__) }; \
    E##_i.type.member_count++;

//#define   enum_value_v(E,T,Y, N,VAL)                enum_value_v_##Y(X, N,VAL)

#define ARG_COUNT_HELPER(_1, _2, N, ...) N
#define ARG_COUNT_NO_ZERO(...) \
    ARG_COUNT_HELPER(__VA_ARGS__, 2, 1, 0)

#define CONCAT_HELPER(x, y) x##y
#define CONCAT(x, y) CONCAT_HELPER(x, y)

#define   enum_value_v_1(E,T,Y, N,VAL)             enum_value_v_##Y(E, T, N,VAL)
#define   enum_value_v_2(E,T,Y, N,VAL,T1)          enum_value_vargs_##Y(E, T, N,VAL,T1)
#define   enum_value_v_3(E,T,Y, N,VAL,T1,T2)       enum_value_vargs_##Y(E, T, N,VAL,T1,T2)
#define   enum_value_v_4(E,T,Y, N,VAL,T1,T2,T3)    enum_value_vargs_##Y(E, T, N,VAL,T1,T2,T3)
#define   enum_value_v_5(E,T,Y, N,VAL,T1,T2,T3,T4) enum_value_vargs_##Y(E, T, N,VAL,T1,T2,T3,T4)
#define   enum_value_v(E,T,Y, N,VAL, ...) \
    CONCAT(enum_value_v_, ARG_COUNT_NO_ZERO(__VA_ARGS__))(E,T,Y, N,VAL __VA_OPT__(,) __VA_ARGS__)

// runtime type-check api
#ifndef NDEBUG
    #define TC(MEMBER, VALUE) VALUE
#else
    #define TC(MEMBER, VALUE) VALUE
#endif

#define NULL_TOKEN
#define ARG_COUNT_NZ_IMPL(NULL_TOKEN, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N

#define ARG_COUNT_NZ(...) _ARG_COUNT_IMPL(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _ARG_COUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N
#define _ARG_COUNT_I(...) _ARG_COUNT_IMPL(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define _ARG_COUNT(...)   _ARG_COUNT_I("A-type", ## __VA_ARGS__)
#define _COMBINE_(A, B)   A##B
#define _COMBINE(A, B)    _COMBINE_(A, B)
#define _N_ARGS_0( TYPE)
#define _N_ARGS_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, A) const void *: (void)0)(instance, a)
#define _N_ARGS_2( TYPE, a,b) instance->a = TC(a,b);
#define _N_ARGS_4( TYPE, a,b, c,d) \
        _N_ARGS_2 (TYPE, a,b) instance->c = TC(c,d);
#define _N_ARGS_6( TYPE, a,b, c,d, e,f) \
        _N_ARGS_4 (TYPE, a,b, c,d) instance->e = TC(e,f);
#define _N_ARGS_8( TYPE, a,b, c,d, e,f, g,h) \
        _N_ARGS_6 (TYPE, a,b, c,d, e,f) instance->g = TC(g,h);
#define _N_ARGS_10(TYPE, a,b, c,d, e,f, g,h, i,j) \
        _N_ARGS_8 (TYPE, a,b, c,d, e,f, g,h) instance->i = TC(i,j);
#define _N_ARGS_12(TYPE, a,b, c,d, e,f, g,h, i,j, l,m) \
        _N_ARGS_10(TYPE, a,b, c,d, e,f, g,h, i,j) instance->l = TC(l,m);
#define _N_ARGS_14(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o) \
        _N_ARGS_12(TYPE, a,b, c,d, e,f, g,h, i,j, l,m) instance->n = TC(n,o);
#define _N_ARGS_16(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q) \
        _N_ARGS_14(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o) instance->p = TC(p,q);
#define _N_ARGS_18(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s) \
        _N_ARGS_16(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q) instance->r = TC(r,s);
#define _N_ARGS_20(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s, t,u) \
        _N_ARGS_18(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s) instance->t = TC(t,u);
#define _N_ARGS_22(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s, t,u, v,w) \
        _N_ARGS_20(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s, t,u) instance->v = TC(v,w);
#define _N_ARGS_HELPER2(TYPE, N, ...)  _COMBINE(_N_ARGS_, N)(TYPE, ## __VA_ARGS__)
#define _N_ARGS(TYPE,...)    _N_ARGS_HELPER2(TYPE, _ARG_COUNT(__VA_ARGS__), ## __VA_ARGS__)


//#define _N_STRUCT_ARGS_0( TYPE) _N_ARGS_2( TYPE )
#define _N_STRUCT_ARGS_1( TYPE, a) \
    ({ TYPE instance = _Generic((a), TYPE##_schema(TYPE, GENERICS, A) const void *: (void)0)(a); instance; }) 

#define _N_STRUCT_ARGS_2( TYPE, ...) ({ TYPE instance = (TYPE) { __VA_ARGS__ }; instance; })
#define _N_STRUCT_ARGS_3( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_4( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_5( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_6( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_7( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_8( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_9( TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_10(TYPE, ...) _N_STRUCT_ARGS_2( TYPE, __VA_ARGS__ )
#define _N_STRUCT_ARGS_HELPER2(TYPE, N, ...)  _COMBINE(_N_STRUCT_ARGS_, N)(TYPE, ## __VA_ARGS__)
#define _N_STRUCT_ARGS(TYPE,...)    _N_STRUCT_ARGS_HELPER2(TYPE, _ARG_COUNT(__VA_ARGS__), ## __VA_ARGS__)

#define structure_of(TYPE, ...) _N_STRUCT_ARGS(TYPE, __VA_ARGS__);

#define _ARG_COUNT_IMPL2(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N
#define _ARG_COUNT2(...)        _ARG_COUNT_IMPL2(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define _COMBINE_2(A, B)        A##B
#define _COMBINE2(A, B)         _COMBINE_2(A, B)
#define _F(T, a,b)              (((1 << (i64)T##_##a) * b)) 
#define _F_ARGS_0(T)
#define _F_ARGS_1(T)                  
#define _F_ARGS_2(T, a,b)                    _F(T, a, b)
#define _F_ARGS_4(T, a,b, c,d)               _F_ARGS_2 (a,b) | _F(T, c,d)
#define _F_ARGS_6(T, a,b, c,d, e,f)          _F_ARGS_4 (a,b, c, d) | _F(T, e,f)
#define _F_ARGS_8(T, a,b, c,d, e,f, g,h)     _F_ARGS_6 (a,b, c, d, e, f) | _F(T, g,h)
#define _F_ARGS(T,...)    _F_ARGS_HELPER2(T, _ARG_COUNT2(__VA_ARGS__), __VA_ARGS__)
#define _F_ARGS_HELPER2(T, N, ...)  _COMBINE2(_F_ARGS_, N)(T, __VA_ARGS__)
#define flags(T, ...) _F_ARGS(T, __VA_ARGS__);

#define new(TYPE, ...) \
    ({ \
        TYPE instance = (TYPE)alloc(typeid(TYPE), 1); \
        _N_ARGS(TYPE, ## __VA_ARGS__); \
        A_initialize((A)instance); \
        instance; \
    })
#define new2(TYPE, ...) \
    ({ \
        TYPE instance = (TYPE)alloc(typeid(TYPE), 1); \
        TYPE##_N_ARGS(TYPE, ## __VA_ARGS__); \
        A_initialize((A)instance); \
        instance; \
    })

/// with construct we give it a dynamic type, symbols and A-values
#define construct(type, ...) \
    ({ \
        T instance = (T)alloc(type, 1); \
        _N_ARGS(instance, ## __VA_ARGS__); \
        A_initialize((A)instance); \
        instance; \
    })

#define new0(T, ...) \
    ({ \
        T instance = (T)alloc(typeid(T), 1); \
        _N_ARGS(instance, ## __VA_ARGS__); \
        A_initialize((A)instance); \
        instance; \
    })

#define allocate(T, ...) \
    ({ \
        T instance = (T)alloc(typeid(T), 1); \
        _N_ARGS(instance, ## __VA_ARGS__); \
        instance; \
    })

#define valloc(T, N)                ((A)alloc(typeid(T), N))
#define ftable(TYPE, INSTANCE)      ((TYPE##_f*)((A)INSTANCE)[-1].type)
#define isa(INSTANCE)               (INSTANCE ? (struct _A_f*)((struct _A*)INSTANCE - 1)->type : (struct _A_f*)0)
// see: javascript; returns null if its not an instance-of; faults if you give it a null
//#define instanceof(left, type)      A_instanceof(left, typeid(type))
#define ftableI(I)                  ((__typeof__((I)->f)) ((A)(I))[-1].type)
#define fcall(I,M,...)              ({ __typeof__(I) _i_ = I; ftableI(_i_)->M(_i_, ## __VA_ARGS__); })
#define mcall(I,M,...)              ({ __typeof__(I) _i_ = I; (_i_) ? ftableI(_i_)->M(_i_, ## __VA_ARGS__) : 0; })
#define cstring(I)                  cast(cstr, I)
#define val(T,V)                    primitive(typeid(T), (&(T){V}))
#define idx_1(I,T1,V1)              fcall(I, index ##_## T1, V1)
#define idx_2(I,T1,T2,V1,V2)        fcall(I, index ##_## T1 ##_## T2, V1, V2)
#define idx(I,V1)                   fcall(I, index ##_## num, V1)
#define meta_t(I,IDX)               isa(I) -> meta.meta_##IDX
#define ctr(T,WITH,...)             A_initialize(T##_i.type.with_##WITH(alloc(typeid(T), 1), ## __VA_ARGS__))
#define ctr1(T,WITH,...)            A_initialize(T##_i.type.with_##WITH(alloc(typeid(T), 1), ## __VA_ARGS__))
#define alloc_ctr(T,WITH,...)       A_initialize(T##_i.type.with_##WITH(alloc(typeid(T), 1), ## __VA_ARGS__))
#define str(CSTR)                   string_i.type.with_symbol((string)alloc((AType)&string_i.type, 1), (symbol)(CSTR))
#define addr_validateI(I)           ({ \
    __typeof__(I) *addr = &I; \
    I \
})

/// arg expansion for type emission (give address of its statically defined)
#define emit_types(...)             EXPAND_ARGS(__VA_ARGS__)
#define combine_tokens_(A, B)       A##B
#define combine_tokens(A, B)        combine_tokens_(A, B)
#define EXPAND_ARGS(...)            EXPAND_ARGS_HELPER(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define EXPAND_ARGS_HELPER(N, ...)  combine_tokens(EXPAND_ARGS_, N)(__VA_ARGS__)
#define EXPAND_ARGS_0()                                0
#define EXPAND_ARGS_1(a)                               1, (AType)&a##_i.type
#define EXPAND_ARGS_2(a, b)                            2, (AType)&a##_i.type, (AType)&b##_i.type
#define EXPAND_ARGS_3(a, b, c)                         3, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type
#define EXPAND_ARGS_4(a, b, c, d)                      4, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type
#define EXPAND_ARGS_5(a, b, c, d, e)                   5, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type, (AType)&e##_i.type
#define EXPAND_ARGS_6(a, b, c, d, e, f)                6, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type, (AType)&e##_i.type, (AType)&f##_i.type
#define EXPAND_ARGS_7(a, b, c, d, e, f, g)             7, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type, (AType)&e##_i.type, (AType)&f##_i.type, (AType)&g##_i.type
#define EXPAND_ARGS_8(a, b, c, d, e, f, g, h)          8, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type, (AType)&e##_i.type, (AType)&f##_i.type, (AType)&g##_i.type, (AType)&h##_i.type
#define EXPAND_ARGS_9(a, b, c, d, e, f, g, h, ii)      9, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type, (AType)&e##_i.type, (AType)&f##_i.type, (AType)&g##_i.type, (AType)&h##_i.type, (AType)&ii##_i.type
#define EXPAND_ARGS_10(a, b, c, d, e, f, g, h, ii, j) 10, (AType)&a##_i.type, (AType)&b##_i.type, (AType)&c##_i.type, (AType)&d##_i.type, (AType)&e##_i.type, (AType)&f##_i.type, (AType)&g##_i.type, (AType)&h##_i.type, (AType)&ii##_i.type, (AType)&j##_i.type
//#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define COUNT_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define COUNT_ARGS(...)             COUNT_ARGS_IMPL(dummy, ## __VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

//#define EXPAND_ARGS2_0()                       
#define EXPAND_ARGS2_1(a)                        a
#define EXPAND_ARGS2_2(a, b)                     a##_##b
#define EXPAND_ARGS2_3(a, b, c)                  a##_##b##_##c
#define EXPAND_ARGS2_4(a, b, c, d)               a##_##b##_##c##_##d
#define EXPAND_ARGS2_5(a, b, c, d, e)            a##_##b##_##c##_##d##_e
#define EXPAND_ARGS2_6(a, b, c, d, e, f)         a##_##b##_##c##_##d##_e##_##f
#define EXPAND_ARGS2_7(a, b, c, d, e, f, g)      a##_##b##_##c##_##d##_e##_##f##_##g
#define EXPAND_ARGS2_8(a, b, c, d, e, f, g, h)   a##_##b##_##c##_##d##_e##_##f##_##g##_##h
#define emit_idx_symbol(...)             EXPAND_ARGS2(__VA_ARGS__)
#define EXPAND_ARGS2(...)            EXPAND_ARGS_HELPER2(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define EXPAND_ARGS_HELPER2(N, ...)  combine_tokens(EXPAND_ARGS2_, N)(__VA_ARGS__)

#define   i_ctr_interface_F(X, ARG)
#define   i_ctr_interface_F_EXTERN(X, ARG)
#define   i_ctr_interface_INST(X, ARG)
#define   i_ctr_interface_INST_EXTERN(X, ARG)
#define   i_ctr_interface_DECL(X, ARG)
#define   i_ctr_interface_DECL_EXTERN(X, ARG)
#define   i_ctr_interface_MEMBER_INDEX(X, ARG)
#define   i_ctr_interface_GENERICS(X, ARG)
#define   i_ctr_interface_INIT(X, ARG) \
    member_validate[validate_count++] = #ARG;
#define   i_ctr_interface_PROTO(X, ARG)
#define   i_ctr_interface_METHOD(X, ARG)





#define   i_ctr_public_F(X, ARG)
#define   i_ctr_public_F_EXTERN(X, ARG)
#define   i_ctr_public_INST(X, ARG)
#define   i_ctr_public_INST_EXTERN(X, ARG)
#define   i_ctr_public_DECL(X, ARG) X X##_with_##ARG(X, ARG);
#define   i_ctr_public_DECL_EXTERN(X, ARG) X X##_with_##ARG(X, ARG);
#define   i_ctr_public_GENERICS(X, ARG) ARG: X##_i.type.with_##ARG,

#define   i_ctr_public_INIT(X, ARG) \
    X##_i.type.with_##ARG = & X##_with_##ARG; \
    X##_i.type.members[X##_i.type.member_count].name        = stringify(with_##ARG); \
    X##_i.type.members[X##_i.type.member_count].args        = (meta_t) { emit_types(ARG) }; \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&ARG##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(X##_f, with_##ARG); \
    X##_i.type.members[X##_i.type.member_count].ptr         = (void*)& X##_with_##ARG; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_CONSTRUCT; \
    X##_i.type.member_count++;  
#define   i_ctr_public_PROTO(X, ARG)
#define   i_ctr_public_METHOD(X, ARG)      X (*with_##ARG)(X, ARG);

#define   i_ctr_intern_F(X, ARG)
#define   i_ctr_intern_F_EXTERN(X, ARG)
#define   i_ctr_intern_INST(X, ARG)
#define   i_ctr_intern_INST_EXTERN(X, ARG)
#define   i_ctr_intern_INIT(X, ARG) 
#define   i_ctr_intern_PROTO(X, ARG)
#define   i_ctr_intern_METHOD(X, ARG)
#define   i_ctr_intern_METHOD_EXTERN(X, ARG)
#define   i_ctr(X, Y, T, ARG)              i_ctr_##T##_##Y(X, ARG)

#define   i_prop_opaque_F(X, R, N)              u8 N;
#define   i_prop_opaque_F_EXTERN(X, R, N)       u8 N;
#define   i_prop_opaque_INST(X, R, N)           R N;
#define   i_prop_opaque_INST_EXTERN(X, R, N)    i_prop_public_INST(X, R, N)
#define   i_prop_opaque_DECL(X, R, N)           i_prop_public_DECL(X, R, N)
#define   i_prop_opaque_DECL_EXTERN(X, R, N)    i_prop_public_DECL(X, R, N)
#define   i_prop_opaque_GENERICS(X, R, N)
#define   i_prop_opaque_INIT(X, R, N)
#define   i_prop_opaque_PROTO(X, R, N)  
#define   i_prop_opaque_METHOD(X, R, N)


#define   i_prop_interface_F(X, R, N)
#define   i_prop_interface_F_EXTERN(X, R, N)
#define   i_prop_interface_INST(X, R, N)
#define   i_prop_interface_INST_EXTERN(X, R, N)
#define   i_prop_interface_DECL(X, R, N)
#define   i_prop_interface_DECL_EXTERN(X, R, N)
#define   i_prop_interface_GENERICS(X, R, N)
#define   i_prop_interface_INIT(X, R, N) \
    member_validate[validate_count++] = #N;
#define   i_prop_interface_PROTO(X, R, N)  
#define   i_prop_interface_METHOD(X, R, N)

#define   i_prop_public_F(X, R, N)              u8 N;
#define   i_prop_public_F_EXTERN(X, R, N)       u8 N;
#define   i_prop_public_INST(X, R, N)           R N;
#define   i_prop_public_INST_EXTERN(X, R, N)    i_prop_public_INST(X, R, N)  
#define   i_prop_public_DECL(X, R, N)           
#define   i_prop_public_DECL_EXTERN(X, R, N)     
#define   i_prop_public_GENERICS(X, R, N)
#define   i_prop_public_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].name        = #N;                                \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(struct _##X, N);          \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&R##_i.type;                  \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_PROP;                     \
    X##_i.type.members[X##_i.type.member_count].id          = offsetof(struct X##_fields, N);    \
    X##_i.type.member_count++;
#define   i_prop_public_PROTO(X, R, N)  
#define   i_prop_public_METHOD(X, R, N)

#define   i_prop_required_F(X, R, N)            u8 N;
#define   i_prop_required_F_EXTERN(X, R, N)     u8 N;
#define   i_prop_required_INST(X, R, N)         i_prop_public_INST(X, R, N)
#define   i_prop_required_INST_EXTERN(X, R, N)  i_prop_public_INST(X, R, N)
#define   i_prop_required_DECL(X, R, N)         i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN(X, R, N)  i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS(X, R, N)
#define   i_prop_required_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].required = true; \
    i_prop_public_INIT(X, R, N)
#define   i_prop_required_PROTO(X, R, N)  
#define   i_prop_required_METHOD(X, R, N)

#define   i_prop_intern_F(X, R, N)              u8 N;
#define   i_prop_intern_F_EXTERN(X, R, N)       
#define   i_prop_intern_INST(X, R, N)           R N;
#define   i_prop_intern_INST_EXTERN(X, R, N)    R _##N;
#define   i_prop_intern_DECL(X, R, N)           i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN(X, R, N)    i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS(X, R, N)
#define   i_prop_intern_INIT(X, R, N)
#define   i_prop_intern_PROTO(X, R, N)  
#define   i_prop_intern_METHOD(X, R, N)






#define   i_prop_public_F_field(X, R, N, M2)            u8 N;
#define   i_prop_public_F_EXTERN_field(X, R, N, M2)     u8 N;
#define   i_prop_public_INST_field(X, R, N, M2)         R N;
#define   i_prop_public_INST_EXTERN_field(X, R, N, M2)  i_prop_public_INST_field(X, R, N, M2)
#define   i_prop_public_DECL_field(X, R, N, M2)         i_prop_public_DECL(X, R, N)
#define   i_prop_public_DECL_EXTERN_field(X, R, N, M2)  i_prop_public_DECL(X, R, N)
#define   i_prop_public_GENERICS_field(X, R, N, M2)
#define   i_prop_public_INIT_field(X, R, N, M2) \
    X##_i.type.members[X##_i.type.member_count].name        = #M2;                               \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(struct _##X, N);          \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&R##_i.type;                  \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_PROP;                     \
    X##_i.type.members[X##_i.type.member_count].id          = offsetof(struct X##_fields, N);    \
    X##_i.type.member_count++;

#define   i_prop_public_PROTO_field(X, R, N, M2)  
#define   i_prop_public_METHOD_field(X, R, N, M2)

#define   i_prop_required_F_field(X, R, N, M2)              u8 N;
#define   i_prop_required_F_EXTERN_field(X, R, N, M2)       u8 N;
#define   i_prop_required_INST_field(X, R, N, M2)           i_prop_public_INST_field(X, R, N, M2)
#define   i_prop_required_INST_EXTERN_field(X, R, N, M2)    i_prop_public_INST_field(X, R, N, M2)
#define   i_prop_required_DECL_field(X, R, N, M2)           i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN_field(X, R, N, M2)    i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS_field(X, R, N, M2)
#define   i_prop_required_INIT_field(X, R, N, M2) \
    X##_i.type.members[X##_i.type.member_count].required = true; \
    i_prop_public_INIT_field(X, R, N, M2)
#define   i_prop_required_PROTO_field(X, R, N, M2)  
#define   i_prop_required_METHOD_field(X, R, N, M2)

#define   i_prop_intern_F_field(X, R, N, M2)                u8 N;
#define   i_prop_intern_F_EXTERN_field(X, R, N, M2)         
#define   i_prop_intern_INST_field(X, R, N, M2)             R N;
#define   i_prop_intern_INST_EXTERN_field(X, R, N, M2)      R N;
#define   i_prop_intern_DECL_field(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN_field(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS_field(X, R, N, M2)
#define   i_prop_intern_INIT_field(X, R, N, M2)          fault("field is not exposed when intern");
#define   i_prop_intern_PROTO_field(X, R, N, M2)  
#define   i_prop_intern_METHOD_field(X, R, N, M2)  






#define   i_prop_public_F_meta(X, R, N, M2)             u8 N;
#define   i_prop_public_F_EXTERN_meta(X, R, N, M2)      u8 N;
#define   i_prop_public_INST_meta(X, R, N, M2)          R N;
#define   i_prop_public_INST_EXTERN_meta(X, R, N, M2)   R N;
#define   i_prop_public_DECL_meta(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_public_DECL_EXTERN_meta(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_public_GENERICS_meta(X, R, N, M2)
#define   i_prop_public_INIT_meta(X, R, N, M2) \
    X##_i.type.members[X##_i.type.member_count].args = (meta_t) { 1, (AType)&M2##_i.type }; \
    i_prop_public_INIT(X, R, N)

#define   i_prop_public_PROTO_meta(X, R, N, M2)  
#define   i_prop_public_METHOD_meta(X, R, N, M2)

#define   i_prop_required_F_meta(X, R, N, M2)           u8 N;
#define   i_prop_required_F_EXTERN_meta(X, R, N, M2)    u8 N;
#define   i_prop_required_INST_meta(X, R, N, M2)        R N;
#define   i_prop_required_INST_EXTERN_meta(X, R, N, M2) R N;
#define   i_prop_required_DECL_meta(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN_meta(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS_meta(X, R, N, M2)
#define   i_prop_required_INIT_meta(X, R, N, M2) \
    X##_i.type.members[X##_i.type.member_count].required = true; \
    i_prop_public_INIT_meta(X, R, N, M2)
#define   i_prop_required_PROTO_meta(X, R, N, M2)  
#define   i_prop_required_METHOD_meta(X, R, N, M2)

#define   i_prop_intern_F_meta(X, R, N, M2)             u8 N;
#define   i_prop_intern_F_EXTERN_meta(X, R, N, M2)      
#define   i_prop_intern_INST_meta(X, R, N, M2)           R N;
#define   i_prop_intern_INST_EXTERN_meta(X, R, N, M2)    R _##N;
#define   i_prop_intern_DECL_meta(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN_meta(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS_meta(X, R, N, M2)
#define   i_prop_intern_INIT_meta(X, R, N, M2)
#define   i_prop_intern_PROTO_meta(X, R, N, M2)  
#define   i_prop_intern_METHOD_meta(X, R, N, M2)  


#define   i_prop_public_F_as(X, R, N, M2)               u8 N;
#define   i_prop_public_F_EXTERN_as(X, R, N, M2)        u8 N;
#define   i_prop_public_INST_as(X, R, N, M2)            R N;
#define   i_prop_public_INST_EXTERN_as(X, R, N, M2)     M2 N;
#define   i_prop_public_DECL_as(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_public_DECL_EXTERN_as(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_public_GENERICS_as(X, R, N, M2)
#define   i_prop_public_INIT_as(X, R, N, M2) \
    verify(false, "'as' keyword is used for internals")
#define   i_prop_public_PROTO_as(X, R, N, M2)  
#define   i_prop_public_METHOD_as(X, R, N, M2)

#define   i_prop_required_F_as(X, R, N, M2)             u8 N;
#define   i_prop_required_F_EXTERN_as(X, R, N, M2)      u8 N;
#define   i_prop_required_INST_as(X, R, N, M2)          R N;
#define   i_prop_required_INST_EXTERN_as(X, R, N, M2)   M2 N;
#define   i_prop_required_DECL_as(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN_as(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS_as(X, R, N, M2)
#define   i_prop_required_INIT_as(X, R, N, M2) \
    verify(false, "'as' keyword used with internals and cannot be required");
#define   i_prop_required_PROTO_as(X, R, N, M2)  
#define   i_prop_required_METHOD_as(X, R, N, M2)

#define   i_prop_intern_F_as(X, R, N, M2)               u8 N;
#define   i_prop_intern_F_EXTERN_as(X, R, N, M2)        
#define   i_prop_intern_INST_as(X, R, N, M2)           R N;
#define   i_prop_intern_INST_EXTERN_as(X, R, N, M2)    M2 _##N;
#define   i_prop_intern_DECL_as(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN_as(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS_as(X, R, N, M2)
#define   i_prop_intern_INIT_as(X, R, N, M2)
#define   i_prop_intern_PROTO_as(X, R, N, M2)  
#define   i_prop_intern_METHOD_as(X, R, N, M2)  








#define i_prop_1(X, Y, T, R, N, ...) \
    i_prop_##T##_##Y(X, R, N)

#define i_prop_2(X, Y, T, R, N, MODE2, M2) \
    i_prop_##T##_##Y##_##MODE2(X, R, N, M2)

#define ARG_COUNT_HELPER(_1, _2, N, ...) N
#define ARG_COUNT_NO_ZERO(...) \
    ARG_COUNT_HELPER(__VA_ARGS__, 2, 1, 0)

#define CONCAT_HELPER(x, y) x##y
#define CONCAT(x, y) CONCAT_HELPER(x, y)

#define i_prop(X, Y, T, R, N, ...) \
    CONCAT(i_prop_, ARG_COUNT_NO_ZERO(__VA_ARGS__))(X, Y, T, R, N __VA_OPT__(,) __VA_ARGS__)

#define   i_vprop_interface_F(X, R, N)
#define   i_vprop_interface_F_EXTERN(X, R, N)
#define   i_vprop_interface_INST(X, R, N)
#define   i_vprop_interface_INST_EXTERN(X, R, N)
#define   i_vprop_interface_DECL(X, R, N)        i_prop_public_DECL(X, R, N)
#define   i_vprop_interface_DECL_EXTERN(X, R, N) i_prop_public_DECL(X, R, N)
#define   i_vprop_interface_GENERICS(X, R, N) \
    member_validate[validate_count++] = #N;
#define   i_vprop_interface_PROTO(X, R, N)  
#define   i_vprop_interface_METHOD(X, R, N)

#define   i_vprop_public_F(X, R, N) u8 N;
#define   i_vprop_public_F_EXTERN(X, R, N) u8 N;
#define   i_vprop_public_INST(X, R, N)         R* N;
#define   i_vprop_public_INST_EXTERN(X, R, N)  i_vprop_public_INST(X, R, N)
#define   i_vprop_public_DECL(X, R, N)          i_prop_public_DECL(X, R, N)
#define   i_vprop_public_DECL_EXTERN(X, R, N)   i_prop_public_DECL(X, R, N)
#define   i_vprop_public_GENERICS(X, R, N)
#define   i_vprop_public_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].name     = #N; \
    X##_i.type.members[X##_i.type.member_count].offset   = offsetof(struct _##X, N); \
    X##_i.type.members[X##_i.type.member_count].type     = (AType)&ARef_i.type; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_VPROP; \
    X##_i.type.member_count++;
#define   i_vprop_public_PROTO(X, R, N)  
#define   i_vprop_public_METHOD(X, R, N)

#define   i_vprop_required_F(X, R, N) u8 N;
#define   i_vprop_required_F_EXTERN(X, R, N) u8 N;
#define   i_vprop_required_INST(X, R, N)         i_vprop_public_INST(X, R, N)
#define   i_vprop_required_INST_EXTERN(X, R, N)  i_vprop_public_INST(X, R, N)
#define   i_vprop_required_DECL(X, R, N)         i_prop_public_DECL(X, R, N)
#define   i_vprop_required_DECL_EXTERN(X, R, N)  i_prop_public_DECL(X, R, N)
#define   i_vprop_required_GENERICS(X, R, N)
#define   i_vprop_required_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].required = true; \
    i_vprop_public_INIT(X, R, N)
#define   i_vprop_required_PROTO(X, R, N)  
#define   i_vprop_required_METHOD(X, R, N)

#define   i_vprop_intern_F(X, R, N) u8 N;
#define   i_vprop_intern_F_EXTERN(X, R, N)
#define   i_vprop_intern_INST(X, R, N)           R* N;
#define   i_vprop_intern_INST_EXTERN(X, R, N)
#define   i_vprop_intern_DECL(X, R, N)          i_prop_public_DECL(X, R, N)
#define   i_vprop_intern_DECL_EXTERN(X, R, N)   i_prop_public_DECL(X, R, N)
#define   i_vprop_intern_GENERICS(X, R, N)
#define   i_vprop_intern_INIT(X, R, N)
#define   i_vprop_intern_PROTO(X, R, N)  
#define   i_vprop_intern_METHOD(X, R, N)  
#define   i_vprop(X, Y, T, R, N) i_vprop_##T##_##Y(X, R, N)



#define i_attr_F(           X, ENUM, ID, VALUE, ...)
#define i_attr_F_EXTERN(    X, ENUM, ID, VALUE, ...)
#define i_attr_INST(        X, ENUM, ID, VALUE, ...)
#define i_attr_INST_EXTERN( X, ENUM, ID, VALUE, ...)
#define i_attr_DECL(        X, ENUM, ID, VALUE, ...)
#define i_attr_DECL_EXTERN( X, ENUM, ID, VALUE, ...)
#define i_attr_GENERICS(    X, ENUM, ID, VALUE, ...)
#define i_attr_INIT(        X, ENUM, ID, VALUE, ...) \
    X##_i.type.members[X##_i.type.member_count].name        = #ID; \
    X##_i.type.members[X##_i.type.member_count].id          = ENUM##_##ID; \
    X##_i.type.members[X##_i.type.member_count].value       = VALUE; \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&ENUM##_i.type; \
    X##_i.type.members[X##_i.type.member_count].args        = (meta_t) { emit_types(__VA_ARGS__) }; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_ATTR; \
    X##_i.type.member_count++;
#define i_attr_PROTO(       X, ENUM, ID, VALUE, ...)  
#define i_attr_METHOD(      X, ENUM, ID, VALUE, ...)  

#define i_attr(X, Y, ENUM, ID, VALUE, ...) i_attr_##Y(X, ENUM, ID, VALUE, __VA_ARGS__)

#define   i_array_interface_F(X, R, S, N)
#define   i_array_interface_F_EXTERN(X, R, S, N)
#define   i_array_interface_INST(X, R, S, N)
#define   i_array_interface_INST_EXTERN(X, R, S, N)
#define   i_array_interface_DECL(X, R, S, N)
#define   i_array_interface_DECL_EXTERN(X, R, S, N)
#define   i_array_interface_GENERICS(X, R, S, N)
#define   i_array_interface_INIT(X, R, S, N) \
    member_validate[validate_count++] = #N;
#define   i_array_interface_PROTO(X, R, S, N)  
#define   i_array_interface_METHOD(X, R, S, N)


#define   i_array_public_F(X, R, S, N) u8 N;
#define   i_array_public_F_EXTERN(X, R, S, N) u8 N;
#define   i_array_public_INST(X, R, S, N)         R N[S];  
#define   i_array_public_INST_EXTERN(X, R, S, N)  i_array_public_INST(X, R, S, N)
#define   i_array_public_DECL(X, R, S, N)           i_prop_public_DECL(X, R, N)
#define   i_array_public_DECL_EXTERN(X, R, S, N)
#define   i_array_public_GENERICS(X, R, S, N)
#define   i_array_public_INIT(X, R, S, N) \
    X##_i.type.members[X##_i.type.member_count].count    = S; \
    i_prop_public_INIT(X, R, N)

#define   i_array_public_PROTO(X, R, S, N)  
#define   i_array_public_METHOD(X, R, S, N)

#define   i_array_intern_F(X, R, S, N) u8 N;
#define   i_array_intern_F_EXTERN(X, R, S, N)
#define   i_array_intern_INST(X, R, S, N)         R N[S];
#define   i_array_intern_INST_EXTERN(X, R, S, N)  ARef _##N[S];
#define   i_array_intern_DECL(X, R, S, N)         i_prop_public_DECL(X, R, N)
#define   i_array_intern_DECL_EXTERN(X, R, S, N)
#define   i_array_intern_GENERICS(X, R, S, N)
#define   i_array_intern_INIT(X, R, S, N)
#define   i_array_intern_PROTO(X, R, S, N)  
#define   i_array_intern_METHOD(X, R, S, N)  
#define   i_array(X, Y, T, R, S, N) i_array_##T##_##Y(X, R, S, N)

#define   i_struct_ctr_INST(X, ARG)
#define   i_struct_ctr_INST_EXTERN(X, ARG)
#define   i_struct_ctr_DECL(X, ARG)
#define   i_struct_ctr_DECL_EXTERN(X, ARG)
#define   i_struct_ctr_GENERICS(X, ARG) ARG*: X##_i.type.with_##ARG,
#define   i_struct_ctr_INIT(X, ARG) \
    X##_i.type.with_##ARG = & X##_with_##ARG; \
    X##_i.type.members[X##_i.type.member_count].name        = stringify(with_##ARG); \
    X##_i.type.members[X##_i.type.member_count].args        = (meta_t) { }; \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&ARG##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(X##_f, with_##ARG); \
    X##_i.type.members[X##_i.type.member_count].ptr         = (void*)& X##_with_##ARG; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_CONSTRUCT; \
    X##_i.type.member_count++;  
#define   i_struct_ctr_PROTO(X, ARG)
#define   i_struct_ctr_METHOD(X, ARG)      X (*with_##ARG)(ARG*);
#define   i_struct_ctr(X, Y, ARG)          i_struct_ctr_##Y(X, ARG)

#define   i_struct_ctr_obj_INST(X, ARG)
#define   i_struct_ctr_obj_INST_EXTERN(X, ARG)
#define   i_struct_ctr_obj_DECL(X, ARG)
#define   i_struct_ctr_obj_DECL_EXTERN(X, ARG)
#define   i_struct_ctr_obj_GENERICS(X, ARG) ARG: X##_i.type.with_##ARG,
#define   i_struct_ctr_obj_INIT(X, ARG) \
    X##_i.type.with_##ARG = & X##_with_##ARG; \
    X##_i.type.members[X##_i.type.member_count].name        = stringify(with_##ARG); \
    X##_i.type.members[X##_i.type.member_count].args        = (meta_t) { }; \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&ARG##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(X##_f, with_##ARG); \
    X##_i.type.members[X##_i.type.member_count].ptr         = (void*)& X##_with_##ARG; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_CONSTRUCT; \
    X##_i.type.member_count++;  
#define   i_struct_ctr_obj_PROTO(X, ARG)
#define   i_struct_ctr_obj_METHOD(X, ARG)      X (*with_##ARG)(ARG);
#define   i_struct_ctr_obj(X, Y, ARG)          i_struct_ctr_obj_##Y(X, ARG)


#define   i_struct_array_INST(X, R, S, N)         R N[S];
#define   i_struct_array_INST_EXTERN(X, R, S, N)  i_struct_array_INST(X, R, S, N)
#define   i_struct_array_DECL(X, R, S, N)
#define   i_struct_array_DECL_EXTERN(X, R, S, N)
#define   i_struct_array_GENERICS(X, R, S, N)
#define   i_struct_array_INIT(X, R, S, N) \
    X##_i.type.members[X##_i.type.member_count].name     = #N; \
    X##_i.type.members[X##_i.type.member_count].offset   = offsetof(struct _##X, N); \
    X##_i.type.members[X##_i.type.member_count].type     = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].count    = S; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_PROP; \
    X##_i.type.member_count++;
#define   i_struct_array_PROTO(X, R, S, N)  
#define   i_struct_array_METHOD(X, R, S, N)        
#define   i_struct_array(X, Y, R, S, N) i_struct_array_##Y(X, R, S, N)

#define   i_struct_prop_INST(X, R, N)         R N;
#define   i_struct_prop_INST_EXTERN(X, R, N)  i_struct_prop_INST(X, R, N)
#define   i_struct_prop_DECL(X, R, N)
#define   i_struct_prop_DECL_EXTERN(X, R, N)
#define   i_struct_prop_GENERICS(X, R, N)
#define   i_struct_prop_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].name     = #N; \
    X##_i.type.members[X##_i.type.member_count].offset   = offsetof(struct _##X, N); \
    X##_i.type.members[X##_i.type.member_count].type     = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].count    = 1; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_PROP; \
    X##_i.type.member_count++;
#define   i_struct_prop_PROTO(X, R, N)  
#define   i_struct_prop_METHOD(X, R, N)      
#define   i_struct_prop(X, Y, R, N) i_struct_prop_##Y(X, R, N)

#define   i_struct_cast_INST(X, R)
#define   i_struct_cast_INST_EXTERN(X, R)
#define   i_struct_cast_DECL(X, R)
#define   i_struct_cast_DECL_EXTERN(X, R)
#define   i_struct_cast_GENERICS(X, R)
#define   i_struct_cast_INIT(X, R) \
    X##_i.type.cast_##R = & X##_cast_##R; \
    X##_i.type.members[X##_i.type.member_count].name    = stringify(cast_##R); \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(X) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = offsetof(X##_f, cast_##R); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_CAST; \
    X##_i.type.member_count++;  
#define   i_struct_cast_PROTO(X, R)
#define   i_struct_cast_METHOD(X, R)        R (*cast_##R)(X);         
#define   i_struct_cast(X, Y, R)                i_struct_cast_##Y(X, R)

#define   i_struct_method_INST(    X, R, N, ...)
#define   i_struct_method_INST_EXTERN(    X, R, N, ...)
#define   i_struct_method_DECL(X, R, N, ...)        R X##_##N(X* __VA_OPT__(,) __VA_ARGS__);
#define   i_struct_method_DECL_EXTERN(X, R, N, ...) R X##_##N(X* __VA_OPT__(,) __VA_ARGS__);
#define   i_struct_method_GENERICS(X, R, N, ...)
#define   i_struct_method_INIT(    X, R, N, ...) \
    X##_i.type . N = & X## _ ## N; \
    X##_i.type.members[X##_i.type.member_count].name    = #N; \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = offsetof(X##_f, N); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_IMETHOD; \
    X##_i.type.member_count++;
#define   i_struct_method_PROTO(X, R, N, ...)
#define   i_struct_method_METHOD(X, R, N, ...)      R (*N)(X* __VA_OPT__(,) __VA_ARGS__);
#define   i_struct_method(X, Y, R, N, ...)          i_struct_method_##Y(X, R, N, __VA_ARGS__)



#define   i_struct_static_INST(    X, R, N, ...)
#define   i_struct_static_INST_EXTERN(X, R, N, ...)
#define   i_struct_static_DECL(    X, R, N, ...)            R X##_##N(__VA_ARGS__);
#define   i_struct_static_DECL_EXTERN(    X, R, N, ...)     R X##_##N(__VA_ARGS__);
#define   i_struct_static_GENERICS(X, R, N, ...)
#define   i_struct_static_INIT(    X, R, N, ...) \
    X##_i.type . N = & X## _ ## N; \
    X##_i.type.members[X##_i.type.member_count].name    = #N; \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = offsetof(X##_f, N); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_IMETHOD; \
    X##_i.type.member_count++;
#define   i_struct_static_PROTO(X, R, N, ...)
#define   i_struct_static_METHOD(X, R, N, ...)      R (*N)(__VA_ARGS__);
#define   i_struct_static(X, Y, R, N, ...)          i_struct_static_##Y(X, R, N, __VA_ARGS__)









#define   i_inlay_public_F(X, R, N)            u8 N;
#define   i_inlay_public_F_EXTERN(X, R, N)     u8 N;
#define   i_inlay_public_INST(X, R, N)         struct _##R N;
#define   i_inlay_public_INST_EXTERN(X, R, N)  struct _##R N;
#define   i_inlay_public_DECL(X, R, N)
#define   i_inlay_public_DECL_EXTERN(X, R, N)
#define   i_inlay_public_GENERICS(X, R, N)
#define   i_inlay_public_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].name     = #N; \
    X##_i.type.members[X##_i.type.member_count].offset   = offsetof(struct _##X, N); \
    X##_i.type.members[X##_i.type.member_count].type     = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_INLAY; \
    X##_i.type.member_count++;
#define   i_inlay_public_PROTO(X, R, N)  
#define   i_inlay_public_METHOD(X, R, N)

#define   i_inlay_required_F(X, R, N) u8 N;
#define   i_inlay_required_F_EXTERN(X, R, N) u8 N;
#define   i_inlay_required_INST(X, R, N)         i_inlay_public_INST(X, R, N)
#define   i_inlay_required_INST_EXTERN(X, R, N)  i_inlay_public_INST_EXTERN(X, R, N)
#define   i_inlay_required_DECL(X, R, N)
#define   i_inlay_required_DECL_EXTERN(X, R, N)
#define   i_inlay_required_GENERICS(X, R, N)
#define   i_inlay_required_INIT(X, R, N) \
    X##_i.type.members[X##_i.type.member_count].required = true; \
    i_inlay_public_INIT(X, R, N)
#define   i_inlay_required_PROTO(X, R, N)  
#define   i_inlay_required_METHOD(X, R, N)

#define   i_inlay_intern_F(X, R, N)               u8 N;
#define   i_inlay_intern_F_EXTERN(X, R, N)        
#define   i_inlay_intern_INST(X, R, N)            struct _##R N;
#define   i_inlay_intern_INST_EXTERN(X, R, N)     i_inlay_intern_INST(X, R, N)
#define   i_inlay_intern_DECL(X, R, N)
#define   i_inlay_intern_DECL_EXTERN(X, R, N)
#define   i_inlay_intern_GENERICS(X, R, N)
#define   i_inlay_intern_INIT(X, R, N)
#define   i_inlay_intern_PROTO(X, R, N)  
#define   i_inlay_intern_METHOD(X, R, N)  
#define   i_inlay(X, Y, T, R, N) i_inlay_##T##_##Y(X, R, N)




#define   t_method_interface_F(X, R, N, ...)
#define   t_method_interface_F_EXTERN(X, R, N, ...)
#define   t_method_interface_INST(X, R, N, ...)
#define   t_method_interface_INST_EXTERN(X, R, N, ...)
#define   t_method_interface_DECL(X, R, N, ...)
#define   t_method_interface_DECL_EXTERN(X, R, N, ...)
#define   t_method_interface_GENERICS(X, R, N, ...)
#define   t_method_interface_INIT(X, R, N, ...) \
    member_validate[validate_count++] = #N;
#define   t_method_interface_PROTO(X, R, N, ...)
#define   t_method_interface_METHOD(X, R, N, ...)
#define   t_method_public_F(X, R, N, ...)
#define   t_method_public_F_EXTERN(X, R, N, ...)
#define   t_method_public_INST(X, R, N, ...)
#define   t_method_public_INST_EXTERN(X, R, N, ...)
#define   t_method_public_DECL(X, R, N, ...)        R N(__VA_ARGS__);
#define   t_method_public_DECL_EXTERN(X, R, N, ...) R N(__VA_ARGS__);
#define   t_method_public_GENERICS(X, R, N, ...)
#define   t_method_public_INIT(X, R, N, ...) \
    X##_i.type.members[X##_i.type.member_count].name    = #N; \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(__VA_ARGS__) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = 0; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_SMETHOD; \
    X##_i.type.members[X##_i.type.member_count].ptr     = &N; \
    X##_i.type.member_count++;   
#define   t_method_public_PROTO(X, R, N, ...)
#define   t_method_public_METHOD(X, R, N, ...)
#define   t_method_intern_F(X, R, N, ...)
#define   t_method_intern_F_EXTERN(X, R, N, ...)
#define   t_method_intern_INST(X, R, N, ...)    
#define   t_method_intern_INST_EXTERN(X, R, N, ...)
#define   t_method_intern_DECL(X, R, N, ...)        R N(__VA_ARGS__);
#define   t_method_intern_DECL_EXTERN(X, R, N, ...)
#define   t_method_intern_GENERICS(X, R, N, ...)
#define   t_method_intern_INIT(X, R, N, ...)      
#define   t_method_intern_PROTO(X, R, N, ...)
#define   t_method_intern_METHOD(X, R, N, ...)





#define   s_method_interface_F(X, R, N, ...)
#define   s_method_interface_F_EXTERN(X, R, N, ...)
#define   s_method_interface_INST(X, R, N, ...)
#define   s_method_interface_INST_EXTERN(X, R, N, ...)
#define   s_method_interface_DECL(X, R, N, ...)
#define   s_method_interface_DECL_EXTERN(X, R, N, ...)
#define   s_method_interface_GENERICS(X, R, N, ...)
#define   s_method_interface_INIT(X, R, N, ...) \
    member_validate[validate_count++] = #N;
#define   s_method_interface_PROTO(X, R, N, ...)
#define   s_method_interface_METHOD(X, R, N, ...)
#define   s_method_public_F(X, R, N, ...)
#define   s_method_public_F_EXTERN(X, R, N, ...)
#define   s_method_public_INST(X, R, N, ...)
#define   s_method_public_INST_EXTERN(X, R, N, ...)
#define   s_method_public_DECL(X, R, N, ...)        R X##_##N(__VA_ARGS__);
#define   s_method_public_DECL_EXTERN(X, R, N, ...) R X##_##N(__VA_ARGS__);
#define   s_method_public_GENERICS(X, R, N, ...)
#define   s_method_public_INIT(X, R, N, ...) \
    X##_i.type.members[X##_i.type.member_count].name    = #N; \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(__VA_ARGS__) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = 0; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_SMETHOD; \
    X##_i.type.members[X##_i.type.member_count].ptr     = &X##_##N; \
    X##_i.type.member_count++;   
#define   s_method_public_PROTO(X, R, N, ...)
#define   s_method_public_METHOD(X, R, N, ...)
#define   s_method_intern_F(X, R, N, ...)
#define   s_method_intern_F_EXTERN(X, R, N, ...)
#define   s_method_intern_INST(X, R, N, ...)    
#define   s_method_intern_INST_EXTERN(X, R, N, ...)
#define   s_method_intern_DECL(X, R, N, ...)        R X##_##N(__VA_ARGS__);
#define   s_method_intern_DECL_EXTERN(X, R, N, ...)
#define   s_method_intern_GENERICS(X, R, N, ...)
#define   s_method_intern_INIT(X, R, N, ...)      
#define   s_method_intern_PROTO(X, R, N, ...)
#define   s_method_intern_METHOD(X, R, N, ...)
#define   i_method_interface_F(X, R, N, ...)
#define   i_method_interface_F_EXTERN(X, R, N, ...)
#define   i_method_interface_INST(X, R, N, ...)
#define   i_method_interface_INST_EXTERN(X, R, N, ...)
#define   i_method_interface_DECL(X, R, N, ...)
#define   i_method_interface_DECL_EXTERN(X, R, N, ...)
#define   i_method_interface_GENERICS(X, R, N, ...)
#define   i_method_interface_INIT(X, R, N, ...) \
    member_validate[validate_count++] = #N;
#define   i_method_interface_PROTO(X, R, N, ...)
#define   i_method_interface_METHOD(X, R, N, ...)
#define   i_method_public_F(    X, R, N, ...)
#define   i_method_public_F_EXTERN(    X, R, N, ...)
#define   i_method_public_INST(    X, R, N, ...)
#define   i_method_public_INST_EXTERN(  X, R, N, ...)
#define   i_method_public_DECL(    X, R, N, ...)           R X##_##N(__VA_ARGS__);
#define   i_method_public_DECL_EXTERN(    X, R, N, ...)    
#define   i_method_public_GENERICS(X, R, N, ...)
#define   i_method_public_INIT(    X, R, N, ...) \
    X##_i.type . N = & X## _ ## N; \
    X##_i.type.members[X##_i.type.member_count].name    = #N; \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(__VA_ARGS__) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = offsetof(X##_f, N); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_IMETHOD; \
    X##_i.type.members[X##_i.type.member_count].ptr     = (void*)X##_i.type . N; \
    X##_i.type.member_count++;
#define   i_method_public_PROTO(X, R, N, ...)
#define   i_method_public_METHOD(X, R, N, ...)          R (*N)(__VA_ARGS__);
#define   i_method_intern_F(    X, R, N, ...)
#define   i_method_intern_F_EXTERN(    X, R, N, ...)
#define   i_method_intern_INST(    X, R, N, ...)
#define   i_method_intern_INST_EXTERN(  X, R, N, ...)
#define   i_method_intern_DECL(    X, R, N, ...)        R (*N)(__VA_ARGS__);
#define   i_method_intern_DECL_EXTERN(    X, R, N, ...)
#define   i_method_intern_GENERICS(X, R, N, ...)
#define   i_method_intern_INIT(    X, R, N, ...)    
#define   i_method_intern_PROTO(X, R, N, ...)
#define   i_method_intern_METHOD(X, R, N, ...)      

#define   i_final_public_F(    X, R, N, ...)
#define   i_final_public_F_EXTERN(    X, R, N, ...)
#define   i_final_public_INST(    X, R, N, ...)
#define   i_final_public_INST_EXTERN(  X, R, N, ...)
#define   i_final_public_DECL(    X, R, N, ...)           R N(__VA_ARGS__);
#define   i_final_public_DECL_EXTERN(    X, R, N, ...)    
#define   i_final_public_GENERICS(X, R, N, ...)
#define   i_final_public_INIT(    X, R, N, ...) \
    X##_i.type.members[X##_i.type.member_count].name    = #N; \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(__VA_ARGS__) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = 0; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_IFINAL; \
    X##_i.type.members[X##_i.type.member_count].ptr     = &N; \
    X##_i.type.member_count++;
#define   i_final_public_PROTO(X, R, N, ...)
#define   i_final_public_METHOD(X, R, N, ...)

/// avoiding conflict with parsing i_method uses
#define   i_method\
(X, Y, T, R, N, ...) i_method_##T##_##Y(X, R, N, X __VA_OPT__(,) __VA_ARGS__)

#define   i_final\
(X, Y, T, R, N, ...) i_final_##T##_##Y(X, R, N, X __VA_OPT__(,) __VA_ARGS__)

#define   s_method\
(X, Y, T, R, N, ...) s_method_##T##_##Y(X, R, N __VA_OPT__(,) __VA_ARGS__)

#define   t_method\
(X, Y, T, R, N, ...) t_method_##T##_##Y(X, R, N, AType __VA_OPT__(,) __VA_ARGS__)

#define   i_guard\
(X, Y, T, R, N, ...) i_method_##T##_##Y(X, R, N, X, ## __VA_ARGS__)

#define   i_operator_interface_F(X, R, N, ARG)
#define   i_operator_interface_F_EXTERN(X, R, N, ARG)
#define   i_operator_interface_INST(X, R, N, ARG)
#define   i_operator_interface_INST_EXTERN(X, R, N, ARG)
#define   i_operator_interface_DECL(X, R, N, ARG)
#define   i_operator_interface_DECL_EXTERN(X, R, N, ARG)
#define   i_operator_interface_GENERICS(X, R, N, ARG)
#define   i_operator_interface_INIT(X, R, N, ARG) \
    member_validate[validate_count] = #N;
#define   i_operator_interface_PROTO(X, R, N, ARG)
#define   i_operator_interface_METHOD(X, R, N, ARG)

#define   i_operator_public_F(X, R, N, ARG)
#define   i_operator_public_F_EXTERN(X, R, N, ARG)
#define   i_operator_public_INST(X, R, N, ARG)
#define   i_operator_public_INST_EXTERN(X, R, N, ARG)
#define   i_operator_public_DECL(X, R, N, ARG)
#define   i_operator_public_DECL_EXTERN(X, R, N, ARG)
#define   i_operator_public_GENERICS(X, R, N, ARG)
#define   i_operator_public_INIT(X, R, N, ARG) \
    X##_i.type  . operator_##N = & X## _operator_ ## N; \
    X##_i.type.members[X##_i.type.member_count].name    = stringify(operator_##N); \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(X, ARG) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = offsetof(X##_f, operator_##N); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_OPERATOR; \
    X##_i.type.members[X##_i.type.member_count].operator_type = OPType_ ## N; \
    X##_i.type.member_count++; 
#define   i_operator_public_PROTO(X, R, N, ARG)
#define   i_operator_public_METHOD(X, R, N, ARG)    R (*operator_ ## N)(X, ARG);

#define   i_operator_intern_F(X, R, N, ARG)
#define   i_operator_intern_F_EXTERN(X, R, N, ARG)
#define   i_operator_intern_INST(X, R, N, ARG)
#define   i_operator_intern_INST_EXTERN(X, R, N, ARG)
#define   i_operator_intern_DECL(X, R, N, ARG)
#define   i_operator_intern_DECL_EXTERN(X, R, N, ARG)
#define   i_operator_intern_GENERICS(X, R, N, ARG)
#define   i_operator_intern_INIT(X, R, N, ARG)      
#define   i_operator_intern_PROTO(X, R, N, ARG)
#define   i_operator_intern_METHOD(X, R, N, ARG)    
#define   i_operator(X, Y, T, R, N, ARG)            i_operator_##T##_##Y(X, R, N, ARG)

#define   i_cast_interface_F(X, R)
#define   i_cast_interface_F_EXTERN(X, R)
#define   i_cast_interface_INST(X, R)
#define   i_cast_interface_INST_EXTERN(X, R)
#define   i_cast_interface_DECL(X, R)
#define   i_cast_interface_DECL_EXTERN(X, R)
#define   i_cast_interface_GENERICS(X, R)
#define   i_cast_interface_INIT(X, R) \
    member_validate[validate_count++] = "cast_" #R;
#define   i_cast_interface_PROTO(X, R)
#define   i_cast_interface_METHOD(X, R)

#define   i_cast_public_F(X, R)
#define   i_cast_public_F_EXTERN(X, R)
#define   i_cast_public_INST(X, R)
#define   i_cast_public_INST_EXTERN(X, R)
#define   i_cast_public_DECL(X, R)
#define   i_cast_public_DECL_EXTERN(X, R)
#define   i_cast_public_GENERICS(X, R)
#define   i_cast_public_INIT(X, R) \
    X##_i.type.cast_##R = & X##_cast_##R; \
    X##_i.type.members[X##_i.type.member_count].name    = stringify(cast_##R); \
    X##_i.type.members[X##_i.type.member_count].args    = (meta_t) { emit_types(X) }; \
    X##_i.type.members[X##_i.type.member_count].type    = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset  = offsetof(X##_f, cast_##R); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_CAST; \
    X##_i.type.member_count++;  
#define   i_cast_public_PROTO(X, R)
#define   i_cast_public_METHOD(X, R)        R (*cast_##R)(X);

#define   i_cast_intern_F(X, R)
#define   i_cast_intern_F_EXTERN(X, R)
#define   i_cast_intern_INST(X, R)
#define   i_cast_intern_INST_EXTERN(X, R)
#define   i_cast_intern_DECL(X, R)
#define   i_cast_intern_DECL_EXTERN(X, R)
#define   i_cast_intern_GENERICS(X, R)
#define   i_cast_intern_INIT(X, R)          
#define   i_cast_intern_PROTO(X, R)
#define   i_cast_intern_METHOD(X, R)        
#define   i_cast(X, Y, T, R)                i_cast_##T##_##Y(X, R)

#define i_index_interface_F(X, R, ...)
#define i_index_interface_F_EXTERN(X, R, ...)
#define i_index_interface_INST(X, R, ...)
#define i_index_interface_INST_EXTERN(X, R, ...)
#define i_index_interface_DECL(X, R, ...)
#define i_index_interface_DECL_EXTERN(X, R, ...)
#define i_index_interface_GENERICS(X, R, ...)
#define i_index_interface_INIT(X, R, ...) \
    member_validate[validate_count++] = "index_" #R;
#define i_index_interface_PROTO(X, R, ...)
#define i_index_interface_METHOD(X, R, ...)

#define i_index_public_F(X, R, ...)
#define i_index_public_F_EXTERN(X, R, ...)
#define i_index_public_INST(X, R, ...)
#define i_index_public_INST_EXTERN(X, R, ...)
#define i_index_public_DECL(X, R, ...)
#define i_index_public_DECL_EXTERN(X, R, ...)
#define i_index_public_GENERICS(X, R, ...)
#define i_index_public_INIT(X, R, ...) \
    X##_i.type.emit_idx_symbol(index, __VA_ARGS__) = & emit_idx_symbol(X ## _index, __VA_ARGS__); \
    X##_i.type.members[X##_i.type.member_count].name        = stringify(emit_idx_symbol(index, __VA_ARGS__)); \
    X##_i.type.members[X##_i.type.member_count].args        = (meta_t) { emit_types(X, __VA_ARGS__) }; \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(X##_f, emit_idx_symbol(index, __VA_ARGS__)); \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_INDEX; \
    X##_i.type.member_count++;  
#define i_index_public_PROTO(X, R, ...)
#define i_index_public_METHOD(X, R, ...)                R (*emit_idx_symbol(index,__VA_ARGS__))(X, ##__VA_ARGS__);

#define i_index_intern_F(X, R, ...)
#define i_index_intern_F_EXTERN(X, R, ...)
#define i_index_intern_INST(X, R, ...)
#define i_index_intern_INST_EXTERN(X, R, ...)
#define i_index_intern_DECL(X, R, ...)
#define i_index_intern_DECL_EXTERN(X, R, ...)
#define i_index_intern_GENERICS(X, R, ...)
#define i_index_intern_INIT(X, R, ...)
#define i_index_intern_PROTO(X, R, ...)
#define i_index_intern_METHOD(X, R, ...)                R (*emit_idx_symbol(index, __VA_ARGS__))(X, ##__VA_ARGS__);
#define i_index(X, Y, T, R, ...)                        i_index_##T##_##Y(X, R, ##__VA_ARGS__)

#define i_vargs_public_F(X, R, N, ...)
#define i_vargs_public_F_EXTERN(X, R, N, ...)
#define i_vargs_public_INST(X, R, N, ...)
#define i_vargs_public_INST_EXTERN(X, R, N, ...)
#define i_vargs_public_DECL(X, R, N, ...)               R X##_##N(X  __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs_public_DECL_EXTERN(X, R, N, ...)        R X##_##N(X, __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs_public_GENERICS(X, R, N, ...)
#define i_vargs_public_INIT(X, R, N, ...)               i_method_public_INIT(X, R, N, __VA_ARGS__)  
#define i_vargs_public_PROTO(X, R, N, ...)
#define i_vargs_public_METHOD(X, R, N, ...)             R (*N)(__VA_ARGS__, ...);

#define i_vargs_intern_F(X, R, N, ...)
#define i_vargs_intern_F_EXTERN(X, R, N, ...)
#define i_vargs_intern_INST(X, R, N, ...)
#define i_vargs_intern_INST_EXTERN(X, R, N, ...)
#define i_vargs_intern_DECL(X, R, N, ...)               static R X##_##N(X __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs_intern_DECL_EXTERN(X, R, N, ...)
#define i_vargs_intern_GENERICS(X, R, N, ...)
#define i_vargs_intern_INIT(X, R, N, ...)               i_method_intern_INIT(X, R, N, __VA_ARGS__)  
#define i_vargs_intern_PROTO(X, R, N, ...)
#define i_vargs_intern_METHOD(X, R, N, ...)             R (*N)(X __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs(X, Y, T, R, N, ...)                     i_vargs_##T##_##Y(X, R, N, __VA_ARGS__)

#define s_vargs_public_F(X, R, N, ...)
#define s_vargs_public_F_EXTERN(X, R, N, ...)
#define s_vargs_public_INST(X, R, N, ...)
#define s_vargs_public_INST_EXTERN(X, R, N, ...)
#define s_vargs_public_DECL(X, R, N, ...)               R N(__VA_ARGS__, ...);
#define s_vargs_public_DECL_EXTERN(X, R, N, ...)        R N(__VA_ARGS__, ...);
#define s_vargs_public_GENERICS(X, R, N, ...)
#define s_vargs_public_INIT(X, R, N, ...)               t_method_public_INIT(X, R, N, ##__VA_ARGS__)
#define s_vargs_public_PROTO(X, R, N, ...)
#define s_vargs_public_METHOD(X, R, N, ...)

#define s_vargs_intern_F(X, R, N, ...)
#define s_vargs_intern_F_EXTERN(X, R, N, ...)
#define s_vargs_intern_INST(X, R, N, ...)
#define s_vargs_intern_INST_EXTERN(X, R, N, ...)
#define s_vargs_intern_DECL(X, R, N, ...)               R N(__VA_ARGS__, ...);
#define s_vargs_intern_DECL_EXTERN(X, R, N, ...)
#define s_vargs_intern_GENERICS(X, R, N, ...)
#define s_vargs_intern_INIT(X, R, N, ...)               t_method_intern_INIT(X, R, N, ##__VA_ARGS__)
#define s_vargs_intern_PROTO(X, R, N, ...)
#define s_vargs_intern_METHOD(X, R, N, ...)             
#define s_vargs(X, Y, T, R, N, ...)                     s_vargs_##T##_##Y(X, R, N, ##__VA_ARGS__)
#define t_vargs(X, Y, T, R, N, ...)                     s_vargs_##T##_##Y(X, R, N, AType, __VA_ARGS__)

#define i_override_method_F(X, N)
#define i_override_method_F_EXTERN(X, N)
#define i_override_method_INST(X, N)
#define i_override_method_INST_EXTERN(X, N)
#define i_override_method_DECL(X, N)
#define i_override_method_DECL_EXTERN(X, N)
#define i_override_method_GENERICS(X, N)
#define i_override_method_INIT(X, N) \
    X##_i.type . N = (__typeof__(X##_i.type . N))& X## _ ## N;

#define i_override_method_PROTO(X, N)
#define i_override_method_METHOD(X, N)

#define i_override_ctr_F(X, R)
#define i_override_ctr_F_EXTERN(X, R)
#define i_override_ctr_INST(X, R)
#define i_override_ctr_INST_EXTERN(X, N)
#define i_override_ctr_DECL(X, R)
#define i_override_ctr_GENERICS(X, N)                   N: X##_i.type.with_##N,
#define i_override_ctr_INIT(X, R) \
    X##_i.type . with_##R = & X##_with_##R; \
    X##_i.type.members[X##_i.type.member_count].name        = stringify(with_##R); \
    X##_i.type.members[X##_i.type.member_count].args        = (meta_t) { emit_types(R) }; \
    X##_i.type.members[X##_i.type.member_count].type        = (AType)&R##_i.type; \
    X##_i.type.members[X##_i.type.member_count].offset      = offsetof(X##_f, with_##R); \
    X##_i.type.members[X##_i.type.member_count].ptr         = (void*)& X##_with_##R; \
    X##_i.type.members[X##_i.type.member_count].member_type = A_FLAG_CONSTRUCT; \
    X##_i.type.member_count++; 
#define i_override_ctr_PROTO(X, R)
#define i_override_ctr_METHOD(X, R)

#define i_override_cast_F(X, R)
#define i_override_cast_F_EXTERN(X, R)
#define i_override_cast_INST(X, R)
#define i_override_cast_INST_EXTERN(X, R)
#define i_override_cast_DECL(X, R)
#define i_override_cast_GENERICS(X, N)
#define i_override_cast_INIT(X, R)                      X##_i.type . cast_##R = & X##_cast_##R;
#define i_override_cast_PROTO(X, R)
#define i_override_cast_METHOD(X, R)

#define i_override_idx_F(X, R)
#define i_override_idx_F_EXTERN(X, R)
#define i_override_idx_INST(X, R)
#define i_override_idx_INST_EXTERN(X, R)
#define i_override_idx_DECL(X, R)
#define i_override_idx_GENERICS(X, R)
#define i_override_idx_INIT(X, R)                    X##_i.type . idx_##R = & X##_idx_##R;
#define i_override_idx_PROTO(X, R)
#define i_override_idx_METHOD(X, R)

#define i_override_1(X, Y, OT, N, ...) \
    i_override_##OT##_##Y(X, N)

#define i_override_2(X, Y, OT, N, N2) \
    i_override_##OT##_##Y(X, N) \
    i_override_##OT##_##Y(X, N2)

#define i_override_3(X, Y, OT, N, N2, N3) \
    i_override_##OT##_##Y(X, N)  \
    i_override_##OT##_##Y(X, N2) \
    i_override_##OT##_##Y(X, N3)

#define i_override_4(X, Y, OT, N, N2, N3) \
    i_override_##OT##_##Y(X, N)  \
    i_override_##OT##_##Y(X, N2) \
    i_override_##OT##_##Y(X, N3) \
    i_override_##OT##_##Y(X, N4)

#define i_override_5(X, Y, OT, N, N2, N3) \
    i_override_##OT##_##Y(X, N)  \
    i_override_##OT##_##Y(X, N2) \
    i_override_##OT##_##Y(X, N3) \
    i_override_##OT##_##Y(X, N4) \
    i_override_##OT##_##Y(X, N5)

#define i_override_6(X, Y, OT, N, N2, N3) \
    i_override_##OT##_##Y(X, N)  \
    i_override_##OT##_##Y(X, N2) \
    i_override_##OT##_##Y(X, N3) \
    i_override_##OT##_##Y(X, N4) \
    i_override_##OT##_##Y(X, N5) \
    i_override_##OT##_##Y(X, N6)

#define i_override_7(X, Y, OT, N, N2, N3) \
    i_override_##OT##_##Y(X, N)  \
    i_override_##OT##_##Y(X, N2) \
    i_override_##OT##_##Y(X, N3) \
    i_override_##OT##_##Y(X, N4) \
    i_override_##OT##_##Y(X, N5) \
    i_override_##OT##_##Y(X, N6) \
    i_override_##OT##_##Y(X, N7)

#define i_override_8(X, Y, OT, N, N2, N3) \
    i_override_##OT##_##Y(X, N)  \
    i_override_##OT##_##Y(X, N2) \
    i_override_##OT##_##Y(X, N3) \
    i_override_##OT##_##Y(X, N4) \
    i_override_##OT##_##Y(X, N5) \
    i_override_##OT##_##Y(X, N6) \
    i_override_##OT##_##Y(X, N7) \
    i_override_##OT##_##Y(X, N8)

#define ARG_COUNT_HELPER(_1, _2, N, ...) N
#define ARG_COUNT_NO_ZERO(...) \
    ARG_COUNT_HELPER(__VA_ARGS__, 2, 1, 0)

#define CONCAT_HELPER(x, y) x##y
#define CONCAT(x, y) CONCAT_HELPER(x, y)

#define i_override(X, Y, OT, N, ...) \
    CONCAT(i_override_, ARG_COUNT_NO_ZERO(__VA_ARGS__))(X, Y, OT, N __VA_OPT__(,) __VA_ARGS__)


#define M(X, Y, VISIBILITY, NAME, ...) \
    i_ ## NAME ## _ ## VISIBILITY ## _ ## Y (X, __VA_ARGS__)


#define backwards(container, E, e) \
    if (container && len(((array)container))) for (E e = peek(((array)container), len(((array)container)) - 1), e0 = 0; e0 == 0; e0++) \
        for (num __i = len(((array)container)); __i >= 0; __i--, e = __i >= 0 ? (E)peek(((array)container), __i) : (E)null) \

/// we can iterate through a collection with this strange code
#define each(container, E, e) \
    if (container && len(((array)container))) for (E e = peek(((array)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = len(((array)container)); __i < __len; __i++, e = peek(((array)container), __i)) \

#define each_(container, E, e) \
    if (container && len(((array)container))) for (E e = (E)peek(((array)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = len(((array)container)); __i < __len; __i++, e = (E)peek(((array)container), __i)) \

#define values(container, E, e) \
    if (container && len(((array)container))) for (E e = *(E*)peek(((array)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = len(((array)container)); __i < __len; __i++, e = *(E*)peek(((array)container), __i)) \

/// we can go through a map
#define pairs(MM, EE) \
    for (item EE = (MM && MM->fifo) ? MM->fifo->first : (item)null; EE; EE = EE->next)


#define         form(T, t, ...)   (T)formatter(typeid(T), null,   (A)false, (symbol)t, ## __VA_ARGS__)
#define            f(T, t, ...)   (T)formatter(typeid(T), null,   (A)false, (symbol)t, ## __VA_ARGS__)
#define         exec(t, ...)      command_exec(((string)formatter((AType)null, null, (A)false, (symbol)t, ## __VA_ARGS__)))
#define         vexec(n, t, ...)     verify(exec(t __VA_OPT__(,) __VA_ARGS__) == 0, "shell command failed: %s", n);

#define        A_log(sL, t, ...)   formatter((AType)null,      stdout, string(sL),  t, ## __VA_ARGS__)
#define        print(t, ...)   ({\
    static string _topic = null; \
    if (!_topic) _topic = (string)hold((A)string(__func__)); \
    formatter((AType)null, stdout, (A)_topic, t, ## __VA_ARGS__); \
})

#define          put(t,    ...)   formatter((AType)null,      stdout, (A)false, (symbol)t, ## __VA_ARGS__)
//#define        print(L, t,    ...) formatter((AType)null,      stdout, (A)true,  (symbol)t, ## __VA_ARGS__)
#define        error(t, ...)      formatter((AType)null,      stderr, (A)true,  (symbol)t, ## __VA_ARGS__)


#define print(t, ...)   ({\
    static string _topic = null; \
    if (!_topic) _topic = (string)hold((A)string(__func__)); \
    formatter((AType)null, stdout, (A)_topic, t, ## __VA_ARGS__); \
})

#define fault(t, ...) do {\
    static string _topic = null; \
    if (!_topic) _topic = (string)hold((A)string(__func__)); \
     formatter((AType)null, stderr, (A)_topic,  (symbol)t, ## __VA_ARGS__); \
     exit(1); \
    } while(0)


#define  file_exists(t, ...)     (A_exists(formatter((AType)null, null, (A)false, (symbol)t, ## __VA_ARGS__)) == Exists_file)
#define   dir_exists(t, ...)     (A_exists(formatter((AType)null, null, (A)false, (symbol)t, ## __VA_ARGS__)) == Exists_dir)
#ifndef NDEBUG
#define       assert(a, t, ...) do { if (!(a)) { formatter((AType)null, stderr, (A)true,  t, ## __VA_ARGS__); exit(1); } } while(0)
#else
#define       assert(a, t, ...) do { } while(0)
#endif
#define       verify(a, t, ...) \
    ({ \
        if (!(a)) { \
            formatter((AType)null, stderr, (A)true,  (symbol)t, ## __VA_ARGS__); \
            if (level_err >= fault_level) { \
                exit(1); \
            } \
            false; \
        } else { \
            true; \
        } \
        true; \
    })

#undef min
#undef max

#define sqr(x) ({ \
    __typeof__(x) a = x; \
    a*a \
})\

#define min(A, B) ({ \
    __typeof__(A) a = A; \
    __typeof__(B) b = B; \
    __typeof__(A) r = a < b ? a : b; \
    r; \
})

#define max(A, B) ({ \
    __typeof__(A) a = A; \
    __typeof__(B) b = B; \
    __typeof__(A) r = a > b ? a : b; \
    r; \
})

/// possible to iterate safely through primitives
#define primitives(arr, E, e) \
    if (len(arr)) for (E e = *(E*)peek(arr, 0), e0 = 0; e0 == 0; e0++) \
        for (num i = 0, __len = len(arr); i < __len; i++, e = *(E*)peek(arr, i)) \

#define head(o) header((A)o)
#define A_struct(T) alloc(typeid(u8), sizeof(T))
#define     e_str(E,I) estring(typeid(E), I)
#define     e_val(E,S) evalue (typeid(E), S)

#endif