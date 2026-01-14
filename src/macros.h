#ifndef _macros_h
#define _macros_h


#define TC_a(V) ((Au)(V))
#define _ARG_COUNT_IMPL_a(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,N,...) N
#define _ARG_COUNT_I_a(...) _ARG_COUNT_IMPL_a(__VA_ARGS__,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define _ARG_COUNT_a(...)   _ARG_COUNT_I_a("Au object model", ## __VA_ARGS__)
#define _COMBINE_a_(A,B) A##B
#define _COMBINE_a(A,B)  _COMBINE_a_(A,B)
#define _N_ARGS_a_0(TYPE)
#define _N_ARGS_a_1(TYPE, a)                 TC_a(a)
#define _N_ARGS_a_2(TYPE, a,b)               _N_ARGS_a_1(TYPE,a), TC_a(b)
#define _N_ARGS_a_3(TYPE, a,b,c)             _N_ARGS_a_2(TYPE,a,b), TC_a(c)
#define _N_ARGS_a_4(TYPE, a,b,c,d)           _N_ARGS_a_3(TYPE,a,b,c), TC_a(d)
#define _N_ARGS_a_5(TYPE, a,b,c,d,e)         _N_ARGS_a_4(TYPE,a,b,c,d), TC_a(e)
#define _N_ARGS_a_6(TYPE, a,b,c,d,e,f)       _N_ARGS_a_5(TYPE,a,b,c,d,e), TC_a(f)
#define _N_ARGS_a_7(TYPE, a,b,c,d,e,f,g)     _N_ARGS_a_6(TYPE,a,b,c,d,e,f), TC_a(g)
#define _N_ARGS_a_8(TYPE, a,b,c,d,e,f,g,h)   _N_ARGS_a_7(TYPE,a,b,c,d,e,f,g), TC_a(h)
#define _N_ARGS_a_9(TYPE, a,b,c,d,e,f,g,h,i) _N_ARGS_a_8(TYPE,a,b,c,d,e,f,g,h), TC_a(i)
#define _N_ARGS_a_10(TYPE, a,b,c,d,e,f,g,h,i,j) \
    _N_ARGS_a_9(TYPE,a,b,c,d,e,f,g,h,i), TC_a(j)
#define _N_ARGS_HELPER2_a(TYPE,N,...) _COMBINE_a(_N_ARGS_a_,N)(TYPE, ## __VA_ARGS__)
#define _N_ARGS_a(TYPE,...)          _N_ARGS_HELPER2_a(TYPE, _ARG_COUNT_a(__VA_ARGS__), ## __VA_ARGS__)
#define a(...) array_of(_N_ARGS_a(a, ## __VA_ARGS__), null)


#define TC_m(K, V) ((symbol)(K)), ((Au)(V))
#define _ARG_COUNT_IMPL_m(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,N,...) N
#define _ARG_COUNT_I_m(...) _ARG_COUNT_IMPL_m(__VA_ARGS__,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define _ARG_COUNT_m(...)   _ARG_COUNT_I_m("Au object model", ## __VA_ARGS__)
#define _COMBINE_m_(A,B) A##B
#define _COMBINE_m(A,B)  _COMBINE_m_(A,B)
#define _N_ARGS_m_0(TYPE)
#define _N_ARGS_m_2(TYPE, a,b)                   TC_m(a,b)
#define _N_ARGS_m_4(TYPE, a,b, c,d)              _N_ARGS_m_2(TYPE, a,b), TC_m(c,d)
#define _N_ARGS_m_6(TYPE, a,b, c,d, e,f)         _N_ARGS_m_4(TYPE, a,b, c,d), TC_m(e,f)
#define _N_ARGS_m_8(TYPE, a,b, c,d, e,f, g,h)    _N_ARGS_m_6(TYPE, a,b, c,d, e,f), TC_m(g,h)
#define _N_ARGS_m_10(TYPE, a,b, c,d, e,f, g,h, i,j) \
    _N_ARGS_m_8(TYPE, a,b, c,d, e,f, g,h), TC_m(i,j)
#define _N_ARGS_m_12(TYPE, a,b, c,d, e,f, g,h, i,j, k,l) \
    _N_ARGS_m_10(TYPE, a,b, c,d, e,f, g,h, i,j), TC_m(k,l)
#define _N_ARGS_m_14(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n) \
    _N_ARGS_m_12(TYPE, a,b, c,d, e,f, g,h, i,j, k,l), TC_m(m,n)
#define _N_ARGS_m_16(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p) \
    _N_ARGS_m_14(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n), TC_m(o,p)
#define _N_ARGS_m_18(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r) \
    _N_ARGS_m_16(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p), TC_m(q,r)
#define _N_ARGS_m_20(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r, s,t) \
    _N_ARGS_m_18(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r), TC_m(s,t)
#define _N_ARGS_m_22(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r, s,t, u,v) \
    _N_ARGS_m_20(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r, s,t), TC_m(u,v)

#define _N_ARGS_HELPER2_m(TYPE,N,...) _COMBINE_m(_N_ARGS_m_,N)(TYPE, ## __VA_ARGS__)
#define _N_ARGS_m(TYPE,...) _N_ARGS_HELPER2_m(TYPE, _ARG_COUNT_m(__VA_ARGS__), ## __VA_ARGS__)

#define m(...) map_of(_N_ARGS_m(m, ## __VA_ARGS__), null)



//#define a(...) array_of(__VA_ARGS__ __VA_OPT__(,) null)
//#define m(...)   map_of(__VA_ARGS__ __VA_OPT__(,) null)

#define   enum_value_DECL(E, T, N, VAL)             static const E E##_##N = VAL;
#define   enum_value_COUNT(E, T, N, VAL)            1,
#define   enum_value_METHOD(E, T, N, VAL)
#define   enum_value_IMPL(E, T, N, VAL) { \
        static T static_##N = VAL; \
        Au_t m = def_enum_value((Au_t)&E##_i.type, #N, (void*)&static_##N); \
        m->access_type = interface_public; \
        m->offset   = (i32)(E##_##N);\
        m->type     = (Au_t)&T ## _i.type; \
        m->value    = (object)&static_##N;\
        m->member_type = AU_MEMBER_ENUMV; \
    }

#define   enum_value(E,T,Y, N, VAL)                enum_value_##Y(E, T, N, VAL)

#define   enum_method_DECL(E, T, R, N, ...)
#define   enum_method_COUNT(E, T, R, N, ...)
#define   enum_method_IMPL(E, T, R, N, ...) { \
    Au_t m = def((Au_t)&E##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_SMETHOD); \
    m->access_type = interface_public; \
    E##_i.type . ft.N = & E## _ ## N; \
    set_args_array(m, emit_types(__VA_ARGS__)); \
    m->type    = (Au_t)&R##_i.type; \
    m->offset  = offsetof(E##_f, N); \
    m->value   = (object)& E##_##N; \
    m->index   = offsetof(__typeof__(E##_i.type.ft), N) / sizeof(void*);

#define   enum_method_METHOD(E, T, R, N, ...)    R (*N)(E value __VA_OPT__(,) __VA_ARGS__);
#define   enum_method(E,T,Y,R,N,...)            enum_method_##Y(E,T, R,N __VA_OPT__(,) __VA_ARGS__)



#define   enum_value_v_DECL(E, T, N, VAL)             static const E E##_##N = VAL;
#define   enum_value_v_COUNT(E, T, N, VAL)            1,
#define   enum_value_v_METHOD(E, T, N, VAL)
#define   enum_value_v_IMPL(E, T, N, VAL) { \
    Au_t m = def_enum_value((Au_t)&E##_i.type, #N, &static_##N); \
    m->access_type = interface_public; \
    static T static_##N = VAL; \
    m->offset   = (i64)E##_##N;\
    m->type     = (Au_t)&T ## _i.type; \
    m->value    = &static_##N;\
}

#define   enum_value_vargs_DECL(E, T, N, VAL,...)             static const E E##_##N = VAL;
#define   enum_value_vargs_COUNT(E, T, N, VAL,...)            1,
#define   enum_value_vargs_METHOD(E, T, N, VAL,...)
#define   enum_value_vargs_IMPL(E, T, N, VAL,...) { \
    static T static_##N = VAL; \
    Au_t m = def_enum_value((Au_t)&E##_i.type, #N, &static_##N); \
    m->access_type = interface_public; \
    m->offset   = (i64)E##_##N;\
    m->type     = (Au_t)&T ## _i.type; \
    m->member_type = AU_MEMBER_ENUMV; \
    m->value    = &static_##N;\
    set_meta_array(m, emit_types(__VA_ARGS__)); \
}

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
#define _ARG_COUNT(...)   _ARG_COUNT_I("Au", ## __VA_ARGS__)
#define _COMBINE_(A, B)   A##B
#define _COMBINE(A, B)    _COMBINE_(A, B)
#define _N_ARGS_0( TYPE)
#define _N_ARGS_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, Au) const void *: (void)0)(instance, a)
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
    ({ TYPE instance = _Generic((a), TYPE##_schema(TYPE, GENERICS, Au) const void *: (void)0)(a); instance; }) 

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
        TYPE instance = (TYPE)alloc(typeid(TYPE), 1, (Au_t*)null); \
        _N_ARGS(TYPE, ## __VA_ARGS__); \
        Au_initialize((Au)instance); \
        instance; \
    })
#define new2(TYPE, ...) \
    ({ \
        TYPE instance = (TYPE)alloc(typeid(TYPE), 1, (Au_t*)null); \
        TYPE##_N_ARGS(TYPE, ## __VA_ARGS__); \
        Au_initialize((Au)instance); \
        instance; \
    })

/// with construct we give it a dynamic type, symbols and Au-values
#define construct(type, ...) \
    ({ \
        T instance = (T)alloc(type, 1, (Au_t*)null); \
        _N_ARGS(instance, ## __VA_ARGS__); \
        Au_initialize((Au)instance); \
        instance; \
    })

#define new0(T, ...) \
    ({ \
        T instance = (T)alloc(typeid(T), 1, (Au_t*)null); \
        _N_ARGS(instance, ## __VA_ARGS__); \
        Au_initialize((Au)instance); \
        instance; \
    })

#define allocate(T, ...) \
    ({ \
        T instance = (T)alloc(typeid(T), 1, (Au_t*)null); \
        _N_ARGS(instance, ## __VA_ARGS__); \
        instance; \
    })

#define valloc(T, N)                ((Au)alloc(typeid(T), N, (Au_t*)null))
#define ftable(TYPE, INSTANCE)      ((TYPE##_f*)((Au)INSTANCE)[-1].type)
#define isa(INSTANCE)               ((Au_t)(INSTANCE ? (struct _Au_f*)((struct _Au*)INSTANCE - 1)->type : (struct _Au_f*)0))
// see: javascript; returns null if its not an instance-of; faults if you give it a null
//#define instanceof(left, type)      Au_instanceof(left, type)
#define ftableI(I)                  ((__typeof__((I)->__f[0])) ((Au)(I))[-1].type)
#define fcall(I,M,...)              ({ __typeof__(I) _i_ = I; ftableI(_i_)->ft.M(_i_, ## __VA_ARGS__); })
#define mcall(I,M,...)              ({ __typeof__(I) _i_ = I; (_i_) ? ftableI(_i_)->ft.M(_i_, ## __VA_ARGS__) : 0; })
#define cstring(I)                  cast(cstr, I)
#define val(T,V)                    primitive(typeid(T), (&(T){V}))
#define idx_1(I,T1,V1)              fcall(I, index ##_## T1, V1)
#define idx_2(I,T1,T2,V1,V2)        fcall(I, index ##_## T1 ##_## T2, V1, V2)
#define idx(I,V1)                   fcall(I, index ##_## num, V1)
#define meta_t(I,IDX)               isa(I) -> meta.origin[IDX]
#define ctr(T,WITH,...)             Au_initialize(T##_i.type.ft.with_##WITH(alloc(typeid(T), 1, (Au_t*)null), ## __VA_ARGS__))
#define ctr1(T,WITH,...)            Au_initialize(T##_i.type.ft.with_##WITH(alloc(typeid(T), 1, (Au_t*)null), ## __VA_ARGS__))
#define alloc_ctr(T,WITH,...)       Au_initialize(T##_i.type.ft.with_##WITH(alloc(typeid(T), 1, (Au_t*)null), ## __VA_ARGS__))
#define str(CSTR)                   string_i.type.with_symbol((string)alloc((Au_t)&string_i.type, 1, (Au_t*)null), (symbol)(CSTR))
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
#define EXPAND_ARGS_1(a)                               1, (Au_t)&a##_i.type
#define EXPAND_ARGS_2(a, b)                            2, (Au_t)&a##_i.type, (Au_t)&b##_i.type
#define EXPAND_ARGS_3(a, b, c)                         3, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type
#define EXPAND_ARGS_4(a, b, c, d)                      4, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type
#define EXPAND_ARGS_5(a, b, c, d, e)                   5, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, (Au_t)&e##_i.type
#define EXPAND_ARGS_6(a, b, c, d, e, f)                6, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, (Au_t)&e##_i.type, (Au_t)&f##_i.type
#define EXPAND_ARGS_7(a, b, c, d, e, f, g)             7, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type
#define EXPAND_ARGS_8(a, b, c, d, e, f, g, h)          8, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type
#define EXPAND_ARGS_9(a, b, c, d, e, f, g, h, ii)      9, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type
#define EXPAND_ARGS_10(a, b, c, d, e, f, g, h, ii, j) 10, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, (Au_t)&j##_i.type

#define EXPAND_ARGS_11(a, b, c, d, e, f, g, h, ii, j, k) \
    11, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, \
    (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, \
    (Au_t)&j##_i.type, (Au_t)&k##_i.type

#define EXPAND_ARGS_12(a, b, c, d, e, f, g, h, ii, j, k, l) \
    12, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, \
    (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, \
    (Au_t)&j##_i.type, (Au_t)&k##_i.type, (Au_t)&l##_i.type

#define EXPAND_ARGS_13(a, b, c, d, e, f, g, h, ii, j, k, l, m) \
    13, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, \
    (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, \
    (Au_t)&j##_i.type, (Au_t)&k##_i.type, (Au_t)&l##_i.type, (Au_t)&m##_i.type

#define EXPAND_ARGS_14(a, b, c, d, e, f, g, h, ii, j, k, l, m, n) \
    14, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, \
    (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, \
    (Au_t)&j##_i.type, (Au_t)&k##_i.type, (Au_t)&l##_i.type, (Au_t)&m##_i.type, (Au_t)&n##_i.type

#define EXPAND_ARGS_15(a, b, c, d, e, f, g, h, ii, j, k, l, m, n, o) \
    15, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, \
    (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, \
    (Au_t)&j##_i.type, (Au_t)&k##_i.type, (Au_t)&l##_i.type, (Au_t)&m##_i.type, (Au_t)&n##_i.type, (Au_t)&o##_i.type

#define EXPAND_ARGS_16(a, b, c, d, e, f, g, h, ii, j, k, l, m, n, o, p) \
    16, (Au_t)&a##_i.type, (Au_t)&b##_i.type, (Au_t)&c##_i.type, (Au_t)&d##_i.type, \
    (Au_t)&e##_i.type, (Au_t)&f##_i.type, (Au_t)&g##_i.type, (Au_t)&h##_i.type, (Au_t)&ii##_i.type, \
    (Au_t)&j##_i.type, (Au_t)&k##_i.type, (Au_t)&l##_i.type, (Au_t)&m##_i.type, (Au_t)&n##_i.type, (Au_t)&o##_i.type, (Au_t)&p##_i.type

    //#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define COUNT_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N
#define COUNT_ARGS(...)             COUNT_ARGS_IMPL(dummy, ## __VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

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
#define   i_ctr_interface_ISIZE(X, ARG)  
#define   i_ctr_interface_INST_U(X, ARG)
#define   i_ctr_interface_INST_L(X, ARG)
#define   i_ctr_interface_INST_U_EXTERN(X, ARG)
#define   i_ctr_interface_INST_L_EXTERN(X, ARG)
#define   i_ctr_interface_DECL(X, ARG)
#define   i_ctr_interface_DECL_EXTERN(X, ARG)
#define   i_ctr_interface_MEMBER_INDEX(X, ARG)
#define   i_ctr_interface_GENERICS(X, ARG)
#define   i_ctr_interface_INIT(X, ARG)
#define   i_ctr_interface_PROTO(X, ARG)
#define   i_ctr_interface_METHOD(X, ARG)

#define   i_ctr_public_F(X, ARG)
#define   i_ctr_public_F_EXTERN(X, ARG)
#define   i_ctr_public_ISIZE(X, ARG)   
#define   i_ctr_public_ISIZE_EXTERN(X, ARG)   
#define   i_ctr_public_INST_U(X, ARG)
#define   i_ctr_public_INST_L(X, ARG)
#define   i_ctr_public_INST_U_EXTERN(X, ARG)
#define   i_ctr_public_INST_L_EXTERN(X, ARG)
#define   i_ctr_public_DECL(X, ARG) X X##_with_##ARG(X, ARG);
#define   i_ctr_public_DECL_EXTERN(X, ARG) X X##_with_##ARG(X, ARG);
#define   i_ctr_public_GENERICS(X, ARG) ARG: X##_i.type.ft.with_##ARG,

#define   i_ctr_public_INIT(X, ARG) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(with_##ARG), AU_MEMBER_CONSTRUCT, 0); \
    m->alt = #X "_with_" #ARG; \
    m->access_type = interface_public; \
    X##_i.type.ft.with_##ARG = & X##_with_##ARG; \
    set_args_array(m, emit_types(X, ARG)); \
    m->type        = (Au_t)&X##_i.type; \
    /*m->offset      = offsetof(X##_f.ft, with_##ARG);*/ \
    m->value       = (void*)& X##_with_##ARG; \
    m->index   = offsetof(__typeof__(X##_i.type.ft), with_##ARG) / sizeof(void*); \
}

#define   i_ctr_public_PROTO(X, ARG)
#define   i_ctr_public_METHOD(X, ARG)      X (*with_##ARG)(X, ARG);

#define   i_ctr_intern_F(X, ARG)
#define   i_ctr_intern_F_EXTERN(X, ARG)
#define   i_ctr_intern_ISIZE(X, ARG)        
#define   i_ctr_intern_INST_U(X, ARG)
#define   i_ctr_intern_INST_L(X, ARG)
#define   i_ctr_intern_INST_U_EXTERN(X, ARG)
#define   i_ctr_intern_INST_L_EXTERN(X, ARG)
#define   i_ctr_intern_INIT(X, ARG) 
#define   i_ctr_intern_PROTO(X, ARG)
#define   i_ctr_intern_METHOD(X, ARG)
#define   i_ctr_intern_METHOD_EXTERN(X, ARG)
#define   i_ctr(X, Y, T, ARG)              i_ctr_##T##_##Y(X, ARG)



#define   i_prop_opaque_F(X, R, N)              u8 N;
#define   i_prop_opaque_F_EXTERN(X, R, N)       u8 N;
#define   i_prop_opaque_ISIZE(X, R, N) 
#define   i_prop_opaque_ISIZE_EXTERN(X, R, N)          
#define   i_prop_opaque_INST_U(X, R, N)           R N;
#define   i_prop_opaque_INST_L(X, R, N)
#define   i_prop_opaque_INST_U_EXTERN(X, R, N)    i_prop_public_INST_U(X, R, N)
#define   i_prop_opaque_INST_L_EXTERN(X, R, N)
#define   i_prop_opaque_DECL(X, R, N)           i_prop_public_DECL(X, R, N)
#define   i_prop_opaque_DECL_EXTERN(X, R, N)    i_prop_public_DECL(X, R, N)
#define   i_prop_opaque_GENERICS(X, R, N)
#define   i_prop_opaque_INIT(X, R, N)
#define   i_prop_opaque_PROTO(X, R, N)  
#define   i_prop_opaque_METHOD(X, R, N)

#define   i_prop_interface_F(X, R, N)
#define   i_prop_interface_F_EXTERN(X, R, N)
#define   i_prop_interface_ISIZE(X, R, N)      
#define   i_prop_interface_ISIZE_EXTERN(X, R, N)       
#define   i_prop_interface_INST_U(X, R, N)
#define   i_prop_interface_INST_L(X, R, N)
#define   i_prop_interface_INST_U_EXTERN(X, R, N)
#define   i_prop_interface_INST_L_EXTERN(X, R, N)
#define   i_prop_interface_DECL(X, R, N)
#define   i_prop_interface_DECL_EXTERN(X, R, N)
#define   i_prop_interface_GENERICS(X, R, N)
#define   i_prop_interface_INIT(X, R, N)
#define   i_prop_interface_PROTO(X, R, N)  
#define   i_prop_interface_METHOD(X, R, N)

#define   i_prop_public_F(X, R, N)              u8 N;
#define   i_prop_public_F_EXTERN(X, R, N)       u8 N;
#define   i_prop_public_ISIZE(X, R, N)   
#define   i_prop_public_ISIZE_EXTERN(X, R, N)     
#define   i_prop_public_ISIZE_EXTERN_meta(X, R, N, M2, ...) 
#define   i_prop_public_INST_U(X, R, N)           R N;
#define   i_prop_public_INST_L(X, R, N)
#define   i_prop_public_INST_U_EXTERN(X, R, N)    i_prop_public_INST_U(X, R, N)  
#define   i_prop_public_INST_L_EXTERN(X, R, N)
#define   i_prop_public_DECL(X, R, N)           
#define   i_prop_public_DECL_EXTERN(X, R, N)     
#define   i_prop_public_GENERICS(X, R, N)


#define   i_prop_public_INIT(X, R, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->member_type = AU_MEMBER_VAR; \
    m->index       = offsetof(struct X##_fields, N); \
}

#define   i_prop_public_PROTO(X, R, N)  
#define   i_prop_public_METHOD(X, R, N)

#define   i_prop_required_F(X, R, N)            u8 N;
#define   i_prop_required_F_EXTERN(X, R, N)     u8 N;
#define   i_prop_required_ISIZE(X, R, N)     
#define   i_prop_required_ISIZE_EXTERN(X, R, N)    
#define   i_prop_required_ISIZE_EXTERN_meta(X, R, N, M2, ...)    
#define   i_prop_required_INST_U(X, R, N)         i_prop_public_INST_U(X, R, N)
#define   i_prop_required_INST_L(X, R, N)
#define   i_prop_required_INST_U_EXTERN(X, R, N)  i_prop_public_INST_U(X, R, N)
#define   i_prop_required_INST_L_EXTERN(X, R, N)
#define   i_prop_required_DECL(X, R, N)         i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN(X, R, N)  i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS(X, R, N)
#define   i_prop_required_INIT(X, R, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_REQUIRED | AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
};

#define   i_prop_required_PROTO(X, R, N)  
#define   i_prop_required_METHOD(X, R, N)

#define   i_prop_intern_F(X, R, N)              u8 N;
#define   i_prop_intern_F_EXTERN(X, R, N)       
#define   i_prop_intern_ISIZE(X, R, N)          
#define   i_prop_intern_ISIZE_EXTERN(X, R, N)   +sizeof(R)
#define   i_prop_intern_INST_U(X, R, N)           
#define   i_prop_intern_INST_L(X, R, N)           R N;
#define   i_prop_intern_INST_U_EXTERN(X, R, N)
#define   i_prop_intern_INST_L_EXTERN(X, R, N)
#define   i_prop_intern_DECL(X, R, N)           i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN(X, R, N)    i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS(X, R, N)
#define   i_prop_intern_INIT(X, R, N)
#define   i_prop_intern_PROTO(X, R, N)  
#define   i_prop_intern_METHOD(X, R, N)

#define   i_prop_intern_F_pad(X, R, N, M2)                u8 N;
#define   i_prop_intern_F_EXTERN_pad(X, R, N, M2)       
#define   i_prop_intern_ISIZE_pad(X, R, N, M2)            
#define   i_prop_intern_ISIZE_EXTERN_pad(X, R, N, M2)     + M2
#define   i_prop_intern_INST_U_pad(X, R, N, M2)           
#define   i_prop_intern_INST_L_pad(X, R, N, M2)           union { R N; char _##N##_pad[M2]; };
#define   i_prop_intern_INST_U_EXTERN_pad(X, R, N, M2)
#define   i_prop_intern_INST_L_EXTERN_pad(X, R, N, M2)
#define   i_prop_intern_DECL_pad(X, R, N, M2)           i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN_pad(X, R, N, M2)    i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS_pad(X, R, N, M2)
#define   i_prop_intern_INIT_pad(X, R, N, M2)
#define   i_prop_intern_PROTO_pad(X, R, N, M2)  
#define   i_prop_intern_METHOD_pad(X, R, N, M2)

#define   i_prop_public_F_field(X, R, N, M2)            u8 N;
#define   i_prop_public_F_EXTERN_field(X, R, N, M2)     u8 N;
#define   i_prop_public_ISIZE_field(X, R, N, M2)        
#define   i_prop_public_INST_U_field(X, R, N, M2)         R N;
#define   i_prop_public_INST_L_field(X, R, N, M2)
#define   i_prop_public_INST_U_EXTERN_field(X, R, N, M2)  i_prop_public_INST_field(X, R, N, M2)
#define   i_prop_public_INST_L_EXTERN_field(X, R, N, M2)
#define   i_prop_public_DECL_field(X, R, N, M2)         i_prop_public_DECL(X, R, N)
#define   i_prop_public_DECL_EXTERN_field(X, R, N, M2)  i_prop_public_DECL(X, R, N)
#define   i_prop_public_GENERICS_field(X, R, N, M2)
#define   i_prop_public_INIT_field(X, R, N, M2) { \
    Au_t m = def((Au_t)&X##_i.type, #M2, AU_MEMBER_VAR, AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->id          = offsetof(struct X##_fields, N);    \
}

#define   i_prop_public_PROTO_field(X, R, N, M2)  
#define   i_prop_public_METHOD_field(X, R, N, M2)

#define   i_prop_required_F_field(X, R, N, M2)              u8 N;
#define   i_prop_required_F_EXTERN_field(X, R, N, M2)       u8 N;
#define   i_prop_required_ISIZE_field(X, R, N, M2)          +sizeof(R)
#define   i_prop_required_INST_U_field(X, R, N, M2)           i_prop_public_INST_field(X, R, N, M2)
#define   i_prop_required_INST_L_field(X, R, N, M2)
#define   i_prop_required_INST_U_EXTERN_field(X, R, N, M2)    i_prop_public_INST_field(X, R, N, M2)
#define   i_prop_required_INST_L_EXTERN_field(X, R, N, M2)
#define   i_prop_required_DECL_field(X, R, N, M2)           i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN_field(X, R, N, M2)    i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS_field(X, R, N, M2)
#define   i_prop_required_INIT_field(X, R, N, M2) { \
    Au_t m = def((Au_t)&X##_i.type, #M2, AU_MEMBER_VAR, AU_TRAIT_REQUIRED | AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
}

#define   i_prop_required_PROTO_field(X, R, N, M2)  
#define   i_prop_required_METHOD_field(X, R, N, M2)

#define   i_prop_intern_F_field(X, R, N, M2)                u8 N;
#define   i_prop_intern_F_EXTERN_field(X, R, N, M2)         
#define   i_prop_intern_ISIZE_field(X, R, N, M2)            +sizeof(R)
#define   i_prop_intern_INST_U_field(X, R, N, M2)
#define   i_prop_intern_INST_L_field(X, R, N, M2)             R N;
#define   i_prop_intern_INST_U_EXTERN_field(X, R, N, M2)
#define   i_prop_intern_INST_L_EXTERN_field(X, R, N, M2)
#define   i_prop_intern_DECL_field(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN_field(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS_field(X, R, N, M2)
#define   i_prop_intern_INIT_field(X, R, N, M2)          fault("field is not exposed when intern");
#define   i_prop_intern_PROTO_field(X, R, N, M2)  
#define   i_prop_intern_METHOD_field(X, R, N, M2)

#define   i_prop_public_F_meta(X, R, N, M2)             u8 N;
#define   i_prop_public_F_EXTERN_meta(X, R, N, M2)      u8 N;
#define   i_prop_public_ISIZE_meta(X, R, N, M2)            
#define   i_prop_public_INST_U_meta(X, R, N, M2)          R N;
#define   i_prop_public_INST_L_meta(X, R, N, M2)          
#define   i_prop_public_INST_U_EXTERN_meta(X, R, N, M2)   R N;
#define   i_prop_public_INST_L_EXTERN_meta(X, R, N, M2)
#define   i_prop_public_DECL_meta(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_public_DECL_EXTERN_meta(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_public_GENERICS_meta(X, R, N, M2)
#define   i_prop_public_INIT_meta(X, R, N, M2) {\
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
    set_meta_array(m, 1, (Au_t)&M2##_i.type); \
}

#define   i_prop_public_PROTO_meta(X, R, N, M2)  
#define   i_prop_public_METHOD_meta(X, R, N, M2)

#define   i_prop_required_F_meta(X, R, N, M2)           u8 N;
#define   i_prop_required_F_EXTERN_meta(X, R, N, M2)    u8 N;
#define   i_prop_required_ISIZE_meta(X, R, N, M2)            
#define   i_prop_required_INST_U_meta(X, R, N, M2)        R N;
#define   i_prop_required_INST_L_meta(X, R, N, M2)
#define   i_prop_required_INST_U_EXTERN_meta(X, R, N, M2) R N;
#define   i_prop_required_INST_L_EXTERN_meta(X, R, N, M2)
#define   i_prop_required_DECL_meta(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN_meta(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS_meta(X, R, N, M2)
#define   i_prop_required_INIT_meta(X, R, N, M2) {\
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_REQUIRED | AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
    set_meta_array(m, 1, (Au_t)&M2##_i.type); \
}

#define   i_prop_required_PROTO_meta(X, R, N, M2)  
#define   i_prop_required_METHOD_meta(X, R, N, M2)

#define   i_prop_intern_F_meta(X, R, N, M2)             u8 N;
#define   i_prop_intern_F_EXTERN_meta(X, R, N, M2)      
#define   i_prop_intern_ISIZE_meta(X, R, N, M2)          +sizeof(R)
#define   i_prop_intern_INST_U_meta(X, R, N, M2)
#define   i_prop_intern_INST_L_meta(X, R, N, M2)           R N;
#define   i_prop_intern_INST_U_EXTERN_meta(X, R, N, M2)
#define   i_prop_intern_INST_L_EXTERN_meta(X, R, N, M2)
#define   i_prop_intern_DECL_meta(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_intern_DECL_EXTERN_meta(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_intern_GENERICS_meta(X, R, N, M2)
#define   i_prop_intern_INIT_meta(X, R, N, M2)
#define   i_prop_intern_PROTO_meta(X, R, N, M2)  
#define   i_prop_intern_METHOD_meta(X, R, N, M2)  

#define   i_prop_public_F_as(X, R, N, M2)               u8 N;
#define   i_prop_public_F_EXTERN_as(X, R, N, M2)        u8 N;
#define   i_prop_public_ISIZE_as(X, R, N, M2)           
#define   i_prop_public_ISIZE_EXTERN_as(X, R, N, M2)  
#define   i_prop_public_INST_U_as(X, R, N, M2)            R N;
#define   i_prop_public_INST_L_as(X, R, N, M2)
#define   i_prop_public_INST_U_EXTERN_as(X, R, N, M2)     M2 N;
#define   i_prop_public_INST_L_EXTERN_as(X, R, N, M2)
#define   i_prop_public_DECL_as(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_public_DECL_EXTERN_as(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_public_GENERICS_as(X, R, N, M2)
#define   i_prop_public_INIT_as(X, R, N, M2) {\
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_IPROP); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&X##_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
    set_meta_array(m, 1, (Au_t)&X##_i.type); \
}
#define   i_prop_public_PROTO_as(X, R, N, M2)  
#define   i_prop_public_METHOD_as(X, R, N, M2)

#define   i_prop_required_F_as(X, R, N, M2)             u8 N;
#define   i_prop_required_F_EXTERN_as(X, R, N, M2)      u8 N;
#define   i_prop_required_ISIZE_as(X, R, N, M2)        
#define   i_prop_required_ISIZE_EXTERN_as(X, R, N, M2)     
#define   i_prop_required_INST_U_as(X, R, N, M2)          R N;
#define   i_prop_required_INST_L_as(X, R, N, M2)
#define   i_prop_required_INST_U_EXTERN_as(X, R, N, M2)   M2 N;
#define   i_prop_required_INST_L_EXTERN_as(X, R, N, M2) 
#define   i_prop_required_DECL_as(X, R, N, M2)        i_prop_public_DECL(X, R, N)
#define   i_prop_required_DECL_EXTERN_as(X, R, N, M2) i_prop_public_DECL(X, R, N)
#define   i_prop_required_GENERICS_as(X, R, N, M2)
#define   i_prop_required_INIT_as(X, R, N, M2) \
    verify(false, "'as' keyword used with internals and cannot be required");
#define   i_prop_required_PROTO_as(X, R, N, M2)  
#define   i_prop_required_METHOD_as(X, R, N, M2)

#define   i_prop_intern_F_as(X, R, N, M2)               u8 N;
#define   i_prop_intern_F_EXTERN_as(X, R, N, M2)        
#define   i_prop_intern_ISIZE_as(X, R, N, M2)          
#define   i_prop_intern_ISIZE_EXTERN_as(X, R, N, M2)   +sizeof(M2)
#define   i_prop_intern_INST_U_as(X, R, N, M2)           
#define   i_prop_intern_INST_L_as(X, R, N, M2)           R N;
#define   i_prop_intern_INST_U_EXTERN_as(X, R, N, M2)
#define   i_prop_intern_INST_L_EXTERN_as(X, R, N, M2)
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


#define M(X,Y,I,T, ...) \
    I##_##T(X, Y, __VA_ARGS__)

#define   i_ref_interface_F(X, R, N)
#define   i_ref_interface_F_EXTERN(X, R, N)
#define   i_ref_interface_ISIZE(X, R, N)
#define   i_ref_interface_ISIZE_EXTERN(X, R, N)
#define   i_ref_interface_INST_U(X, R, N)
#define   i_ref_interface_INST_L(X, R, N)
#define   i_ref_interface_INST_U_EXTERN(X, R, N)
#define   i_ref_interface_INST_L_EXTERN(X, R, N)
#define   i_ref_interface_DECL(X, R, N)        
#define   i_ref_interface_DECL_EXTERN(X, R, N) 
#define   i_ref_interface_GENERICS(X, R, N)
#define   i_ref_interface_PROTO(X, R, N)  
#define   i_ref_interface_METHOD(X, R, N)


#define   i_origin_public_F(X, R, N) u8 N;
#define   i_origin_public_F_EXTERN(X, R, N) u8 N;
#define   i_origin_public_ISIZE(X, R, N)        
#define   i_origin_public_ISIZE_EXTERN(X, R, N) 
#define   i_origin_public_INST_U(X, R, N)         union { Au* N; i8* N##_i8; i16* N##_i16; i32* N##_i32; i64* N##_i64; f64* N##_f64; f32* N##_f32; fp16* N##_f16; };
#define   i_origin_public_INST_L(X, R, N)
#define   i_origin_public_INST_U_EXTERN(X, R, N)  i_origin_public_INST_U(X, R, N)
#define   i_origin_public_INST_L_EXTERN(X, R, N)
#define   i_origin_public_DECL(X, R, N)          
#define   i_origin_public_DECL_EXTERN(X, R, N)   
#define   i_origin_public_GENERICS(X, R, N)
#define   i_origin_public_INIT(X, R, N) {\
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_VPROP); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&ARef_i.type; \
}

#define   i_origin_public_PROTO(X, R, N)  
#define   i_origin_public_METHOD(X, R, N)

#define i_origin(X, Y, ACCESS, TY, N) i_origin_## ACCESS ##_##Y(X, TY, N)


#define   i_ref_public_F(X, R, N) u8 N;
#define   i_ref_public_F_EXTERN(X, R, N) u8 N;
#define   i_ref_public_ISIZE(X, R, N)        
#define   i_ref_public_ISIZE_EXTERN(X, R, N) 
#define   i_ref_public_INST_U(X, R, N)         R* N;
#define   i_ref_public_INST_L(X, R, N)
#define   i_ref_public_INST_U_EXTERN(X, R, N)  i_ref_public_INST_U(X, R, N)
#define   i_ref_public_INST_L_EXTERN(X, R, N)
#define   i_ref_public_DECL(X, R, N)          i_prop_public_DECL(X, R, N)
#define   i_ref_public_DECL_EXTERN(X, R, N)   i_prop_public_DECL(X, R, N)
#define   i_ref_public_GENERICS(X, R, N)
#define   i_ref_public_INIT(X, R, N) {\
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_VPROP); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&ARef_i.type; \
}

#define   i_ref_public_PROTO(X, R, N)  
#define   i_ref_public_METHOD(X, R, N)

#define   i_ref_required_F(X, R, N) u8 N;
#define   i_ref_required_F_EXTERN(X, R, N) u8 N;
#define   i_ref_required_ISIZE(X, R, N)          
#define   i_ref_required_ISIZE_EXTERN(X, R, N)          
#define   i_ref_required_INST_U(X, R, N)         i_ref_public_INST_U(X, R, N)
#define   i_ref_required_INST_L(X, R, N)
#define   i_ref_required_INST_U_EXTERN(X, R, N)  i_ref_public_INST_U(X, R, N)
#define   i_ref_required_INST_L_EXTERN(X, R, N)
#define   i_ref_required_DECL(X, R, N)         i_prop_public_DECL(X, R, N)
#define   i_ref_required_DECL_EXTERN(X, R, N)  i_prop_public_DECL(X, R, N)
#define   i_ref_required_GENERICS(X, R, N)
#define   i_ref_required_INIT(X, R, N) {\
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_VPROP | AU_TRAIT_REQUIRED); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&ARef_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
}

#define   i_ref_required_PROTO(X, R, N)  
#define   i_ref_required_METHOD(X, R, N)

#define   i_ref_intern_F(X, R, N) u8 N;
#define   i_ref_intern_F_EXTERN(X, R, N)
#define   i_ref_intern_ISIZE(X, R, N)          
#define   i_ref_intern_ISIZE_EXTERN(X, R, N)   +sizeof(R*)
#define   i_ref_intern_INST_U(X, R, N)
#define   i_ref_intern_INST_L(X, R, N)           R* N;
#define   i_ref_intern_INST_U_EXTERN(X, R, N)
#define   i_ref_intern_INST_L_EXTERN(X, R, N)
#define   i_ref_intern_DECL(X, R, N)          i_prop_public_DECL(X, R, N)
#define   i_ref_intern_DECL_EXTERN(X, R, N)   i_prop_public_DECL(X, R, N)
#define   i_ref_intern_GENERICS(X, R, N)
#define   i_ref_intern_INIT(X, R, N)
#define   i_ref_intern_PROTO(X, R, N)  
#define   i_ref_intern_METHOD(X, R, N)  

#define   i_ref(X, Y, T, R, N) i_ref_##T##_##Y(X, R, N)



#define i_attr_F(           X, ENUM, ID, VALUE, ...)
#define i_attr_F_EXTERN(    X, ENUM, ID, VALUE, ...)
#define i_attr_ISIZE(       X, ENUM, ID, VALUE, ...)
#define i_attr_ISIZE_EXTERN(       X, ENUM, ID, VALUE, ...)
#define i_attr_INST_U(        X, ENUM, ID, VALUE, ...)
#define i_attr_INST_L(        X, ENUM, ID, VALUE, ...)
#define i_attr_INST_U_EXTERN( X, ENUM, ID, VALUE, ...)
#define i_attr_INST_L_EXTERN( X, ENUM, ID, VALUE, ...)
#define i_attr_DECL(        X, ENUM, ID, VALUE, ...)
#define i_attr_DECL_EXTERN( X, ENUM, ID, VALUE, ...)
#define i_attr_GENERICS(    X, ENUM, ID, VALUE, ...)
#define i_attr_INIT(        X, ENUM, ID, VALUE, ...) { \
    Au_t m = def((Au_t)&X##_i.type, #ID, AU_MEMBER_ATTR, 0); \
    m->access_type = interface_public; \
    m->index       = ENUM##_##ID; \
    m->value       = VALUE; \
    m->type        = (Au_t)&ENUM##_i.type; \
    set_meta_array(m, emit_types(__VA_ARGS__)); \
}

#define i_attr_PROTO(       X, ENUM, ID, VALUE, ...)  
#define i_attr_METHOD(      X, ENUM, ID, VALUE, ...)  

#define i_attr(X, Y, ENUM, ID, VALUE, ...) i_attr_##Y(X, ENUM, ID, VALUE, __VA_ARGS__)

#define   i_array_interface_F(X, R, S, N)
#define   i_array_interface_F_EXTERN(X, R, S, N)
#define   i_array_interface_ISIZE(X, R, S, N)
#define   i_array_interface_ISIZE_EXTERN(X, R, S, N)
#define   i_array_interface_INST_U(X, R, S, N)
#define   i_array_interface_INST_L(X, R, S, N)
#define   i_array_interface_INST_U_EXTERN(X, R, S, N)
#define   i_array_interface_INST_L_EXTERN(X, R, S, N)
#define   i_array_interface_DECL(X, R, S, N)
#define   i_array_interface_DECL_EXTERN(X, R, S, N)
#define   i_array_interface_GENERICS(X, R, S, N)
#define   i_array_interface_INIT(X, R, S, N)
#define   i_array_interface_PROTO(X, R, S, N)  
#define   i_array_interface_METHOD(X, R, S, N)


#define   i_array_public_F(X, R, S, N) u8 N;
#define   i_array_public_F_EXTERN(X, R, S, N) u8 N;
#define   i_array_public_ISIZE(X, R, S, N)
#define   i_array_public_ISIZE_EXTERN(X, R, S, N)
#define   i_array_public_INST_U(X, R, S, N)         R N[S];  
#define   i_array_public_INST_L(X, R, S, N)         
#define   i_array_public_INST_U_EXTERN(X, R, S, N)  i_array_public_INST_U(X, R, S, N)
#define   i_array_public_INST_L_EXTERN(X, R, S, N)
#define   i_array_public_DECL(X, R, S, N)           i_prop_public_DECL(X, R, N)
#define   i_array_public_DECL_EXTERN(X, R, S, N)
#define   i_array_public_GENERICS(X, R, S, N)
#define   i_array_public_INIT(X, R, S, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, 0); \
    m->access_type = interface_public; \
    m->offset      = offsetof(struct _##X, N); \
    m->type        = (Au_t)&R##_i.type; \
    m->index       = offsetof(struct X##_fields, N); \
    m->elements    = S; \
}

#define   i_array_public_PROTO(X, R, S, N)  
#define   i_array_public_METHOD(X, R, S, N)

#define   i_array_intern_F(X, R, S, N) u8 N;
#define   i_array_intern_F_EXTERN(X, R, S, N)
#define   i_array_intern_ISIZE(X, R, S, N)              
#define   i_array_intern_ISIZE_EXTERN(X, R, S, N)        +sizeof(R[S])
#define   i_array_intern_INST_U(X, R, S, N)         
#define   i_array_intern_INST_L(X, R, S, N)         R N[S];
#define   i_array_intern_INST_U_EXTERN(X, R, S, N)
#define   i_array_intern_INST_L_EXTERN(X, R, S, N)
#define   i_array_intern_DECL(X, R, S, N)         i_prop_public_DECL(X, R, N)
#define   i_array_intern_DECL_EXTERN(X, R, S, N)
#define   i_array_intern_GENERICS(X, R, S, N)
#define   i_array_intern_INIT(X, R, S, N)
#define   i_array_intern_PROTO(X, R, S, N)  
#define   i_array_intern_METHOD(X, R, S, N)  
#define   i_array(X, Y, T, R, S, N) i_array_##T##_##Y(X, R, S, N)

#define   i_struct_ctr_ISIZE(X, ARG)
#define   i_struct_ctr_ISIZE_EXTERN(X, ARG)
#define   i_struct_ctr_INST(X, ARG)
#define   i_struct_ctr_INST_EXTERN(X, ARG)
#define   i_struct_ctr_DECL(X, ARG)
#define   i_struct_ctr_DECL_EXTERN(X, ARG)
#define   i_struct_ctr_GENERICS(X, ARG) ARG*: X##_i.type.ft.with_##ARG,
#define   i_struct_ctr_INIT(X, ARG) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(with_##ARG), AU_MEMBER_CONSTRUCT, 0); \
    m->access_type = interface_public; \
    X##_i.type.ft.with_##ARG = & X##_with_##ARG; \
    m->type        = (Au_t)&ARG##_i.type; \
    /* m->offset      = offsetof(X##_f, with_##ARG); */ \
    m->value       = (void*)& X##_with_##ARG; \
    m->index       = offsetof(__typeof__(X##_i.type.ft), with_##ARG) / sizeof(void*); \
}

#define   i_struct_ctr_PROTO(X, ARG)
#define   i_struct_ctr_METHOD(X, ARG)      X (*with_##ARG)(ARG*);
#define   i_struct_ctr(X, Y, ARG)          i_struct_ctr_##Y(X, ARG)

#define   i_struct_ctr_obj_ISIZE(X, ARG)
#define   i_struct_ctr_obj_ISIZE_EXTERN(X, ARG)
#define   i_struct_ctr_obj_INST(X, ARG)
#define   i_struct_ctr_obj_INST_EXTERN(X, ARG)
#define   i_struct_ctr_obj_DECL(X, ARG)
#define   i_struct_ctr_obj_DECL_EXTERN(X, ARG)
#define   i_struct_ctr_obj_GENERICS(X, ARG) ARG: X##_i.type.ft.with_##ARG,
#define   i_struct_ctr_obj_INIT(X, ARG) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(with_##ARG), AU_MEMBER_CONSTRUCT, 0); \
    m->access_type = interface_public; \
    X##_i.type.ft.with_##ARG = & X##_with_##ARG; \
    m->type        = (Au_t)&ARG##_i.type; \
    m->value       = (void*)& X##_with_##ARG; \
}

#define   i_struct_ctr_obj_PROTO(X, ARG)
#define   i_struct_ctr_obj_METHOD(X, ARG)      X (*with_##ARG)(ARG);
#define   i_struct_ctr_obj(X, Y, ARG)          i_struct_ctr_obj_##Y(X, ARG)

#define   i_struct_array_ISIZE(X, R, S, N)
#define   i_struct_array_ISIZE_EXTERN(X, R, S, N)
#define   i_struct_array_INST(X, R, S, N)         R N[S];
#define   i_struct_array_INST_EXTERN(X, R, S, N)  i_struct_array_INST(X, R, S, N)
#define   i_struct_array_DECL(X, R, S, N)
#define   i_struct_array_DECL_EXTERN(X, R, S, N)
#define   i_struct_array_GENERICS(X, R, S, N)
#define   i_struct_array_INIT(X, R, S, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, 0); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&R##_i.type; \
    m->elements    = S; \
    m->member_type = AU_MEMBER_VAR; \
}

#define   i_struct_array_PROTO(X, R, S, N)  
#define   i_struct_array_METHOD(X, R, S, N)        
#define   i_struct_array(X, Y, R, S, N) i_struct_array_##Y(X, R, S, N)

#define   i_struct_prop_ISIZE(X, R, N)
#define   i_struct_prop_ISIZE_EXTERN(X, R, N)
#define   i_struct_prop_INST(X, R, N)         R N;
#define   i_struct_prop_INST_EXTERN(X, R, N)  i_struct_prop_INST(X, R, N)
#define   i_struct_prop_DECL(X, R, N)
#define   i_struct_prop_DECL_EXTERN(X, R, N)
#define   i_struct_prop_GENERICS(X, R, N)
#define   i_struct_prop_INIT(X, R, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, 0); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&R##_i.type; \
}

#define   i_struct_prop_PROTO(X, R, N)  
#define   i_struct_prop_METHOD(X, R, N)      
#define   i_struct_prop(X, Y, R, N) i_struct_prop_##Y(X, R, N)

#define   i_struct_cast_ISIZE(X, R)
#define   i_struct_cast_ISIZE_EXTERN(X, R)
#define   i_struct_cast_INST(X, R)
#define   i_struct_cast_INST_EXTERN(X, R)
#define   i_struct_cast_DECL(X, R)
#define   i_struct_cast_DECL_EXTERN(X, R)
#define   i_struct_cast_GENERICS(X, R)
#define   i_struct_cast_INIT(ST, R) { \
    Au_t m = def((Au_t)&ST##_i.type, stringify(cast_##R), AU_MEMBER_CAST, 0); \
    m->access_type = interface_public; \
    ST##_i.type.ft.cast_##R = & ST##_cast_##R; \
    set_args_array(m, emit_types(ST)); \
    m->type    = (Au_t)&R##_i.type; \
    m->offset  = offsetof(__typeof(ST##_i.type.ft), cast_##R); \
    m->member_type = AU_MEMBER_CAST; \
}

#define   i_struct_cast_PROTO(X, R)
#define   i_struct_cast_METHOD(X, R)        R (*cast_##R)(X);         
#define   i_struct_cast(X, Y, R)                i_struct_cast_##Y(X, R)

#define   i_struct_method_ISIZE(    X, R, N, ...)
#define   i_struct_method_ISIZE_EXTERN(X, R, N, ...)
#define   i_struct_method_INST(    X, R, N, ...)
#define   i_struct_method_INST_EXTERN(    X, R, N, ...)
#define   i_struct_method_DECL(X, R, N, ...)        R X##_##N(X* __VA_OPT__(,) __VA_ARGS__);
#define   i_struct_method_DECL_EXTERN(X, R, N, ...) R X##_##N(X* __VA_OPT__(,) __VA_ARGS__);
#define   i_struct_method_GENERICS(X, R, N, ...)
#define   i_struct_method_INIT(    X, R, N, ...) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_IMETHOD); \
    m->access_type = interface_public; \
    X##_i.type . ft.N = & X## _ ## N; \
    m->type    = (Au_t)&R##_i.type; \
}

#define   i_struct_method_PROTO(X, R, N, ...)
#define   i_struct_method_METHOD(X, R, N, ...)      R (*N)(X* __VA_OPT__(,) __VA_ARGS__);
#define   i_struct_method(X, Y, R, N, ...)          i_struct_method_##Y(X, R, N, __VA_ARGS__)


#define   i_struct_static_ISIZE(    X, R, N, ...)
#define   i_struct_static_ISIZE_EXTERN(X, R, N, ...)
#define   i_struct_static_INST(    X, R, N, ...)
#define   i_struct_static_INST_EXTERN(X, R, N, ...)
#define   i_struct_static_DECL(    X, R, N, ...)            R X##_##N(__VA_ARGS__);
#define   i_struct_static_DECL_EXTERN(    X, R, N, ...)     R X##_##N(__VA_ARGS__);
#define   i_struct_static_GENERICS(X, R, N, ...)
#define   i_struct_static_INIT(    X, R, N, ...) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_SMETHOD); \
    m->access_type = interface_public; \
    X##_i.type . ft.N = & X## _ ## N; \
    m->type    = (Au_t)&R##_i.type; \
}

#define   i_struct_static_PROTO(X, R, N, ...)
#define   i_struct_static_METHOD(X, R, N, ...)      R (*N)(__VA_ARGS__);
#define   i_struct_static(X, Y, R, N, ...)          i_struct_static_##Y(X, R, N, __VA_ARGS__)









#define   i_inlay_public_F(X, R, N)            u8 N;
#define   i_inlay_public_F_EXTERN(X, R, N)     u8 N;
#define   i_inlay_public_ISIZE(X, R, N)
#define   i_inlay_public_ISIZE_EXTERN(X, R, N)
#define   i_inlay_public_INST_U(X, R, N)         struct _##R N;
#define   i_inlay_public_INST_L(X, R, N)         
#define   i_inlay_public_INST_U_EXTERN(X, R, N)  struct _##R N;
#define   i_inlay_public_INST_L_EXTERN(X, R, N)
#define   i_inlay_public_DECL(X, R, N)
#define   i_inlay_public_DECL_EXTERN(X, R, N)
#define   i_inlay_public_GENERICS(X, R, N)
#define   i_inlay_public_INIT(X, R, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_INLAY); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&R##_i.type; \
}

#define   i_inlay_public_PROTO(X, R, N)  
#define   i_inlay_public_METHOD(X, R, N)

#define   i_inlay_required_F(X, R, N) u8 N;
#define   i_inlay_required_F_EXTERN(X, R, N) u8 N;
#define   i_inlay_required_ISIZE(X, R, N)           
#define   i_inlay_required_ISIZE_EXTERN(X, R, N)    
#define   i_inlay_required_INST_U(X, R, N)         i_inlay_public_INST_U(X, R, N)
#define   i_inlay_required_INST_L(X, R, N)         
#define   i_inlay_required_INST_U_EXTERN(X, R, N)  i_inlay_public_INST_U_EXTERN(X, R, N)
#define   i_inlay_required_INST_L_EXTERN(X, R, N)
#define   i_inlay_required_DECL(X, R, N)
#define   i_inlay_required_DECL_EXTERN(X, R, N)
#define   i_inlay_required_GENERICS(X, R, N)
#define   i_inlay_required_INIT(X, R, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_VAR, AU_TRAIT_INLAY | AU_TRAIT_REQUIRED); \
    m->access_type = interface_public; \
    m->offset   = offsetof(struct _##X, N); \
    m->type     = (Au_t)&R##_i.type; \
    m->index    = offsetof(struct X##_fields, N); \
}

#define   i_inlay_required_PROTO(X, R, N)  
#define   i_inlay_required_METHOD(X, R, N)

#define   i_inlay_intern_F(X, R, N)               u8 N;
#define   i_inlay_intern_F_EXTERN(X, R, N)        
#define   i_inlay_intern_ISIZE(X, R, N)           
#define   i_inlay_intern_ISIZE_EXTERN(X, R, N, ...) +sizeof(struct _##R)
#define   i_inlay_intern_INST_U(X, R, N)            
#define   i_inlay_intern_INST_L(X, R, N)            struct _##R N;
#define   i_inlay_intern_INST_U_EXTERN(X, R, N)
#define   i_inlay_intern_INST_L_EXTERN(X, R, N)
#define   i_inlay_intern_DECL(X, R, N)
#define   i_inlay_intern_DECL_EXTERN(X, R, N)
#define   i_inlay_intern_GENERICS(X, R, N)
#define   i_inlay_intern_INIT(X, R, N)
#define   i_inlay_intern_PROTO(X, R, N)  
#define   i_inlay_intern_METHOD(X, R, N)  
#define   i_inlay(X, Y, T, R, N) i_inlay_##T##_##Y(X, R, N)


#define   s_method_interface_F(X, R, N, ...)
#define   s_method_interface_F_EXTERN(X, R, N, ...)
#define   s_method_interface_ISIZE(X, R, N, ...)   
#define   s_method_interface_ISIZE_EXTERN(X, R, N, ...)   
#define   s_method_interface_INST_U(X, R, N, ...)
#define   s_method_interface_INST_L(X, R, N, ...)
#define   s_method_interface_INST_U_EXTERN(X, R, N, ...)
#define   s_method_interface_INST_L_EXTERN(X, R, N, ...)
#define   s_method_interface_DECL(X, R, N, ...)
#define   s_method_interface_DECL_EXTERN(X, R, N, ...)
#define   s_method_interface_GENERICS(X, R, N, ...)
#define   s_method_interface_INIT(X, R, N, ...)
#define   s_method_interface_PROTO(X, R, N, ...)
#define   s_method_interface_METHOD(X, R, N, ...)

#define   s_method_public_F(X, R, N, ...)
#define   s_method_public_F_EXTERN(X, R, N, ...)
#define   s_method_public_ISIZE(X, R, N, ...)
#define   s_method_public_ISIZE_EXTERN(X, R, N, ...)
#define   s_method_public_INST_U(X, R, N, ...)
#define   s_method_public_INST_L(X, R, N, ...)
#define   s_method_public_INST_U_EXTERN(X, R, N, ...)
#define   s_method_public_INST_L_EXTERN(X, R, N, ...)
#define   s_method_public_DECL(X, R, N, ...)        R N(__VA_ARGS__);
#define   s_method_public_DECL_EXTERN(X, R, N, ...) R N(__VA_ARGS__);
#define   s_method_public_GENERICS(X, R, N, ...)
#define   s_method_public_INIT(X, R, N, ...) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_SMETHOD); \
    m->access_type = interface_public; \
    set_args_array(m, emit_types(__VA_ARGS__)); \
    m->type    = (Au_t)&R##_i.type; \
    m->offset  = 0; \
    m->value   = (object)&N; \
}

#define   s_method_public_PROTO(X, R, N, ...)
#define   s_method_public_METHOD(X, R, N, ...)
#define   s_method_intern_F(X, R, N, ...)
#define   s_method_intern_F_EXTERN(X, R, N, ...)
#define   s_method_intern_ISIZE(X, R, N, ...)
#define   s_method_intern_ISIZE_EXTERN(X, R, N, ...)
#define   s_method_intern_INST_U(X, R, N, ...)
#define   s_method_intern_INST_L(X, R, N, ...)
#define   s_method_intern_INST_U_EXTERN(X, R, N, ...)
#define   s_method_intern_INST_L_EXTERN(X, R, N, ...)
#define   s_method_intern_DECL(X, R, N, ...)        R N(__VA_ARGS__);
#define   s_method_intern_DECL_EXTERN(X, R, N, ...)
#define   s_method_intern_GENERICS(X, R, N, ...)
#define   s_method_intern_INIT(X, R, N, ...)      
#define   s_method_intern_PROTO(X, R, N, ...)
#define   s_method_intern_METHOD(X, R, N, ...)

#define   i_method_interface_F(X, R, N, ...)
#define   i_method_interface_F_EXTERN(X, R, N, ...)
#define   i_method_interface_ISIZE(X, R, N, ...)
#define   i_method_interface_ISIZE_EXTERN(X, R, N, ...)
#define   i_method_interface_INST_U(X, R, N, ...)
#define   i_method_interface_INST_L(X, R, N, ...)
#define   i_method_interface_INST_U_EXTERN(X, R, N, ...)
#define   i_method_interface_INST_L_EXTERN(X, R, N, ...)
#define   i_method_interface_DECL(X, R, N, ...)
#define   i_method_interface_DECL_EXTERN(X, R, N, ...)
#define   i_method_interface_GENERICS(X, R, N, ...)
#define   i_method_interface_INIT(X, R, N, ...)
#define   i_method_interface_PROTO(X, R, N, ...)
#define   i_method_interface_METHOD(X, R, N, ...)

#define   i_method_public_F(    X, R, N, ...)
#define   i_method_public_F_EXTERN(    X, R, N, ...)
#define   i_method_public_ISIZE(X, R, N, ...)
#define   i_method_public_ISIZE_EXTERN(X, R, N, ...)
#define   i_method_public_INST_U(    X, R, N, ...)
#define   i_method_public_INST_L(    X, R, N, ...)
#define   i_method_public_INST_U_EXTERN(  X, R, N, ...)
#define   i_method_public_INST_L_EXTERN(  X, R, N, ...)
#define   i_method_public_DECL(    X, R, N, ...)           R X##_##N(__VA_ARGS__);
#define   i_method_public_DECL_EXTERN(    X, R, N, ...)    
#define   i_method_public_GENERICS(X, R, N, ...)
#define   i_method_public_INIT(    X, R, N, ...) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_IMETHOD); \
    m->alt = #X "_" #N; \
    m->access_type = interface_public; \
    X##_i.type . ft.N = & X## _ ## N; \
    set_args_array(m, emit_types(__VA_ARGS__)); \
    m->type    = (Au_t)&R##_i.type; \
    /* m->offset  = offsetof(X##_f, N); */ \
    m->index   = offsetof(__typeof__(X##_i.type.ft), N) / sizeof(void*); \
    m->value   = (object)X##_i.type . ft.N; \
}

#define   i_method_public_PROTO(X, R, N, ...)
#define   i_method_public_METHOD(X, R, N, ...)          R (*N)(__VA_ARGS__);

#define   i_method_intern_F(    X, R, N, ...)
#define   i_method_intern_F_EXTERN(    X, R, N, ...)
#define   i_method_intern_ISIZE(    X, R, N, ...)
#define   i_method_intern_INST_U(    X, R, N, ...)
#define   i_method_intern_INST_L(    X, R, N, ...)
#define   i_method_intern_INST_U_EXTERN(  X, R, N, ...)
#define   i_method_intern_INST_L_EXTERN(  X, R, N, ...)
#define   i_method_intern_DECL(    X, R, N, ...)        R (*N)(__VA_ARGS__);
#define   i_method_intern_DECL_EXTERN(    X, R, N, ...)
#define   i_method_intern_GENERICS(X, R, N, ...)
#define   i_method_intern_INIT(    X, R, N, ...)    
#define   i_method_intern_PROTO(X, R, N, ...)
#define   i_method_intern_METHOD(X, R, N, ...)      

#define   i_final_public_F(ORIG,    X, R, N, ...)
#define   i_final_public_F_EXTERN(ORIG,    X, R, N, ...)
#define   i_final_public_ISIZE(ORIG,    X, R, N, ...)
#define   i_final_public_ISIZE_EXTERN(ORIG,    X, R, N, ...)
#define   i_final_public_INST_U(ORIG,    X, R, N, ...)
#define   i_final_public_INST_L(ORIG,    X, R, N, ...)
#define   i_final_public_INST_U_EXTERN(ORIG,  X, R, N, ...)
#define   i_final_public_INST_L_EXTERN(ORIG,  X, R, N, ...)
#define   i_final_public_DECL(ORIG,    X, R, N, ...)           R X##_##N(__VA_ARGS__);
#define   i_final_public_DECL_EXTERN(ORIG,    X, R, N, ...)    
#define   i_final_public_GENERICS(ORIG,X, R, N, ...)
#define   i_final_public_INIT(ORIG,    X, R, N, ...) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_IFINAL); \
    m->alt = #X "_" #N; \
    m->access_type = interface_public; \
    set_args_array(m, emit_types(__VA_ARGS__)); \
    m->type    = (Au_t)&R##_i.type; \
    m->offset  = 0; \
    m->value   = (object)&X##_##N; \
}

#define   i_final_public_PROTO(ORIG,X, R, N, ...)
#define   i_final_public_METHOD(ORIG,X, R, N, ...)   R (*N)(__VA_ARGS__);

/// avoiding conflict with parsing i_method uses
#define   i_method\
(X, Y, T, R, N, ...) i_method_##T##_##Y(X, R, N, X __VA_OPT__(,) __VA_ARGS__)

#define   i_final\
(X, Y, T, R, N, ORIG, ...) i_final_##T##_##Y(ORIG, X, R, N, ORIG __VA_OPT__(,) __VA_ARGS__)

#define   s_method\
(X, Y, T, R, N, ...) s_method_##T##_##Y(X, R, N __VA_OPT__(,) __VA_ARGS__)

#define   i_guard\
(X, Y, T, R, N, ...) i_method_##T##_##Y(X, R, N, __VA_ARGS__)

#define   i_operator_interface_F(X, R, N, ARG)
#define   i_operator_interface_F_EXTERN(X, R, N, ARG)
#define   i_operator_interface_ISIZE(X, R, N, ARG)
#define   i_operator_interface_INST_U(X, R, N, ARG)
#define   i_operator_interface_INST_L(X, R, N, ARG)
#define   i_operator_interface_INST_U_EXTERN(X, R, N, ARG)
#define   i_operator_interface_INST_L_EXTERN(X, R, N, ARG)
#define   i_operator_interface_DECL(X, R, N, ARG)
#define   i_operator_interface_DECL_EXTERN(X, R, N, ARG)
#define   i_operator_interface_GENERICS(X, R, N, ARG)
#define   i_operator_interface_INIT(X, R, N, ARG)
#define   i_operator_interface_PROTO(X, R, N, ARG)
#define   i_operator_interface_METHOD(X, R, N, ARG)

#define   i_operator_public_F(X, R, N, ARG)
#define   i_operator_public_F_EXTERN(X, R, N, ARG)
#define   i_operator_public_ISIZE(X, R, N, ARG)
#define   i_operator_public_ISIZE_EXTERN(X, R, N, ARG)
#define   i_operator_public_INST_U(X, R, N, ARG)
#define   i_operator_public_INST_L(X, R, N, ARG)
#define   i_operator_public_INST_U_EXTERN(X, R, N, ARG)
#define   i_operator_public_INST_L_EXTERN(X, R, N, ARG)
#define   i_operator_public_DECL(X, R, N, ARG)
#define   i_operator_public_DECL_EXTERN(X, R, N, ARG)
#define   i_operator_public_GENERICS(X, R, N, ARG)
#define   i_operator_public_INIT(X, R, N, ARG) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_OPERATOR, 0); \
    m->alt = #X "_operator_" #N; \
    m->access_type = interface_public; \
    X##_i.type  . ft.operator_##N = & X## _operator_ ## N; \
    m->ident   = stringify(operator_##N); \
    set_args_array(m, emit_types(X, ARG)); \
    m->type    = (Au_t)&R##_i.type; \
    m->index  = offsetof(__typeof__(X##_i.type.ft), operator_##N) / sizeof(void*); \
    m->member_type = AU_MEMBER_OPERATOR; \
    m->operator_type = OPType_ ## N; \
}

#define   i_operator_public_PROTO(X, R, N, ARG)
#define   i_operator_public_METHOD(X, R, N, ARG)    R (*operator_ ## N)(X, ARG);

#define   i_operator_intern_F(X, R, N, ARG)
#define   i_operator_intern_F_EXTERN(X, R, N, ARG)
#define   i_operator_intern_ISIZE(X, R, N, ARG)
#define   i_operator_intern_INST_U(X, R, N, ARG)
#define   i_operator_intern_INST_L(X, R, N, ARG)
#define   i_operator_intern_INST_U_EXTERN(X, R, N, ARG)
#define   i_operator_intern_INST_L_EXTERN(X, R, N, ARG)
#define   i_operator_intern_DECL(X, R, N, ARG)
#define   i_operator_intern_DECL_EXTERN(X, R, N, ARG)
#define   i_operator_intern_GENERICS(X, R, N, ARG)
#define   i_operator_intern_INIT(X, R, N, ARG)      
#define   i_operator_intern_PROTO(X, R, N, ARG)
#define   i_operator_intern_METHOD(X, R, N, ARG)    
#define   i_operator(X, Y, T, R, N, ARG)            i_operator_##T##_##Y(X, R, N, ARG)

#define   i_cast_interface_F(X, R)
#define   i_cast_interface_F_EXTERN(X, R)
#define   i_cast_interface_ISIZE(X, R)
#define   i_cast_interface_INST_U(X, R)
#define   i_cast_interface_INST_L(X, R)
#define   i_cast_interface_INST_U_EXTERN(X, R)
#define   i_cast_interface_INST_L_EXTERN(X, R)
#define   i_cast_interface_DECL(X, R)
#define   i_cast_interface_DECL_EXTERN(X, R)
#define   i_cast_interface_GENERICS(X, R)
#define   i_cast_interface_INIT(X, R)
#define   i_cast_interface_PROTO(X, R)
#define   i_cast_interface_METHOD(X, R)

#define   i_cast_public_F(X, R)
#define   i_cast_public_F_EXTERN(X, R)
#define   i_cast_public_ISIZE(X, R)
#define   i_cast_public_ISIZE_EXTERN(X, R)
#define   i_cast_public_INST_U(X, R)
#define   i_cast_public_INST_L(X, R)
#define   i_cast_public_INST_U_EXTERN(X, R)
#define   i_cast_public_INST_L_EXTERN(X, R)
#define   i_cast_public_DECL(X, R)
#define   i_cast_public_DECL_EXTERN(X, R)
#define   i_cast_public_GENERICS(X, R)
#define   i_cast_public_INIT(CL, R) { \
    Au_t m = def((Au_t)&CL##_i.type, stringify(cast_##R), AU_MEMBER_CAST, 0); \
    m->alt = #CL "_cast_" #R; \
    m->access_type = interface_public; \
    CL##_i.type.ft.cast_##R = & CL##_cast_##R; \
    m->value = (object)CL##_i.type.ft.cast_##R; \
    set_args_array(m, emit_types(CL)); \
    m->type    = (Au_t)&R##_i.type; \
    m->index   = offsetof(__typeof(CL##_i.type.ft), cast_##R) / sizeof(void*); \
}

#define   i_cast_public_PROTO(X, R)
#define   i_cast_public_METHOD(X, R)        R (*cast_##R)(X);

#define   i_cast_intern_F(X, R)
#define   i_cast_intern_F_EXTERN(X, R)
#define   i_cast_intern_ISIZE(X, R)
#define   i_cast_intern_INST_U(X, R)
#define   i_cast_intern_INST_L(X, R)
#define   i_cast_intern_INST_U_EXTERN(X, R)
#define   i_cast_intern_INST_L_EXTERN(X, R)
#define   i_cast_intern_DECL(X, R)
#define   i_cast_intern_DECL_EXTERN(X, R)
#define   i_cast_intern_GENERICS(X, R)
#define   i_cast_intern_INIT(X, R)          
#define   i_cast_intern_PROTO(X, R)
#define   i_cast_intern_METHOD(X, R)        
#define   i_cast(X, Y, T, R)                i_cast_##T##_##Y(X, R)

#define i_index_interface_F(X, R, ...)
#define i_index_interface_F_EXTERN(X, R, ...)
#define i_index_interface_ISIZE(X, R, ...)
#define i_index_interface_INST_U(X, R, ...)
#define i_index_interface_INST_L(X, R, ...)
#define i_index_interface_INST_U_EXTERN(X, R, ...)
#define i_index_interface_INST_L_EXTERN(X, R, ...)
#define i_index_interface_DECL(X, R, ...)
#define i_index_interface_DECL_EXTERN(X, R, ...)
#define i_index_interface_GENERICS(X, R, ...)
#define i_index_interface_INIT(X, R, ...)
#define i_index_interface_PROTO(X, R, ...)
#define i_index_interface_METHOD(X, R, ...)

#define i_index_public_F(X, R, ...)
#define i_index_public_F_EXTERN(X, R, ...)
#define i_index_public_ISIZE(X, R, ...)
#define i_index_public_ISIZE_EXTERN(X, R, ...)
#define i_index_public_INST_U(X, R, ...)
#define i_index_public_INST_L(X, R, ...)
#define i_index_public_INST_U_EXTERN(X, R, ...)
#define i_index_public_INST_L_EXTERN(X, R, ...)
#define i_index_public_DECL(X, R, ...)
#define i_index_public_DECL_EXTERN(X, R, ...)
#define i_index_public_GENERICS(X, R, ...)
#define i_index_public_INIT(X, R, ...) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(emit_idx_symbol(index, __VA_ARGS__)), AU_MEMBER_INDEX, 0); \
    m->alt = #X "_idx_" #R; \
    m->access_type = interface_public; \
    X##_i.type.ft.emit_idx_symbol(index, __VA_ARGS__) = & emit_idx_symbol(X ## _index, __VA_ARGS__); \
    set_args_array(m, emit_types(X, __VA_ARGS__)); \
    m->type        = (Au_t)&R##_i.type; \
    m->index        = offsetof(__typeof(X##_i.type.ft), emit_idx_symbol(index, __VA_ARGS__)) / sizeof(void*); \
}

#define i_index_public_PROTO(X, R, ...)
#define i_index_public_METHOD(X, R, ...)                R (*emit_idx_symbol(index,__VA_ARGS__))(X, ##__VA_ARGS__);

#define i_index_intern_F(X, R, ...)
#define i_index_intern_F_EXTERN(X, R, ...)
#define i_index_intern_ISIZE(X, R, ...)
#define i_index_intern_INST_U(X, R, ...)
#define i_index_intern_INST_L(X, R, ...)
#define i_index_intern_INST_U_EXTERN(X, R, ...)
#define i_index_intern_INST_L_EXTERN(X, R, ...)
#define i_index_intern_DECL(X, R, ...)
#define i_index_intern_DECL_EXTERN(X, R, ...)
#define i_index_intern_GENERICS(X, R, ...)
#define i_index_intern_INIT(X, R, ...)
#define i_index_intern_PROTO(X, R, ...)
#define i_index_intern_METHOD(X, R, ...)                R (*emit_idx_symbol(index, __VA_ARGS__))(X, ##__VA_ARGS__);
#define i_index(X, Y, T, R, ...)                        i_index_##T##_##Y(X, R, ##__VA_ARGS__)

#define i_vargs_public_F(X, R, N, ...)
#define i_vargs_public_F_EXTERN(X, R, N, ...)
#define i_vargs_public_ISIZE(X, R, N, ...)
#define i_vargs_public_ISIZE_EXTERN(X, R, N, ...)
#define i_vargs_public_INST_U(X, R, N, ...)
#define i_vargs_public_INST_L(X, R, N, ...)
#define i_vargs_public_INST_U_EXTERN(X, R, N, ...)
#define i_vargs_public_INST_L_EXTERN(X, R, N, ...)
#define i_vargs_public_DECL(X, R, N, ...)               R X##_##N(X  __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs_public_DECL_EXTERN(X, R, N, ...)        R X##_##N(X, __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs_public_GENERICS(X, R, N, ...)
#define i_vargs_public_INIT(X, R, N, ...)               i_method_public_INIT(X, R, N, __VA_ARGS__)  
#define i_vargs_public_PROTO(X, R, N, ...)
#define i_vargs_public_METHOD(X, R, N, ...)             R (*N)(__VA_ARGS__, ...);

#define i_vargs_intern_F(X, R, N, ...)
#define i_vargs_intern_F_EXTERN(X, R, N, ...)
#define i_vargs_intern_ISIZE(X, R, N, ...)
#define i_vargs_intern_INST_U(X, R, N, ...)
#define i_vargs_intern_INST_L(X, R, N, ...)
#define i_vargs_intern_INST_U_EXTERN(X, R, N, ...)
#define i_vargs_intern_INST_L_EXTERN(X, R, N, ...)
#define i_vargs_intern_DECL(X, R, N, ...)               static R X##_##N(X __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs_intern_DECL_EXTERN(X, R, N, ...)
#define i_vargs_intern_GENERICS(X, R, N, ...)
#define i_vargs_intern_INIT(X, R, N, ...)               i_method_intern_INIT(X, R, N, __VA_ARGS__)  
#define i_vargs_intern_PROTO(X, R, N, ...)
#define i_vargs_intern_METHOD(X, R, N, ...)             R (*N)(X __VA_OPT__(,) __VA_ARGS__, ...);
#define i_vargs(X, Y, T, R, N, ...)                     i_vargs_##T##_##Y(X, R, N, __VA_ARGS__)

#define s_vargs_public_F(X, R, N, ...)
#define s_vargs_public_F_EXTERN(X, R, N, ...)
#define s_vargs_public_ISIZE(X, R, N, ...)
#define s_vargs_public_ISIZE_EXTERN(X, R, N, ...)
#define s_vargs_public_INST_U(X, R, N, ...)
#define s_vargs_public_INST_L(X, R, N, ...)
#define s_vargs_public_INST_U_EXTERN(X, R, N, ...)
#define s_vargs_public_INST_L_EXTERN(X, R, N, ...)
#define s_vargs_public_DECL(X, R, N, ...)               R N(__VA_ARGS__, ...);
#define s_vargs_public_DECL_EXTERN(X, R, N, ...)        R N(__VA_ARGS__, ...);
#define s_vargs_public_GENERICS(X, R, N, ...)
#define s_vargs_public_INIT(X, R, N, ...)               s_method_public_INIT(X, R, N, ##__VA_ARGS__)
#define s_vargs_public_PROTO(X, R, N, ...)
#define s_vargs_public_METHOD(X, R, N, ...)

#define s_vargs_intern_F(X, R, N, ...)
#define s_vargs_intern_F_EXTERN(X, R, N, ...)
#define s_vargs_intern_ISIZE(X, R, N, ...)
#define s_vargs_intern_INST_U(X, R, N, ...)
#define s_vargs_intern_INST_L(X, R, N, ...)
#define s_vargs_intern_INST_U_EXTERN(X, R, N, ...)
#define s_vargs_intern_INST_L_EXTERN(X, R, N, ...)
#define s_vargs_intern_DECL(X, R, N, ...)               R N(__VA_ARGS__, ...);
#define s_vargs_intern_DECL_EXTERN(X, R, N, ...)
#define s_vargs_intern_GENERICS(X, R, N, ...)
#define s_vargs_intern_INIT(X, R, N, ...)               s_method_intern_INIT(X, R, N, ##__VA_ARGS__)
#define s_vargs_intern_PROTO(X, R, N, ...)
#define s_vargs_intern_METHOD(X, R, N, ...)             
#define s_vargs(X, Y, T, R, N, ...)                     s_vargs_##T##_##Y(X, R, N, ##__VA_ARGS__)
#define t_vargs(X, Y, T, R, N, ...)                     s_vargs_##T##_##Y(X, R, N, Au_t, __VA_ARGS__)

#define i_override_method_F(X, N)
#define i_override_method_F_EXTERN(X, N)
#define i_override_method_ISIZE(X, N)
#define i_override_method_ISIZE_EXTERN(X, N)
#define i_override_method_INST_U(X, N)
#define i_override_method_INST_L(X, N)
#define i_override_method_INST_U_EXTERN(X, N)
#define i_override_method_INST_L_EXTERN(X, N)
#define i_override_method_DECL(X, N)
#define i_override_method_DECL_EXTERN(X, N)
#define i_override_method_GENERICS(X, N)
#define i_override_method_INIT(X, N) { \
    Au_t m = def((Au_t)&X##_i.type, #N, AU_MEMBER_FUNC, AU_TRAIT_OVERRIDE); \
    m->alt = #X "_" #N; \
    m->access_type = interface_undefined; \
    type_ref->ft.N = (__typeof__(type_ref->ft.N))& X## _ ## N; \
    m->value = (object)& X##_##N; \
    member_override((Au_t)type_ref, m, AU_MEMBER_FUNC); \
}

#define i_override_method_PROTO(X, N)
#define i_override_method_METHOD(X, N)

#define i_override_ctr_F(X, R)
#define i_override_ctr_F_EXTERN(X, R)
#define i_override_ctr_ISIZE(X, R)
#define i_override_ctr_ISIZE_EXTERN(X, R)
#define i_override_ctr_INST_U(X, R)
#define i_override_ctr_INST_L(X, R)
#define i_override_ctr_INST_U_EXTERN(X, N)
#define i_override_ctr_INST_L_EXTERN(X, N)
#define i_override_ctr_DECL(X, R)
#define i_override_ctr_GENERICS(X, N)                   N: X##_i.type.ft.with_##N,
#define i_override_ctr_INIT(X, R) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(with_##R), AU_MEMBER_CONSTRUCT, AU_TRAIT_OVERRIDE); \
    m->alt = #X "_with_" #R; \
    m->access_type = interface_undefined; \
    type_ref->ft.with_##R = & X##_with_##R; \
    m->value = (object)& X##_with_##R; \
    member_override((Au_t)type_ref, m, AU_MEMBER_CONSTRUCT); \
}

#define i_override_ctr_PROTO(X, R)
#define i_override_ctr_METHOD(X, R)

#define i_override_cast_F(X, R)
#define i_override_cast_F_EXTERN(X, R)
#define i_override_cast_ISIZE(X, R)
#define i_override_cast_ISIZE_EXTERN(X, R)
#define i_override_cast_INST_U(X, R)
#define i_override_cast_INST_L(X, R)
#define i_override_cast_INST_U_EXTERN(X, R)
#define i_override_cast_INST_L_EXTERN(X, R)
#define i_override_cast_DECL(X, R)
#define i_override_cast_GENERICS(X, N)
#define i_override_cast_INIT(X, R) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(cast_##R), AU_MEMBER_CAST, AU_TRAIT_OVERRIDE); \
    m->alt = #X "_cast_" #R; \
    m->access_type = interface_undefined; \
    type_ref->ft.cast_##R = & X##_cast_##R; \
    m->value = (object)& X##_cast_##R; \
    member_override((Au_t)type_ref, m, AU_MEMBER_CAST); \
}


#define i_override_cast_PROTO(X, R)
#define i_override_cast_METHOD(X, R)

#define i_override_idx_F(X, R)
#define i_override_idx_F_EXTERN(X, R)
#define i_override_idx_ISIZE(X, R)
#define i_override_idx_INST_U(X, R)
#define i_override_idx_INST_L(X, R)
#define i_override_idx_INST_U_EXTERN(X, R)
#define i_override_idx_INST_L_EXTERN(X, R)
#define i_override_idx_DECL(X, R)
#define i_override_idx_GENERICS(X, R)
#define i_override_idx_INIT(X, R) { \
    Au_t m = def((Au_t)&X##_i.type, stringify(idx_##R), AU_MEMBER_INDEX, AU_TRAIT_OVERRIDE); \
    m->alt = #X "_idx_" #R; \
    m->access_type = interface_undefined; \
    type_ref->idx_##R = & X##_idx_##R; \
    m->value = (object)& X##_idx_##R; \
    member_override(type_ref, m, AU_MEMBER_INDEX); \
}

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


//#define M(X, Y, VISIBILITY, NAME, ...) \
//    i_ ## NAME ## _ ## VISIBILITY ## _ ## Y (X, __VA_ARGS__)


#define backwards(container, E, e) \
    if (container && len((container))) for (E e = peek(((collective)container), len((container)) - 1), e0 = 0; e0 == 0; e0++) \
        for (num __i = len((container)); __i >= 0; __i--, e = __i >= 0 ? (E)peek(((collective)container), __i) : (E)null) \

/// we can iterate through a collection with this strange code
#define each(container, E, e) \
    if (container && len((container))) for (E e = (E)peek(((collective)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = len((container)); __i < __len; __i++, e = (E)peek(((collective)container), __i)) \

#define each_(container, E, e) \
    if (container && len((container))) for (E e = (E)peek(((collective)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = len((container)); __i < __len; __i++, e = (E)peek(((collective)container), __i)) \

#define values(container, E, e) \
    if (container && len((container))) for (E e = *(E*)peek(((collective)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = len((container)); __i < __len; __i++, e = *(E*)peek(((collective)container), __i)) \
    
/// we can go through a map
#define pairs(MM, EE) \
    for (item EE = (MM && MM->first) ? MM->first : (item)null; EE; EE = EE->next)


#define         form(T, t, ...)   (T)formatter(typeid(T), null,   (Au)false, (symbol)t, ## __VA_ARGS__)
#define            f(T, t, ...)   (T)formatter(typeid(T), null,   (Au)false, (symbol)t, ## __VA_ARGS__)
#define         exec(t, ...)      command_exec(((command)formatter((Au_t)typeid(command), null, (Au)false, (symbol)t, ## __VA_ARGS__)))
#define          run(t, ...)      command_run(((command)formatter((Au_t)typeid(command), null, (Au)false, (symbol)t, ## __VA_ARGS__)))
#define         vexec(n, t, ...)     verify(exec((string)t __VA_OPT__(,) __VA_ARGS__) == 0, "shell command failed: %s", n);

#define        Au_log(sL, t, ...)   formatter((Au_t)null, stdout, (Au)string(sL),  t, ## __VA_ARGS__)

#define          put(t,    ...)   formatter((Au_t)null,      stdout, (Au)false, (symbol)t, ## __VA_ARGS__)
//#define        print(L, t,    ...) formatter((Au_t)null,      stdout, (Au)true,  (symbol)t, ## __VA_ARGS__)
#define        error(t, ...)      formatter((Au_t)null,      stderr, (Au)true,  (symbol)t, ## __VA_ARGS__)


#define print(t, ...)   ({\
    static string _topic = null; \
    if (!_topic) _topic = (string)hold((Au)new(string, __func__)); \
    formatter((Au_t)null, stdout, (Au)_topic, t, ## __VA_ARGS__); \
})

#define fault(t, ...) do {\
    static string _topic = null; \
    if (!_topic) _topic = (string)Au_hold((Au)new(string, __func__)); \
     string res = (string)formatter((Au_t)null, stderr, (Au)_topic,  (symbol)t, ## __VA_ARGS__); \
     halt(res); \
    } while(0)


#define  file_exists(t, ...)     (resource_exists(formatter((Au_t)null, null, (Au)false, (symbol)t, ## __VA_ARGS__)) == Exists_file)
#define   dir_exists(t, ...)     (resource_exists(formatter((Au_t)null, null, (Au)false, (symbol)t, ## __VA_ARGS__)) == Exists_dir)
#ifndef NDEBUG
#define       assert(a, t, ...) do { if (!(a)) { formatter((Au_t)null, stderr, (Au)true,  t, ## __VA_ARGS__); exit(1); } } while(0)
#else
#define       assert(a, t, ...) do { } while(0)
#endif
#define       verify(a, t, ...) ({ if (!(a)) { string res = (string)formatter((Au_t)null, stderr, (Au)true,  (symbol)t, ## __VA_ARGS__); if (level_err >= fault_level) { halt(res); } false; } else { true; } true; })

#undef min
#undef max

#define sqr(Au) ({ \
    __typeof__(Au) a = A; \
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

#define mset(m, k, v) set(m, (Au)string(k), (Au)v)
#define mget(m, k)    get(m, (Au)string(k))

/// possible to iterate safely through primitives
#define primitives(arr, E, e) \
    if (len(arr)) for (E e = *(E*)peek(arr, 0), e0 = 0; e0 == 0; e0++) \
        for (num i = 0, __len = len(arr); i < __len; i++, e = *(E*)peek(arr, i)) \

#define head(o) header((Au)o)
#define Au_struct(T) (T*)alloc(typeid(u8), sizeof(T), (Au_t*)null)
#define     e_str(E,I) estring(typeid(E), I)
#define     e_val(E,S) evalue (typeid(E), S)

#endif