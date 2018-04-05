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
        Super.super_static();
        printf("value = %d\n", self.value);
    }

    Test *gen_array(int count) {
        Test *ret = (Test *)malloc(sizeof(Test *) * count);
        for (int i = 0; i < count; i++) {
            ret[i] = new Test();
        }
        return ret;
    }

    static int main(Array args) {
        String str1 = (String)args.buffer[0];
        for (int i = 0; i < args.count; i++) {
            String str = (String)args.buffer[i];
            printf("arg[%d] = %s\n", i, str->buffer);
        }
        
        Test t = new Test();
        if (Test.instance((Base)str1)) {
            printf("test inherits\n");
        }
        t.method(1, 2);
        t.value = 1;
        Test *a = t.gen_array(10);
        printf("value = %d\n", t.value);
        return 0;
    }
}
