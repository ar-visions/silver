#include <obj.h>
#include <prop.h>
#include <prim.h>

implement(Base)

static bool enable_logging = true;

void Base_class_preinit(Class c) { }

void Base_class_init(Class c) {
    class_Base cbase = (class_Base)c;
    if (!cbase->meta)
        cbase->meta = new(Pairs);
    Pairs props = new(Pairs);
    pairs_add(cbase->meta, string("props"), props);
    release(props);

    char **mnames = (char **)cbase->mnames;
    cbase->pcount = 0;
    for (int i = 0; i < cbase->mcount; i++) {
        char *start = mnames[i];
        if (strchr(start, '*'))
            continue;
        char *mname = strchr(start, ' ');
        if (mname && strncmp(mname, " get_", 5) == 0)
            cbase->pcount++;
    }
    if (!cbase->pcount)
        return;
    for (int i = 0; i < cbase->mcount; i++) {
        char *start = mnames[i];
        if (strchr(start, '*'))
            continue;
        char *mname = strchr(start, ' ');
        if (mname && strncmp(mname, " get_", 5) == 0) {
            mname++;
            char *args = strchr(mname, ' ');
            if (!args)
                continue;
            int type_len = mname - start - 1;
            int name_len = args - mname - 4;
            char type[type_len + 1];
            strncpy(type, start, type_len);
            type[type_len] = 0;
            char name[name_len + 1];
            strncpy(name, &mname[4], name_len);
            name[name_len] = 0;
            Prop p = class_call(Prop, new_with, type, name, (Getter)cbase->m[i], (Setter)cbase->m[i - 1]);
            if (p)
                pairs_add(props, string(name), p);
        }
    }
}

void Base_init(Base self) { }

int Base_compare(Base a, Base b) {
    return (long long)b - (long long)a;
}

const char *Base_to_cstring(Base self) {
    String str = self(to_string);
    return (const char *)(str ? str->buffer : NULL);
}

Base Base_from_cstring(const char *value) {
    return NULL;
}

bool Base_is_logging(Base self) {
    return enable_logging;
}

String Base_identity(Base self) {
    return string(self->cl->name);
}

void Base_print(Base self, String str) {
    if (str) {
        String identity = call(self, identity);
        printf("%s: %s\n", (const char *)identity->buffer, (const char *)str->buffer);
    }
}

void Base_set_property(Base self, const char *name, Base base_value) {
    Pairs props = pairs_value(self->cl->meta, string("props"), Pairs);
    if (!props)
        return;
    Prop p = pairs_value(props, string(name), Prop);
    if (!p)
        return;
    String value = call(base_value, to_string);
    if (!p->enum_type)
        return;
    switch (p->enum_type->ordinal) {
        case Type_Bool: {
            bool v = (value && strcmp((char *)value->buffer, "true") == 0) ? true : false;
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_Int8: {
            char v = (char)atoi((char *)value->buffer);
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_UInt8: {
            unsigned char v = (unsigned char)atoi((char *)value->buffer);
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_Int16: {
            short v = (short)atoi((char *)value->buffer);
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_UInt16: {
            unsigned short v = (unsigned short)atoi((char *)value->buffer);
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_Int32: {
            int v = (int)atoi((char *)value->buffer);
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_UInt32: {
            unsigned int v = (unsigned int)atoi((char *)value->buffer);
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_Int64: {
            long long v = (long long)strtoll((char *)value->buffer, NULL, 10);
            ((void (*)(Base, long long))p->setter)(self, v);
            break;
        }
        case Type_UInt64: {
            unsigned long long v = (unsigned long long)strtoull((char *)value->buffer, NULL, 10);
            ((void (*)(Base, unsigned long long))p->setter)(self, v);
            break;
        }
        case Type_Long: {
            long v = (long)strtoul((char *)value->buffer, NULL, 10);
            p->setter(self, (void *)v);
            break;
        }
        case Type_ULong: {
            unsigned long v = (unsigned long)strtoul((char *)value->buffer, NULL, 10);
            p->setter(self, (void *)v);
            break;
        }
        case Type_Float: {
            float v = (float)atof((char *)value->buffer);
            ((void (*)(Base, float))p->setter)(self, v);
            break;
        }
        case Type_Double: {
            double v = (double)atof((char *)value->buffer);
            ((void (*)(Base, double))p->setter)(self, v);
            break;
        }
        case Type_Object: {
            class_Base c = (class_Base)p->class_type;
            if (c)
                p->setter(self, c->from_string(value));
            break;
        }
        default:
            break;
    }
}

Base Base_get_property(Base self, const char *name) {
    Pairs props = pairs_value(self->cl->meta, string("props"), Pairs);
    if (!props)
        return NULL;
    Prop p = pairs_value(props, string(name), Prop);
    if (!p || !p->enum_type)
        return NULL;
    switch (p->enum_type->ordinal) {
        case Type_Object:   return (Base)p->getter(self);
        case Type_Bool:     return (Base)bool_object((bool)((size_t (*)(Base))p->getter)(self));
        case Type_Int8:     return (Base)int8_object(((int8 (*)(Base))p->getter)(self));
        case Type_UInt8:    return (Base)uint8_object(((uint8 (*)(Base))p->getter)(self));
        case Type_Int16:    return (Base)int16_object(((int16 (*)(Base))p->getter)(self));
        case Type_UInt16:   return (Base)uint16_object(((uint16 (*)(Base))p->getter)(self));
        case Type_Int32:
            return (Base)int32_object(((int32 (*)(Base))p->getter)(self));
        case Type_UInt32:   return (Base)uint32_object(((uint32 (*)(Base))p->getter)(self));
        case Type_Int64:    return (Base)int64_object(((int64 (*)(Base))p->getter)(self));
        case Type_UInt64:   return (Base)uint64_object(((uint64 (*)(Base))p->getter)(self));
        case Type_Long:     return (Base)long_object(((long (*)(Base))p->getter)(self));
        case Type_ULong:    return (Base)ulong_object(((ulong (*)(Base))p->getter)(self));
        case Type_Float:    return (Base)float_object(((float (*)(Base))p->getter)(self));
        case Type_Double:   return (Base)double_object(((double (*)(Base))p->getter)(self));
        default:
            break;
    }
    return NULL;
}

Base Base_copy(Base self) {
    Base c = (Base)malloc(self->alloc_size);
    memcpy(c, self, self->alloc_size);
    c->refs = 1;
    return c;
}

Base Base_from_string(String value) {
    if (!value)
        return NULL;
    return class_call(Base, from_cstring, (const char *)value->buffer);
}

String Base_to_string(Base self) {
    return string("N/A");
}

Base Base_retain(Base self) {
    if (self->refs++ == 0) {
        // remove from ar
        AutoRelease ar = AutoRelease_cl->current();
        call(ar, remove, self);
    }
    return self;
}

void Base_release(Base self) {
    if (self->refs-- == 0) {
        AutoRelease ar = AutoRelease_cl->current();
        call(ar, remove, self);
    }
    if (self->refs <= 0)
        free_obj(self);
}

Base Base_autorelease(Base self) {
    AutoRelease ar = AutoRelease_cl->current();
    if (ar && !self->ar_node) {
        self->refs = 1;
        call(ar, add, self);
    }
    return self;
}

void Base_free(Base self) {
    free(self);
}

ulong Base_hash(Base self) {
    return 0;
}
