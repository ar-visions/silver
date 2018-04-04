include stdlib;

class Super : Base {
    int test_me = 1;
    void super_only(int test) {
        printf("super only");
    }
    void method(int arg, int arg2) {
        printf("super function: %d\n", self.test_me);
    }
    static void super_static() {
        printf("this method is static, but still there's context: %s\n", self.name);
        int a;
        typedef struct {
            int a;
        } stack;
        Future f = new Future();
        self.method2((arg) ^{
            # 20 "super.sv"
            printf("hi: %d", a);
            f.complete(true);
        });
        return f;
    }
}
