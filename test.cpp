#include <stdio.h>

class A {
public:
    A(int) { }
};

class B {
public:
    B(A) { }
};

class C {
public:
    C(B) { }
};

void func(C c) {
    // Function implementation
}

int main() {
    int x = 42;
    func(x);  // Implicit conversion: int -> A -> B -> C
    return 0;
}