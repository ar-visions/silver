#include <obj.h>

implement(Prop)

typedef struct _TypeAssoc {
    const char *type;
    const char *primitive;
} TypeAssoc;

static TypeAssoc assoc[] = {
    { "Bool",       "bool" },
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

Prop Prop_new_with(char *type, char *name, Getter getter, Setter setter) {
    const char *type_ = var_to_obj_type(type);
    if (!type_)
        type_ = type;
    Class c = class_find(type_);
    Type t = enum_find(Type, type_);
    if (!c && !t)
        return NULL;
    Prop self = new(Prop);
    self->name = new_string(name);
    self->enum_type = (Enum)t;
    self->class_type = c;
    self->getter = getter;
    self->setter = setter;
    return self;
}
