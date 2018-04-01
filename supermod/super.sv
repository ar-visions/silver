include stdlib;

class Super : Base {
    int test_me = 1;
    void super_only(int test) {
        printf("super only");
    }
    void method(int arg, int arg2) {
        printf("original function: %d", self.test_me);
    }
}