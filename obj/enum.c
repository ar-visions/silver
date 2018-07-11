#include <obj/obj.h>
#include <obj/pairs.h>

implement(Enum)

void Enum_class_preinit(Class cself) {
    if (!Enum_cl->meta) {
        Enum_cl->meta = new(Pairs);
    }
    class_Enum c = (class_Enum)cself;
    String enums_str = new_string("enums");
    Pairs enums = pairs_value(Pairs, Enum_cl->meta, enums_str);
    if (!enums) {
        enums = new(Pairs);
        pairs_add(Enum_cl->meta, enums_str, enums);
    }
    release(enums_str);
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
            char *type = (char *)calloc(1, type_len + 1);
            char *name = (char *)calloc(1, name_len + 1);
            memcpy(type, start, type_len);
            type[type_len] = 0;
            memcpy(name, &mname[5], name_len);
            name[name_len] = 0;
            Enum enum_obj = (Enum)new_obj((class_Base)c, 0);
            if (enum_obj) {
                printf("type: %s, name = %s\n", type, name);
                strlwrcase(name);
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

Enum Enum_from_ordinal(Class c, int ordinal) {
    if (!Enum_cl->meta)
        return NULL;
    String key = new_string("enums");
    Pairs enums = pairs_value(Pairs, Enum_cl->meta, key);
    release(key);
    if (!enums)
        return NULL;
    key = new_string(c->name);
    Pairs e = pairs_value(Pairs, enums, key);
    release(key);
    if (!e)
        return NULL;
    KeyValue kv;
    each_pair(e, kv) {
        Enum en = (Enum)kv->value;
        if (en->ordinal == ordinal) {
            return en;
        }
    }
    return NULL;
}

Enum Enum_find(Class c, const char *symbol) {
    if (!Enum_cl->meta)
        return NULL;
    String key = new_string("enums");
    Pairs enums = pairs_value(Pairs, Enum_cl->meta, key);
    release(key);
    if (!enums)
        return NULL;
    key = new_string(c->name);
    Pairs e = pairs_value(Pairs, enums, key);
    release(key);
    if (!e)
        return NULL;
    key = new_string(symbol);
    String sym_str = new_string(symbol);
    strlwrcase(sym_str->buffer);
    Enum en = (Enum)pairs_value(Enum, e, sym_str);
    release(sym_str);
    release(key);
    return en;
}

Pairs Enum_enums(Class cself) {
    Pairs enums = pairs_value(Pairs, Enum_cl->meta, string("enums"));
    if (!enums)
        return NULL;
    return pairs_value(Pairs, enums, string(cself->name));
}

String Enum_to_string(Enum self) {
    return class_call(String, format, "%s:%p", self->cl->name, self->symbol);
}

void Enum_free(Enum self) {
    release(self->symbol);
}

implement(Type)
