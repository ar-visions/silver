#include <stdio.h>

int print_intp(int *p) {
    printf("p = %d\n", *p);
}

int main() {
    int temp;
    print_intp(&(temp = 1));
    return 0;
}