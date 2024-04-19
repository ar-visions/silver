#include <silver/silver.hpp>

int main(int argc, char** argv) {
    silver::init();
    map *m = new map;

    test *t = new test;
    t->member1 = 1;
    t->member2 = 2;
    u64 hash = t->hash_value();

    map* m = new map;
    m.set(t, new obj(10));

    test *t2 = new test;
    t->member1 = 1;
    t->member2 = 2;
    
    test* m0 = m->get(t2);

    /// should be js-like, meaning the hash is based on identity of the object
    /// for strings, the identity is the contents
    /// for objects, the identity is the memory unless the object overrides this
    /// that is more js-like
    /// of course, its not exactly model-like however a 'model' could implement hash the same way as obj

    test *t3 = new test;
    t->member1 = 1;
    t->member2 = 1;
    test* m1 = m->get(t3);


    return 0;
}
