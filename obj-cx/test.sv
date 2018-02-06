module outside as o

// load external module dynamically or statically
// module constructor will simply wait on all of the other modules that it depends on to load, then grab their classes

class Super {
    void super_only(int test) {
        printf("super only");
    }
    void method(int arg, int arg2) {
        printf("original function");
    }
}

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
    int value {
        get {
            return self.value;
        }
        set (value) {
            return self.value;
        }
    } = 2;
    void method(int arg, int arg2) {
        printf("value = %d\n", self.value);
        Test t = new Test(test1,test2);
    }
    static int Main() {
        return 0;
    }
}