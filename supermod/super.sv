include stdlib;

class Super {
    int test_me = 1;
    void super_only(int test) {
        printf("super only");
    }
    void method(int arg, int arg2) {
        int test_me = self.test_me;
        printf("super function: %d\n", test_me);
    }
    static void super_static() {
        printf("this method is static, but still there's context: %s\n", class.name);
    }
}
