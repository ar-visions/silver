# 1 "test.c"

typedef struct _Obj {
    int that;
} Obj;

int test_A(int a);
int test_B(int b);

int test(Obj *self) {

    return self->that;
}

int main() {
    Obj obj = {
        .that = 2
    };
    return test(&obj);
}

# 22 "test.c"
int test_A(int a) {
    int t = 0;
    t++;
    return t;
}
# 22 "test.c"
int test_B(int b) {
    int t = 0;
    t++;
    return t;
}