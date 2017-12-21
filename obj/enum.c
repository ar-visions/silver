#include <obj/obj.h>
#include <obj/pairs.h>

implement(Enum)

void Enum_class_preinit(Class cself) {
    if (!Enum_cl->meta) {
        Enum_cl->meta = new(Pairs);
    }
    class_Enum c = (class_Enum)cself;
    Pairs enums = pairs_value(Enum_cl->meta, string("enums"), Pairs);
    if (!enums) {
        enums = new(Pairs);
        pairs_add(Enum_cl->meta, string("enums"), enums);
    }
    if ((Class)c->parent == class_object(Base))
        return;
    String cname = new_string(c->name);
    Pairs class_enums = new(Pairs);
    pairs_add(enums, cname, class_enums);
    int enum_count = 0;
    char **mnames = (char **)c->mnames;
    for (int i = 0; i < c->mcount; i++) {
        char *start = mnames[i];
        if (strchr(start, '*'))
            continue;
        char *mname = strchr(start, ' ');
        if (mname && strncmp(mname, " enum_", 6) == 0)
            enum_count++;
    }
    if (!enum_count)
        return;
    for (int i = 0; i < c->mcount; i++) {
        char *start = mnames[i];
        if (strchr(start, '*'))
            continue;
        char *mname = strchr(start, ' ');
        if (mname && strncmp(mname, " enum_", 6) == 0) {
            mname++;
            char *args = strchr(mname, ' ');
            if (!args)
                continue;
            int type_len = mname - start - 1;
            int name_len = args - mname - 5;
            char *type = (char *)alloc_bytes(type_len + 1);
            char *name = (char *)alloc_bytes(name_len + 1);
            memcpy(type, start, type_len);
            type[type_len] = 0;
            memcpy(name, &mname[5], name_len);
            name[name_len] = 0;
            Enum enum_obj = (Enum)new_obj((class_Base)c, 0);
            if (enum_obj) {
                printf("type: %s, name = %s\n", type, name);
                String str_name = new_string(name);
                enum_obj->symbol = str_name;
                enum_obj->ordinal = (int)(ulong)(c->m[i])();
                pairs_add(class_enums, str_name, enum_obj);
            }
            free(type);
            free(name);
        }
    }
}

Enum Enum_find(Class c, const char *symbol) {
    if (!Enum_cl->meta)
        return NULL;
    String key = new_string("enums");
    Pairs enums = pairs_value(Enum_cl->meta, key, Pairs);
    release(key);
    if (!enums)
        return NULL;
    key = new_string(c->name);
    Pairs e = pairs_value(enums, key, Pairs);
    release(key);
    if (!e)
        return NULL;
    key = new_string(symbol);
    Enum en = (Enum)pairs_value(e, string(symbol), Enum);
    release(key);
    return en;
}

Pairs Enum_enums(Class cself) {
    Pairs enums = pairs_value(Enum_cl->meta, string("enums"), Pairs);
    if (!enums)
        return NULL;
    return pairs_value(enums, string(cself->name), Pairs);
}

String Enum_to_string(Enum self) {
    return class_call(String, format, "%s:%p", self->cl->name, self->symbol);
}

void Enum_free(Enum self) {
    release(self->symbol);
}

implement(Type)
