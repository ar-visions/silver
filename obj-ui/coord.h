
enum Align {
    Start = 0,
    End,
    Center
};

class Coord {
    double compute(C,double,double,double,double);
    enum Align align;
    bool relative;
    double value;
    bool scale;
}
