#include <stdio.h>

int main() {
    int[] i = [1, 2, 3, 4, 5];              // Array
    int[string] p = ["key":1, "key2":2];    // Pairs
    p["str_value"] = 1;
    int test_int = 1;
    long l = test_int.hash();
    int test = p["str_value"];
    printf("1\n");
    i.push(1);
    int z = i.pop();

    each(key, value in p) {
        String.print(console, "")
    }
    return 0;
}