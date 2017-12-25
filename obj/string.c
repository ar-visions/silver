#include <obj/obj.h>
#include <obj/string.h>

implement(String);

static char tolowercase(char in) {
    return (in >= 'A' && in <= 'Z') ? 'a' + (in - 'A') : in;
}

static char touppercase(char in) {
    return (in >= 'a' && in <= 'z') ? 'A' + (in - 'a') : in;
}

static char *strrstr(const char *s1, const char *s2) {
    size_t  len1 = strlen(s1);
    size_t  len2 = strlen(s2);
    char *s;

    if (len2 > len1)
        return NULL;
    for (s = (char *)&s1[len1 - len2]; s >= s1; --s)
        if (strncmp(s, s2, len2) == 0)
            return s;
    return NULL;
}

static char *strlwrcase(char *str) {
    for (char *p = str; *p; p++)
        *p = tolowercase(*p);
    return str;
}

static char *struprcase(char *str) {
    for (char *p = str; *p; p++)
        *p = touppercase(*p);
    return str;
}

void String_init(String self) {
    self->string_serialize = true;
}

void String_free(String self) {
    free_ptr(self->buffer);
}

int String_char_index(String self, int c) {
    char *p = strchr(self->buffer, c);
    if (!p) return -1;
    return p - self->buffer;
}

int String_str_index(String self, const char *str) {
    char *p = strstr(self->buffer, str);
    if (!p) return -1;
    return p - self->buffer;
}

int String_str_rindex(String self, const char *str) {
    char *p = strrstr(self->buffer, str);
    if (!p) return -1;
    return p - self->buffer;
}

String String_copy(String self) {
    String c = (String)super(copy);
    c->buffer = (char *)malloc(self->buffer_size);
    memcpy(c->buffer, self->buffer, self->buffer_size);
    return c;
}

int String_compare(String self, String b) {
    return strcmp(self->buffer, b->buffer);
}

int String_cmp(String self, const char *str) {
    return strcmp(self->buffer, str);
}

ulong String_hash(String self) {
   ulong h = 0;
   for (uint8 *p = (uint8 *)self->buffer; *p; p++)
      h = 31 * h + *p;
   return h;
}

String String_new_string(const char *buffer) {
    String self = new(String);
    self->length = strlen(buffer);
    self->buffer_size = self->length + 1;
    self->buffer = (char *)malloc(self->buffer_size);
    memcpy(self->buffer, buffer, self->buffer_size);
    return self;
}

String String_from_cstring(const char *buffer) {
    String self = class_call(String, new_string, buffer);
    return autorelease(self);
}

String String_from_string(String value) {
    return class_call(String, from_cstring, value->buffer);
}

String String_to_string(String self) {
    return class_call(String, from_cstring, self->buffer);
}

String String_from_bytes(const char *bytes, size_t length) {
    String self = auto(String);
    self->length = length;
    self->buffer_size = length + 1;
    self->buffer = (char *)malloc(self->buffer_size);
    memcpy(self->buffer, buffer, self->buffer_size);
    self->buffer[length] = 0;
    return self;
}

void String_check_resize(String self, uint chars) {
    if (self->buffer_size <= chars + 1) {
        int buffer_size = self->buffer_size + chars + 1 + clamp((int)((self->buffer_size - 1) << 1), 32, 1024);
        char *copy = (char *)malloc(buffer_size);
        if (self->buffer)
            memcpy(copy, self->buffer, self->length);
        copy[buffer_size] = 0;
        free(self->buffer);
        self->buffer = copy;
        self->buffer_size = buffer_size;
    }
}

void String_concat_char(String self, char c) {
    call(self, check_resize, self->length + 1);
    self->buffer[self->length] = c;
    self->buffer[++self->length] = 0;
}

void String_concat_chars(String self, const char *p, int len) {
    call(self, check_resize, self->length + len);
    memcpy(&self->buffer[self->length], p, len + 1);
    self->length += len;
}

void String_concat_string(String self, String b) {
    if (b)
        call(self, concat_chars, b->buffer, b->length);
}

void String_concat_long(String self, long v, const char *format) {
    char buffer[64];
    sprintf(buffer, format, v);
    call(self, concat_chars, buffer, strlen(buffer));
}

void String_concat_long_long(String self, uint64 v, const char *format) {
    char buffer[128];
    sprintf(buffer, format, v);
    call(self, concat_chars, buffer, strlen(buffer));
}

void String_concat_double(String self, double v, const char *format) {
    char buffer[64];
    sprintf(buffer, format, v);
    call(self, concat_chars, buffer, strlen(buffer));
}

void String_concat_object(String self, Base o) {
    String str = call(o, to_string);
    char *v = str ? str->buffer : (char *)"[null]";
    call(self, concat_chars, v, strlen(v));
}

String String_format(const char *format, ...) {
    if (!format)
        return NULL;
    String self = new(String);
    int flen = strlen(format);
    va_list args;
    va_start(args, format);
    int f_start = -1;
    int char_start = -1;
    bool sign = true;
    int width = 0;
    char formatter[32];

    for (int i = 0; i <= flen; i++) {
        char c = format[i];
        if (c == '%' || c == 0) {
            sign = true;
            width = 0;
            if (char_start >= 0) {
                int len = (i - char_start);
                call(self, concat_chars, &format[char_start], len);
                char_start = -1;
                if (c == 0)
                    break;
            }
            if (f_start >= 0) {
                call(self, concat_char, c);
            } else {
                f_start = i;
                continue;
            }
        } else if (f_start >= 0) {
            switch (c) {
                case 'l':
                    width++;
                    break;
                default: {
                    int flen = i - f_start;
                    if (flen + 1 >= sizeof(formatter))
                        break;
                    memcpy(formatter, &format[f_start], flen + 1);
                    formatter[flen + 1] = 0;
                    switch (c) {
                        case 's': {
                            char *chars = va_arg(args, char *);
                            call(self, concat_chars, chars, strlen(chars));
                            f_start = -1;
                            break;
                        }
                        case 'f':
                            call(self, concat_double, va_arg(args, double), formatter);
                            f_start = -1;
                            break;
                        case 'u':
                            if (width == 2)
                                call(self, concat_long_long, va_arg(args, long long), formatter);
                            else 
                                call(self, concat_long, va_arg(args, int), formatter);
                            f_start = -1;
                            break;
                        case 'd':
                            if (width == 2)
                                call(self, concat_long_long, va_arg(args, long long), formatter);
                            else 
                                call(self, concat_long, va_arg(args, int), formatter);
                            f_start = -1;
                            break;
                        case 'p':
                            call(self, concat_object, va_arg(args, Base));
                            f_start = -1;
                            break;
                    }
                    break;
                }
            }
        } else if (char_start == -1) {
            char_start = i;
        }
    }
    va_end(args);
    return self;
}

String String_lower(String self) {
    String c = cp(self);
    strlwrcase(c->buffer);
    return autorelease(c);
}

String String_upper(String self) {
    String c = cp(self);
    struprcase(c->buffer);
    return autorelease(c);
}