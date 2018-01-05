class Mixable {
    C mix_with(C,C,double);
};

class Color : Mixable {
    override void,init(C);
    override C mix_with(C,C,double);
    C new_rgba(double,double,double,double);
    double r;
    double g;
    double b;
    double a;
};

class Fill : Mixable {
    override void init(C);
    override C mix_with(C,C,double);
    delegate Color color;
};

#define new_rgba(r,g,b,a)   (Color_cl->new_rgba(r,g,b,a))
#define rgba(r,g,b,a)       (autorelease(new_rgba(r,g,b,a)))
