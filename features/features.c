/* C companion: bodies for the intern funcs declared in features.ag */
int c_add(int a, int b) {
    return a + b;
}

int c_scale(int a, int b) {
    return a * b;
}

#include <stdio.h>
#include <unistd.h>

/* resident memory in KB — a 10MB block is mmap'd, so a real free
   returns it to the OS and this number falls */
long c_rss_kb(void) {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long total = 0, res = 0;
    if (fscanf(f, "%ld %ld", &total, &res) != 2) res = 0;
    fclose(f);
    return res * (sysconf(_SC_PAGESIZE) / 1024);
}

/* takes a silver string as cstr, returns its length */
int c_len(const char* s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

/* writes through a pointer the caller owns */
void c_fill(int* out, int n, int base) {
    for (int i = 0; i < n; i++) out[i] = base + i;
}
