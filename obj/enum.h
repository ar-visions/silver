forward Pairs;

class Enum {
    C find(Class,const char *);
    Pairs enums(Class);
    override String to_string(C);
    override void class_preinit(Class);
    override void free(C);
    String symbol;
    int ordinal;
};

#define enum_declare(C,S)                  \
    enum C {                               \
        _##C(cls,enum_def,C)               \
    };                                     \
    declare(C##Enum,S)

enum Type {
    Object = 0,
    Boolean,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Long,
    ULong,
    Float,
    Double
};

#define enum_find(C,N)  ((C)class_call(Enum, find, (Class)class_object(C##Enum), N));
#define enums(C)        (Enum_enums((Class)class_object(C##Enum)))

extern bool enum_init;
