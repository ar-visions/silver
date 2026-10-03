#pragma once

// a C++ template silver uses as a type
template <typename T>
struct pair2 {
    T a;
    T b;

    T sum() const { return a + b; }
};

template struct pair2<int>;
template struct pair2<float>;
