struct _class_Base;
struct _object_Enum;
struct _object_Prop;
struct _object_String;
struct _object_Pairs;

forward String, Prop, Pairs;

/*
    implicit forward declarations:
    go through all headers, gather up all classes and structs
    If the structs or classes are referenced in a struct or class, automatically prepend forward declarations and replace the declaration with the:
        _struct or _class
*/

class Base {
    void class_preinit(Class);
    void class_init(Class);
    void init(C);
    String identity(C);
    void free(C);
    void print(C, String);
    bool is_logging(C);
    C retain(C);
    void release(C);
    C autorelease(C);
    C copy(C);
    const char * to_cstring(C);
    C from_cstring(const char *);
    String to_string(C);
    C from_string(String);
    void set_property(C,const char *,Base);
    Base get_property(C,const char *);
    Base property_meta(C,const char *,const char *);
    Base prop_value(C, Prop);
    Prop find_prop(Class, const char *);
    int compare(C,C);
    ulong hash(C);
    void serialize(C,Pairs);
    String to_json(C);
    C from_json(Class, String);
}

#define set_prop(O,P,V) (call(O, set_property, P, base(V)))
#define get_prop(O,P,C) (inherits(call(O, get_property, P), C))
#define prop_meta(O,P,M,C) (inherits(call(O, property_meta, P, M), C))
#define props_with_meta(M,C) (class_call(Prop, props_with_meta, class_object(C), M))

#define print(C,...)                                          \
    if (C && call(C, is_logging))                             \
        call(C, print, class_call(String, format, __VA_ARGS__));
