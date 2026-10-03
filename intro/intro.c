/* C companion: bodies for the intern funcs in intro.ag */
void fill(int* v, int n, int start) {
    for (int i = 0; i < n; i++)
        v[i] = start + i;
}
