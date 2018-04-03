module supermod;

class Test : Super {
    construct () {
        printf("default constructor\n");
    }
    construct i(int arg) {
        self.value = arg;
    }
    construct s(String arg) {
        self.value = 1;
    }
    int test1 = 1;
    int value_intern;
    static int value_test {
        get {
            return 1;
        }
        set (value) {
            do_nothing;
        }
    }
    int value {
        get {
            return self.value_intern;
        }
        set (value) {
            self.value_intern = value;
        }
    } = 2;
    void method(int arg, int arg2) {
        super.method(arg, arg2);
        printf("value = %d\n", self.value);
        Test t = new Test();
    }
    static int Main() {
        Test t = new Test();
        t.method(1, 2);
        t.value = 1;
        return 0;
    }
}