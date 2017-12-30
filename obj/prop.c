#include <obj/obj.h>

implement(Prop)

typedef struct _TypeAssoc {
    const char *type;
    const char *primitive;
} TypeAssoc;

static Pairs prop_meta = NULL;

static TypeAssoc assoc[] = {
    { "Boolean",    "bool" },
    { "Int8",       "char" },
    { "UInt8",      "unsigned char" },
    { "Int16",      "short" },
    { "UInt16",     "unsigned short" },
    { "Int32",      "int" },
    { "UInt32",     "unsigned int" },
    { "Long",       "long" },
    { "ULong",      "unsigned long" },
    { "Float",      "float" },
    { "Double",     "double" }
};

const char *var_to_obj_type(char *vtype) {
    for (int i = 0; i < sizeof(assoc) / sizeof(TypeAssoc); i++) {
        TypeAssoc *ta = &assoc[i];
        if (strcmp(ta->primitive, vtype) == 0)
            return ta->type;
    }
    return NULL;
}

Prop Prop_new_with(Class cl, char *type, char *name, Getter getter, Setter setter, char *meta) {
    const char *type_ = var_to_obj_type(type);
    if (!type_)
        type_ = type;
    Class c = class_find(type_);
    Type t = enum_find(Type, type_);
    Type t_object = enum_find(Type, "Object");
    if (!c && !t)
        return NULL;
    Prop self = new(Prop);
    self->prop_of = cl;
    self->name = new_string(name);
    self->enum_type = t ? (Enum)t : (c ? (Enum)t_object : NULL);
    self->class_type = c;
    self->getter = getter;
    self->setter = setter;
    if (meta) {
        self->meta = class_call(Pairs, from_cstring, meta);
        if (self->meta) {
            if (!prop_meta)
                prop_meta = new(Pairs);
            String smeta = string(meta);
            List list = pairs_value(prop_meta, smeta, List);
            if (!list) {
                list = auto(List);
                pairs_add(prop_meta, smeta, list);
            }
            list_push(list, self);
            // if meta is specified, add this property to a class-based Pairs with same key
        }
    }
    return self;
}
