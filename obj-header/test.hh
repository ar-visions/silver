class Test : SuperTest {
    override void init();

    [key:true key_isolate key2:"string"]
    int var_test; //...
}

enum EnumTest {
    Enum1 = 10,
    Enum2 = 30,
    Enum3,
    Enum4
}