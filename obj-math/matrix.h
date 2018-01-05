
class Matrix44 {
    override copy(C);
    C mul(C, Matrix44);
    C with_v4(Vec4);
    Vec4 mul_v4(C,Vec4);
    C translate(C,Vec3);
    C scale(C,Vec3);
    float get_xscale(C);
    float get_yscale(C);
    C ident();
    C ortho(float,float,float,float,float,float);
    C rotate(C,float,Vec3);
    C invert(C);
    Vec3 unproject(C,Vec3,Vec4);
    Vec3 project(Vec3,Matrix44,Matrix44,Vec4);
    private float m[16];
};