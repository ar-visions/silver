#pragma once
#include <algorithm>

template <typename T>
struct pairT {
    T a;
    T b;

    T sum() const { return a + b; }

    T scale(T k) const { return (a + b) * k; }

    T operator*(T k) const { return (a + b) * k; }

    T operator+(const pairT& o) const { return (a + b) + (o.a + o.b); }

    T operator-(pairT o) const { return (a + b) - (o.a + o.b); }

    bool operator==(const pairT& o) const { return a == o.a && b == o.b; }
};

template struct pairT<int>;
template struct pairT<float>;

template <typename T, int N>
struct scaled {
    T v;

    T out() const { return v * N; }
};

template struct scaled<int, 8>;
template struct scaled<float, 3>;

template <typename A, typename B>
struct duo {
    A first;
    B second;

    A wide() const { return (A)(first + (A)second); }
};

template struct duo<int, float>;

struct Beast {
    int legs;

    Beast(int l) : legs(l) {}

    virtual int roar() const { return 1; }

    int stride() const { return legs * 2; }
};

struct Wolf : Beast {
    Wolf() : Beast(4) {}

    int roar() const override { return 42; }
};

inline Beast* make_beast() { return new Beast(2); }
inline Beast* make_wolf()  { return new Wolf(); }

struct Ratio {
    int num;
    int den;

    Ratio(int n, int d) : num(n), den(d) {}

    Ratio(int n) : num(n), den(1) {}

    Ratio(float f) : num((int)(f * 100.0f)), den(100) {}

    operator float() const { return (float)num / (float)den; }

    int floor_div() const { return num / den; }
};

namespace geo2 {

struct rect2 {
    int w;
    int h;

    rect2(int w_, int h_) : w(w_), h(h_) {}

    int area() const { return w * h; }

    rect2 operator+(const rect2& o) const { return rect2(w + o.w, h + o.h); }
};

namespace inner {

struct point2 {
    int x;
    int y;

    point2(int x_, int y_) : x(x_), y(y_) {}

    int manhattan() const { return (x < 0 ? -x : x) + (y < 0 ? -y : y); }
};

}

}
