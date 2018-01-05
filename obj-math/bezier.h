#define bezier_B1(t) (t*t*t)
#define bezier_B2(t) (3*t*t*(1-t))
#define bezier_B3(t) (3*t*(1-t)*(1-t))
#define bezier_B4(t) ((1-t)*(1-t)*(1-t))

class Bezier {
    override String to_string(C);
    float2 degree(float2 *,int,float,float2 *, float2 *);
    int crossing_count(float2 *,int);
    int flat_enough(float2 *,int);
    float x_intercept(float2 *,int);
    int find_roots(float2 *, int, float *, int);
    void bezier_form(float2, float2 *, float2 *);
    float2 nearest_point(C, float2);
    float2 point_at(C,float);
    float percent_from_point(C,float2);
    void split(C,C *,C *,float);
    float approx_length(C,int);
    void closest_point(C, float2, float2 *, float *);
    float2 p1;
    float2 h1;
    float2 h2;
    float2 p2;
};

#define new_bezier(P1,H1,H2,P2)     (class_call(Bezier, new_bezier, (P1), (H1), (H2), (P2)))
#define bezier(P1,H1,H2,P2)         (autorelease(new_bezier(P1,H1,H2,P2)))
