#pragma once

template <typename T>
struct minmax {
    T lo;
    T hi;

    T span() { return hi - lo; }

    T operator*(T k) const { return (hi - lo) * k; }

    T operator+(const minmax& b) const { return (hi - lo) + (b.hi - b.lo); }

    T operator-(minmax b) const { return (hi - lo) - (b.hi - b.lo); }
};

template struct minmax<int>;
template struct minmax<float>;

template <typename T, int N>
struct fixed {
    T v;

    T scaled() const { return v * N; }
};

template struct fixed<int, 16>;

struct Animal {
    int legs;

    Animal(int l) : legs(l) {}

    virtual int speak() const { return 1; }

    int walk() const { return legs * 2; }
};

struct Dog : Animal {
    Dog() : Animal(4) {}

    int speak() const override { return 42; }
};

inline Animal* make_animal() { return new Animal(2); }
inline Animal* make_dog()    { return new Dog(); }

namespace geo {

struct box {
    int w;
    int h;

    box(int w_, int h_) : w(w_), h(h_) {}

    box(int s) : w(s), h(s) {}

    box(float s) : w((int)(s * 10.0f)), h((int)(s * 10.0f)) {}

    int area() const { return w * h; }
};

}
