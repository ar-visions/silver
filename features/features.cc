// C++ companion: bodies for the intern funcs declared in features.ag.
// extern "C" is required — silver links by the plain symbol name
#include <vector>
#include <import>

// a class method declared without a body in features.ag: its body is here
extern "C" none Filled_init(Filled a) { a->n = 7; }

extern "C" int cc_mul(int a, int b) {
    return a * b;
}

extern "C" int cc_pow2(int a) {
    return a * a;
}

// uses the C++ standard library, hands back a plain value
extern "C" int cc_sum(const int* v, int n) {
    std::vector<int> xs(v, v + n);
    int total = 0;
    for (int x : xs) total += x;
    return total;
}
