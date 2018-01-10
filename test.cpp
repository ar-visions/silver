#include <cassert>
#include <iostream>

class C {
    public:
        int i;
        C(int i) : i(i) {}
        int m(int j) { return this->i + j; }
};

int main() {
    // Get a method pointer.
    int (C::*p)(int) = &C::m;

    int C::*ip = &C::i;

    // Create a test object.
    C c(1);
    C *cp = &c;

    // Operator .*
    assert((c.*p)(2) == 3);

    // Operator ->*
    assert((cp->*p)(2) == 3);

    assert(cp->*ip == 1);

    c.set_property("i", any_value);
    string *s = c.get_prop<string>("i");

    std::cout << cp->*ip << "\n";
}