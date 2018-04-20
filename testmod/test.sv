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
    int another;

    Test cast(char *input) {
        return new Test();
    }

    void method(int arg, int arg2) {
        super.method(arg, arg2);
        Super.super_static();
        printf("value = %d\n", self.value);
    }

    static void main2(Array args) {
        Test b = new Test(value=1, another=2);
    }

    static int main(Array args) {
        Test b = new Test();
        Array a = new Array();
        String s = (String)"hi";
        int i = (int)b;
        char *str = (char *)s;
        printf("str = %s\n", str);

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
        t.method(1, 2);
        t.value = 1;
        printf("value = %d\n", t.value);

        (int arg) {
            int ii = 0;
            int t = 5;
            printf("hey there %s\n", s.buffer);
            (int arg2) {
                printf("another test:%s %d", s.buffer, t);
            };
        };
        return 0;
    }
}
