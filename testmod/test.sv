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

    Test cast(char *input) {
        return new Test(value:input arg:1);
    }

    void method(int arg, int arg2) {
        super.method(arg, arg2);
        Super.super_static();
        printf("value = %d\n", self.value);
    }

    static int main(Array args) {
        Test b = new Test();
        Array a = new Array();
        
        int i = (int)b;

        a.push(b);
        String str1 = String.instance(args.buffer[0]);
        for (int i = 0; i < args.count; i++) {
            String str = String.instance(args.buffer[i]);
            printf("arg[%d] = %s\n", i, str.buffer);
        }
        Test t = new Test();
        Test failed = t.release();
        if (Test.instance(str1)) {
            printf("test inherits\n");
        }
        t.method((Test)1, 2);
        t.value = 1;
        printf("value = %d\n", t.value);
        return 0;
    }
}
