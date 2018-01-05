
class Vec {
    C add(C,C);
    C sub(C,C);
    C scale(C,double);
    C mul(C,C);
    double dot(C,C);
    C with(Class,...);
    C with_count(int);
    double length(C);
    override void init(C);
    override C from_cstring(const char *);
    override String to_string(C);
    override int compare(C,C);
    override ulong hash(C);
    private int count;
    private double * vec;
    double x;
    double y;
};

class Vec3 : Vec {
    override void init(C);
    double z;
};

class Vec4 : Vec3 {
    override void init(C);
    double w;
};

#define Vec2 Vec
#define Vec2_cl Vec_cl

#define vec2(X,Y)        ((Vec2)class_call(Vec, with, class_object(Vec2),(double)(X),(double)(Y)))
#define vec3(X,Y,Z)      ((Vec3)class_call(Vec, with, class_object(Vec3),(double)(X),(double)(Y),(double)(Z)))
#define vec4(X,Y,Z,W)    ((Vec4)class_call(Vec, with, class_object(Vec4),(double)(X),(double)(Y),(double)(Z),(double)(W)))

#define vadd(A,B)        ((typeof(A))call((A), add, B))
#define vsub(A,B)        ((typeof(A))call((A), sub, B))
#define vmul(A,B)        ((typeof(A))call((A), mul, B))
#define vscale(A,B)      ((typeof(A))call((A), scale, B))
