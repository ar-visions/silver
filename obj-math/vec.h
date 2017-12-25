#ifndef _VEC_
#define _VEC_

#define _Vec(D,T,C) _Base(spr,T,C)                   \
    method(D,T,C,C,add,(C,C))                        \
    method(D,T,C,C,sub,(C,C))                        \
    method(D,T,C,C,scale,(C,double))                 \
    method(D,T,C,C,mul,(C,C))                        \
    method(D,T,C,double,dot,(C,C))                   \
    method(D,T,C,C,with,(Class,...))                 \
    method(D,T,C,C,with_count,(int))                 \
    method(D,T,C,double,length,(C))                  \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,C,from_cstring,(const char *))    \
    override(D,T,C,String,to_string,(C))             \
    override(D,T,C,int,compare,(C,C))                \
    override(D,T,C,ulong,hash,(C))                   \
    var(D,T,C,int,count)                             \
    var(D,T,C,double *,vec)                          \
    var(D,T,C,double,x)                              \
    var(D,T,C,double,y)
declare(Vec, Base)

#define _Vec3(D,T,C) _Vec(spr,T,C)                   \
    override(D,T,C,void,init,(C))                    \
    var(D,T,C,double,z)
declare(Vec3, Vec)

#define _Vec4(D,T,C) _Vec3(spr,T,C)                  \
    override(D,T,C,void,init,(C))                    \
    var(D,T,C,double,w)
declare(Vec4, Vec3)

#define Vec2 Vec
#define Vec2_cl Vec_cl

#define vec2(X,Y)        ((Vec2)Vec_with(class_object(Vec2),(double)(X),(double)(Y)))
#define vec3(X,Y,Z)      ((Vec3)Vec_with(class_object(Vec3),(double)(X),(double)(Y),(double)(Z)))
#define vec4(X,Y,Z,W)    ((Vec4)Vec_with(class_object(Vec4),(double)(X),(double)(Y),(double)(Z),(double)(W)))

#define vadd(A,B)        ((typeof(A))call((A), add, B))
#define vsub(A,B)        ((typeof(A))call((A), sub, B))
#define vmul(A,B)        ((typeof(A))call((A), mul, B))
#define vscale(A,B)      ((typeof(A))call((A), scale, B))

#endif
