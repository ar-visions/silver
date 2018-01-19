

// stack object initialization
// keyword on method to indicate constructor method
// be able to use constructor methods on stack or pointer allocations
// from and to?

// ** pool { blocks } for autorelease sections
// ** try, throw, catch, and finally keywords
// ** stack mode allocation for objects
// ** add construct to structs with collection of members and their offset
//         -> (offset will be difficult to know due to arbitrary compiler padding)
//      allow operator overloading on structs
//      implicit conversion of +=
// ** serialize structs with introspection of constructors
// ** while its not possible to get individual property members on structs due to alignment, you can serialize them one by one

struct Test struct_Test_construct_int(Test *self, int p) {
    // memset(self, 0, sizeof(Test)); <-- replace with simple stack Struct str = { };
    // code output
}

// automatic casting if object inherits from object required
// problem will be generating inline uses of structs such as 

struct Test {
    int a, b, c;

    construct(int p) {
        self.a = p;
        self.b = p;
        self.c = p;
    }

    // output as inlines to avoid copies; allow ability to be turned off to avoid code bloat
    // pointers NOT allowed in operators

    inline Test operator + (Test left, int right) {
        return (Test) { left.x + right, left.y + right, left.z + right };
    }

    inline Test operator / (Test left, int right) {
        return (Test) { left.a / right, left.b / right, left.c / right };
    }

    inline Test operator / (int left, Test right) {
        return (Test) { left / right.a, left / right.b, left / right.c };
    }
};

// allow stapling extra conversion methods on; such as this one to facilitate serialization of Test struct to String
class String {
    construct(Test *t) {
        self.format("%f,%f,%f", t->x, t->y, t->z);
    }
}

class Vector <double, float> {
    construct(int i) {
        // automatic construction of self; either stack or pointer
        self->i = i;

        // stack mode initialization
        //      can NOT perform new struct(args)
        //      -> will work with structs as well
        //      -> operator overloads only on structs
        //          due to the fact that pointer math must always remain

        // constructors for type <-> type conversion

        // pool blocks for fetching new pool and draining

        // 
        pool {

        }

        return self;
    }
    private int test ^[key:value] {
        get {
            return self.that;
        }
    }
    String method(int arg) {
        return (String)self; // <-- conversion method called; these conversion methods are a different animal than operator overloads in that they allow return
    }
}
