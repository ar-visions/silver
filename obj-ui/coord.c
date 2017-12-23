#include <obj-ui/ui.h>

implement(Align)
implement(Coord)

double Coord_compute(Coord self, double start, double rel_start, double end, double rel_end) {
    double s = self->relative ? rel_start : start;
    double e = self->relative ? rel_end : end;
    if (s > e) {
        double t = s;
        s = e;
        e = t;
    }
    double v = self->scale ? self->value * (e - s) : self->value;
    switch (self->align) {
        case Align_Start:
            return s + v;
        case Align_End:
            return e - v;
        case Align_Center:
            return s + (e - s) / 2 + v;
    }
    return 0;
}