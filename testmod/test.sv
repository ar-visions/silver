module supermod;

class Test : Super {
    
    private int value_intern;

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
    }

    static int Main() {
        Test t = new Test();
        t.method(1, 2);
        t.value = 1;
        return 0;
    }
}
