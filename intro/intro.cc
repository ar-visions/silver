// C++ companion: silver links by the plain symbol name
#include <numeric>

extern "C" int total(const int* v, int n) {
    return std::accumulate(v, v + n, 0);
}
