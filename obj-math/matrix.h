#ifndef _MATRIX44_
#define _MATRIX44_

#define _Matrix44(D,T,C) _Base(spr,T,C)              \
    method(D,T,C,C,mul,(C,Matrix44))                 \
    method(D,T,C,C,with_v4,(Vec4))                   \
    method(D,T,C,Vec4,mul_v4,(C,Vec4))               \
    method(D,T,C,C,translate,(C,Vec3))               \
    method(D,T,C,C,scale,(C,Vec3))                   \
    method(D,T,C,float,get_xscale,(C))               \
    method(D,T,C,float,get_yscale,(C))               \
    method(D,T,C,C,ident,())                         \
    method(D,T,C,C,ortho,(float,float,float,float,float,float)) \
    method(D,T,C,C,rotate,(C,float,Vec3))            \
    method(D,T,C,C,invert,(C))                       \
    method(D,T,C,Vec3,unproject,(C,Vec3,Vec4))       \
    method(D,T,C,Vec3,project,(Vec3,Matrix44,Matrix44,Vec4)) \
    private_var(D,T,C,float,m[16])
declare(Matrix44, Base)

#endif
