#include <obj/obj.h>
#include <obj/prop.h>
#include <obj/prim.h>
#include <ctype.h>

implement(Base)

static bool enable_logging = true;

void Base_class_preinit(Class c) { }

bool read_past_args(char *in, int *index) {
    int len = strlen(in);
    int depth = 0;
    for (int i = 0; i < len; i++) {
        if (in[i] == '(') {
            depth++;
        }
        if (in[i] == ')') {
            if (--depth == 0) {
                *index = i + 1;
                return true;
            }
        }
    }
    *index = 0;
    return false;
}

void *Base_alloc(Class cl, size_t size) {
    return calloc(1, size);
}

void Base_deallocx(Class cl, void *ptr) {
    free(ptr);
}

void Base_class_init(Class c) {
    class_Base cbase = (class_Base)c;
    if (!cbase->meta)
        cbase->meta = new(Pairs);
    Pairs props = new(Pairs);
    String props_str = new_string("props");
    pairs_add(cbase->meta, props_str, props);
    release(props_str);
    release(props);

    char **mnames = (char **)cbase->mnames;
    for (int i = 0; i < cbase->mcount; i++) {
        char *start = mnames[i];
        if (strchr(start, '*'))
            continue;
        int offset = 0;
        if (strncmp(start, "enum ", 5) == 0)
            offset = 5;
        char *mname = strchr(start + offset, ' ');
        if (mname && strncmp(mname, " get_", 5) == 0) {
            mname++;
            char *args = strchr(mname, ' ');
            if (!args)
                continue;
            int args_end = 0;
            if (!read_past_args(args, &args_end))
                continue;
            char *hash = &args[args_end];
            if (*hash == ' ')
                hash++;
            else
                continue;
            int type_len = mname - start - 1;
            int name_len = args - mname - 4;
            char *type = (char *)calloc(1, type_len + 1);
            char *name = (char *)calloc(1, name_len + 1);
            memcpy(type, start, type_len);
            type[type_len] = 0;
            memcpy(name, &mname[4], name_len);
            name[name_len] = 0;
            int offset = 0;
            bool is_enum = false;
            if (strncmp(type, "enum ", 5) == 0) {
                type[strlen(type) - 4] = 0; // hackish
                offset = 5;
                is_enum = true;
            }
            Prop p = class_call(Prop, new_with, (Class)cbase, is_enum, type + offset, name,
                (Getter)cbase->m[i], (Setter)cbase->m[i - 1], *hash ? hash : NULL);
            if (p) {
                String name_str = new_string(name);
                pairs_add(props, name_str, p);
                release(name_str);
            }
            free(type);
            free(name);
        }
    }
}

bool implements_serialize(class_Base cl) {
    return cl->to_string != Base_to_string && cl->from_string != Base_from_string;
}

void Base_serialize(Base self, Pairs pairs) {
    Pairs props = pairs_value(Pairs, self->cl->meta, string("props"));
    KeyValue kv;
    each_pair(props, kv) {
        String name  = instance(String, kv->key);
        Prop   prop  = instance(Prop, kv->value);
        Base   value = call(self, prop_value, prop);
        List   vlist = instance(List, value);
        if (value == NULL || implements_serialize((class_Base)value->cl)) {
            pairs_add(pairs, name, value);
        } else if (vlist) {
            List out = auto(List);
            Base v;
            each(vlist, v) {
                if (v == NULL || implements_serialize((class_Base)v->cl))
                    list_push(out, v);
                else {
                    Pairs p = auto(Pairs);
                    call(v, serialize, p);
                    list_push(out, p);
                }
            }
            pairs_add(pairs, name, out);
        } else {
            Pairs p = auto(Pairs);
            call(value, serialize, p);
            pairs_add(pairs, name, p);
        }
    }
}

String to_json(Pairs p, String str) {
    if (!str)
        str = string("");
    KeyValue kv;
    call(str, concat_char, '{');
    bool f = true;
    each_pair(p, kv) {
        String   key = instance(String, kv->key);
        Base   value = kv->value;
        List   vlist = instance(List, value);
        if (!key)
            continue;
        if (value == NULL || implements_serialize((class_Base)value->cl)) {
            if (!f) call(str, concat_char, ',');
            String s = NULL;
            String svalue = value ? call(value, to_string) : NULL;
            bool prim = instance(Primitive, value) != NULL;
            if (svalue) {
                const char *q = !prim ? "\"" : "";
                s = class_call(String, format, "\"%s\":%s%s%s", key->buffer,
                    q, svalue->buffer, q);
            } else {
                s = class_call(String, format, "\"%s\":null", key->buffer);
            }
            call(str, concat_string, s);
            f = false;
        } else if (instance(Pairs, value)) {
            if (!f) call(str, concat_char, ',');
            String s = class_call(String, format, "\"%s\":", key->buffer);
            call(str, concat_string, s);
            to_json((Pairs)value, str);
            f = false;
        } else if (vlist) {
            if (!f) call(str, concat_char, ',');
            String skey = class_call(String, format, "\"%s\":[", key->buffer);
            call(str, concat_string, skey);
            Base v;
            bool first = true;
            each(vlist, v) {
                if (v == NULL || implements_serialize((class_Base)v->cl)) {
                    String svalue = v ? call(v, to_string) : NULL;
                    bool prim = instance(Primitive, v) != NULL;
                    String s = NULL;
                    if (svalue) {
                        const char *q = !prim ? "\"" : "";
                        s = class_call(String, format, "%s%s%s",
                            q, svalue->buffer, q);
                    } else {
                        s = class_call(String, format, "null");
                    }
                    if (!first)
                        call(str, concat_char, ',');
                    call(str, concat_string, s);
                    first = false;
                } else if (instance(Pairs, v)) {
                    to_json((Pairs)v, str);
                }
            }
            call(str, concat_char, ']');
            f = false;
        }
    }
    call(str, concat_char, '}');
    return str;
}

String Base_to_json(Base self) {
    Pairs p = new(Pairs);
    call(self, serialize, p);
    String str = to_json(p, NULL);
    release(p);
    return str;
}

enum JsonMode {
    MODE_OBJECT = 0,
    MODE_ARRAY
};

bool parse_ws(const char **cursor) {
    const char *s = *cursor;
    while (isspace(*s))
        s++;
    *cursor = s;
    if (*s == 0)
        return false;
    return true;
}

String parse_numeric(const char **cursor) {
    const char *s = *cursor;
    if (*s != '-' && !isdigit(*s))
        return NULL;

    const int max_sane_number = 128;
    const char *number_start = s;
    bool fp = false;
    
    for (++s; ; ++s) {
        if (*s == '.') {
            fp = true;
            continue;
        }
        if (!isdigit(*s))
            break;
    }
    size_t number_len = s - number_start;
    if (number_len == 0 || number_len > max_sane_number)
        return NULL;
    *cursor = &number_start[number_len];
    return class_call(String, from_bytes, (uint8 *)number_start, number_len);
}

String parse_quoted_string(const char **cursor, size_t max_len) {
    const char *first = *cursor;
    if (*first != '"')
        return NULL;
    bool last_slash = false;
    const char *start = ++(*cursor);
    const char *s = start;
    for (; *s != 0; ++s) {
        if (*s == '\\')
            last_slash = true;
        else if (*s == '"' && !last_slash)
            break;
        else
            last_slash = false;
    }
    if (*s == 0)
        return NULL;
    size_t len = (size_t)(s - start);
    if (max_len > 0 && len > max_len)
        return NULL;
    *cursor = s + 1;
    return class_call(String, from_bytes, (uint8 *)start, len);
}

String parse_symbol(const char **cursor) {
    const int max_sane_symbol = 128;
    const char *sym_start = *cursor;
    const char *s = *cursor;
    while (isalpha(*s))
        s++;
    *cursor = s;
    size_t sym_len = s - sym_start;
    if (sym_len == 0 || sym_len > max_sane_symbol)
        return NULL;
    return class_call(String, from_bytes, (uint8 *)sym_start, sym_len);
}

enum JsonParse {
    PARSE_KEY,
    PARSE_COLON,
    PARSE_VALUE,
    PARSE_COMMA
};

typedef struct _JsonMode {
    enum JsonMode mode;
    enum JsonParse parse;
    String key;
    List assoc_list;
    Base object;
    Class cl;
} JsMode;

void modes_push(JsMode **m) {
    (*m)++;
    memset(*m, 0, sizeof(JsMode));
}

void modes_pop(JsMode **m) {
    (*m)--;
}

Base Base_from_json(Class c, String value) {
    const int max_key_len = 1024;
    const char *s = value->buffer;
    JsMode *modes = (JsMode *)malloc(value->length * sizeof(JsMode));
    JsMode *modes_origin = modes;

    parse_ws(&s);
    bool pairs_result = c == class_object(Pairs);
    Base obj = autorelease(new_obj((class_Base)c, 0));
    memset(modes, 0, sizeof(JsMode));
    modes->cl = c;
    modes->mode = MODE_OBJECT;
    modes->parse = PARSE_KEY;
    modes->object = obj;
    switch (*s) {
        case '{': {
            bool br = false;
            s++;
            while (!br) {
                parse_ws(&s);
                switch (*s) {
                    case ':':
                        if (modes->mode != MODE_OBJECT || modes->parse != PARSE_COLON)
                            return NULL;
                        modes->parse = PARSE_VALUE;
                        s++;
                        break;
                    case ',':
                        if (modes->parse != PARSE_COMMA)
                            return NULL;
                        if (modes->mode == MODE_OBJECT)
                            modes->parse = PARSE_KEY;
                        else if (modes->mode == MODE_ARRAY)
                            modes->parse = PARSE_VALUE;
                        s++;
                        break;
                    case ']':
                        if (modes->mode != MODE_ARRAY || (modes->parse != PARSE_COMMA && modes->parse != PARSE_VALUE))
                            return NULL;
                        modes_pop(&modes);
                        s++;
                        break;
                    case '}':
                        if (modes->mode != MODE_OBJECT)
                            return NULL;
                        s++;
                        if (modes == modes_origin) {
                            br = true;
                            break;
                        }
                        modes_pop(&modes);
                        break;
                    default: {
                        if (modes->mode == MODE_OBJECT && modes->parse == PARSE_KEY) {
                            modes->key = parse_quoted_string(&s, max_key_len);
                            if (!modes->key)
                                return NULL;
                            modes->parse = PARSE_COLON;
                            break;
                        }
                        if (modes->parse != PARSE_VALUE)
                            return NULL;
                        parse_ws(&s);
                        switch (*s) {
                            case '{': {
                                class_Base cl = (class_Base)modes->cl;
                                Base obj = modes->object;
                                List assoc_list = modes->assoc_list;
                                String key = modes->key;
                                modes->parse = PARSE_COMMA;
                                modes_push(&modes);
                                modes->mode = MODE_OBJECT;
                                modes->parse = PARSE_KEY;
                                if (assoc_list) {
                                    class_Base item_class = pairs_result ? (class_Base)Pairs_cl : 
                                        (class_Base)assoc_list->item_class;
                                    modes->object = autorelease(new_obj(item_class, 0));
                                    list_push(assoc_list, modes->object);
                                } else {
                                    if (pairs_result) {
                                        modes->object = (Base)auto(Pairs);
                                        pairs_add(((Pairs)obj), key, modes->object);
                                    } else {
                                        Prop prop = cl->find_prop((Class)cl, (const char *)key->buffer);
                                        if (!prop || !prop->class_type)
                                            return NULL;
                                        modes->object = autorelease(new_obj((class_Base)prop->class_type, 0));
                                        prop->setter(obj, modes->object);
                                    }
                                }
                                s++;
                                break;
                            }
                            case '[': {
                                Base obj = modes->object;
                                List prev_list = modes->assoc_list;
                                String key = modes->key;
                                modes->parse = PARSE_COMMA;
                                modes_push(&modes);
                                modes->mode = MODE_ARRAY;
                                modes->parse = PARSE_VALUE;
                                modes->assoc_list = pairs_result ? auto(List) : 
                                    get_prop(List, obj, key->buffer);
                                if (pairs_result) {
                                    if (instance(Pairs, obj)) {
                                        if (!key)
                                            return NULL;
                                        pairs_add(((Pairs)obj), key, modes->assoc_list);
                                    } else if (prev_list)
                                        list_push(prev_list, modes->assoc_list);
                                }
                                modes->object = NULL;
                                s++;
                                break;
                            }
                            default: {
                                bool is_numeric = (*s == '-' || isdigit(*s));
                                bool is_str = *s == '"';
                                if (is_numeric || is_str) {
                                    String value = is_numeric ? parse_numeric(&s) : parse_quoted_string(&s, 0);
                                    if (!value)
                                        return NULL;
                                    if (modes->assoc_list) {
                                        Base item;
                                        if (pairs_result) {
                                            if (is_numeric)
                                                item = (Base)int32_object(atoi(value->buffer));
                                            else if (is_str)
                                                item = (Base)value;
                                        } else {
                                            class_Base item_class = (class_Base)modes->assoc_list->item_class;
                                            item = item_class->from_string((Class)item_class, value);
                                        }
                                        list_push(modes->assoc_list, item);
                                    } else if (pairs_result) {
                                        if (call(modes->key, compare, string("filter_count")) == 0) {
                                            int test = 0;
                                            test++;
                                        }
                                        pairs_add(((Pairs)modes->object), modes->key, value);
                                    } else
                                        call(modes->object, set_property, modes->key->buffer, base(value));
                                } else {
                                    String symbol = parse_symbol(&s);
                                    if (!symbol)
                                        return NULL;
                                    if (call(symbol, cmp, "null") == 0) {
                                        if (!pairs_result) {
                                            call(modes->object, set_property, modes->key->buffer, NULL);
                                        }
                                    } else {
                                        bool bool_value = false;
                                        if (call(symbol, cmp, "true") == 0)
                                            bool_value = true;
                                        else if (call(symbol, cmp, "false") != 0)
                                            return NULL;
                                        if (pairs_result) {
                                            if (modes->assoc_list)
                                                list_push(modes->assoc_list, bool_object(bool_value));
                                            else if (!modes->object)
                                                return NULL;
                                            else
                                                pairs_add(((Pairs)modes->object), modes->key, bool_object(bool_value));
                                        } else {
                                            String bool_str = string(bool_value ? "true" : "false");
                                            call(modes->object, set_property, modes->key->buffer, base(bool_str));
                                        }
                                    }
                                }
                                modes->parse = PARSE_COMMA;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            break;
        }
        default:
            return NULL;
    }
    return obj;
}

void Base_init(Base self) { }

int Base_compare(Base a, Base b) {
    return (long long)b - (long long)a;
}

const char *Base_to_cstring(Base self) {
    String str = self(to_string);
    return (const char *)(str ? str->buffer : NULL);
}

Base Base_from_cstring(Class cl, const char *value) {
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

Prop Base_find_prop(Class cl, const char *name) {
    Pairs props = pairs_value(Pairs, cl->meta, string("props"));
    if (!props)
        return NULL;
    Prop p = pairs_value(Prop, props, string(name));
    return p;
}

void Base_set_property(Base self, const char *name, Base base_value) {
    Prop p = self->cl->find_prop((Class)self->cl, name);
    if (!p)
        return;
    if (!base_value) {
        p->setter(self, (void *)NULL);
        return;
    }
    String value = call(base_value, to_string);
    if (!p->enum_type)
        return;
    switch (p->enum_type->ordinal) {
        case Type_Boolean: {
            bool v = (value && strcmp((char *)value->buffer, "true") == 0) ? true : false;
            p->setter(self, (void *)(size_t)v);
            break;
        }
        case Type_Enum: {
            class_Base c = (class_Base)p->class_type;
            Enum e = value ? Enum_cl->find((Class)c, value->buffer) : NULL;
            if (e)
                p->setter(self, (void *)(size_t)e->ordinal);
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
                p->setter(self, c->from_string((Class)c, value));
            break;
        }
        default:
            break;
    }
}

Base Base_prop_value(Base self, Prop p) {
    if (!p || !p->enum_type)
        return NULL;
    switch (p->enum_type->ordinal) {
        case Type_Object:   return (Base)p->getter(self);
        case Type_Enum:     return (Base)Enum_cl->from_ordinal((Class)p->class_type, (int)((int (*)(Base))p->getter)(self));
        case Type_Boolean:  return (Base)bool_object((bool)((size_t (*)(Base))p->getter)(self));
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

Base Base_get_property(Base self, const char *name) {
    Pairs props = pairs_value(Pairs, self->cl->meta, string("props"));
    if (!props)
        return NULL;
    Prop p = pairs_value(Prop, props, string(name));
    return call(self, prop_value, p);
}

Base Base_property_meta(Base self, const char *name, const char *meta) {
    Pairs props = pairs_value(Pairs, self->cl->meta, string("props"));
    if (!props)
        return NULL;
    Prop p = pairs_value(Prop, props, string(name));
    return p ? pairs_value(Base, p->meta, string(meta)) : NULL;
}

Base Base_copy(Base self) {
    Base c = (Base)object_alloc(self, self->alloc_size);
    memcpy(c, self, self->alloc_size);
    c->refs = 1;
    return c;
}

Base Base_from_string(Class cl, String value) {
    if (!value)
        return NULL;
    return ((class_Base)cl)->from_cstring(cl, (const char *)value->buffer);
}

String Base_to_string(Base self) {
    return string("N/A");
}

Base Base_retain(Base self) {
    __atomic_fetch_add(&self->refs, 1, __ATOMIC_SEQ_CST);
    return self;
}

Base Base_release(Base self) {
    __atomic_fetch_add(&self->refs, -1, __ATOMIC_SEQ_CST);
    if (self->refs == 0)
        free_obj(self);
    return self;
}

Base Base_autorelease(Base self) {
    AutoRelease ar = AutoRelease_cl->current();
    if (!ar) {
        fprintf(stderr, "No autorelease pool found!\n");
        exit(1);
    }
    list_push(ar, self);
    return release(self);
}

void Base_free(Base self) {
}

ulong Base_hash(Base self) {
    return 0;
}
