/* C companion: bodies for the intern funcs declared in features.ag */
int c_add(int a, int b) {
    return a + b;
}

int c_scale(int a, int b) {
    return a * b;
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
