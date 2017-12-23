#ifndef _BEZIER_
#define _BEZIER_

#define bezier_B1(t) (t*t*t)
#define bezier_B2(t) (3*t*t*(1-t))
#define bezier_B3(t) (3*t*(1-t)*(1-t))
#define bezier_B4(t) ((1-t)*(1-t)*(1-t))

#define _Bezier(D,T,C) _Base(spr,T,C)              \
    override(D,T,C,String,to_string,(C))           \
    method(D,T,C,float2,degree,(float2 *,int,float,float2 *, float2 *)) \
    method(D,T,C,int,crossing_count,(float2 *,int)) \
    method(D,T,C,int,flat_enough,(float2 *,int)) \
    method(D,T,C,float,x_intercept,(float2 *,int)) \
    method(D,T,C,int,find_roots,(float2 *, int, float *, int)) \
    method(D,T,C,void,bezier_form,(float2, float2 *, float2 *)) \
    method(D,T,C,float2,nearest_point,(C, float2)) \
    method(D,T,C,float2,point_at,(C,float)) \
    method(D,T,C,float,percent_from_point,(C,float2)) \
    method(D,T,C,void,split,(C,C *,C *,float)) \
    method(D,T,C,float,approx_length,(C,int)) \
    method(D,T,C,void,closest_point,(C, float2, float2 *, float *)) \
    var(D,T,C,float2,p1) \
    var(D,T,C,float2,h1) \
    var(D,T,C,float2,h2) \
    var(D,T,C,float2,p2)
declare(Bezier, Base)

#define new_bezier(P1,H1,H2,P2)     (class_call(Bezier, new_bezier, (P1), (H1), (H2), (P2)))
#define bezier(P1,H1,H2,P2)         (autorelease(new_bezier(P1,H1,H2,P2)))

#endif