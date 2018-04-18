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
    void Test_main2(testmod_TestClass class, base_Array args) {
        testmod_Test b  = (testmod_Test)(Test->retain(
            (testmod_Test)set_int((base_Base)(
                (testmod_Test)set_int((base_Base)((testmod_Test)
                    Base->new_object(Base, (base_Class)testmod_Test_var, 0)),
                    Test->set_value,
                    1
                )), Test->set_another, 2)
            )
        );
        Test->release(b);
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
        return 0;
    }
}
