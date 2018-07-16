#include <obj/obj.h>
#include <ctype.h>

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

char *strlwrcase(char *str) {
    for (char *p = str; *p; p++)
        *p = tolowercase(*p);
    return str;
}

char *struprcase(char *str) {
    for (char *p = str; *p; p++)
        *p = touppercase(*p);
    return str;
}

void String_init(String self) {
    //self->buffer = (char *)malloc(1);
    //*self->buffer = 0;
    self->buffer_size = 0;
}

void String_free(String self) {
    free(self->buffer);
    free(self->utf8_buffer);
}

int String_char_index(String self, int c) {
    char *p = strchr(self->buffer, c);
    if (!p) return -1;
    return p - self->buffer;
}

int String_char_count(String self, int c) {
    int count = 0;
    for (int i = 0; i < self->length; i++) {
        if (self->buffer[i] == c)
            count++;
    }
    return count;
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

uint *String_decode_utf8(String self, uint *length) {
    if (self->utf8_buffer) {
        *length = self->utf8_length;
        return self->utf8_buffer;
    }
    uint char_width;
	uint char_count = 0;
    self->utf8_buffer = (uint *)malloc(sizeof(uint) * (self->length + 1));
    if (self->length > 0)
        for (uint i = 0;; i += char_width) {
            char_width = 1;
            uint8 b0 = self->buffer[i];
            uint code = 0;
            if (b0 == 0)
                break;
            else if ((b0 & 0x80) == 0) {
                code = (uint)b0;
            } else if ((b0 & 0xE0) == 0xC0) {
                uint8 b1 = self->buffer[i + 1];
                if (b1 == 0) break;
                code = ((uint)(b0 & ~(0xE0)) << 6) | (uint)(b1 & ~(0xC0));
                char_width = 2;
            } else if ((b0 & 0xF0) == 0xE0) {
                uint8 b1 = self->buffer[i + 1];
                if (b1 == 0) break;
                uint8 b2 = self->buffer[i + 2];
                if (b2 == 0) break;
                code = ((uint)(b0 & ~(0xF0)) << 12) | ((uint)(b1 & ~(0xC0)) << 6) | (uint)(b2 & ~(0xC0));
                char_width = 3;
            } else if ((b0 & 0xF8) == 0xF0) {
                uint8 b1 = self->buffer[i + 1];
                if (b1 == 0) break;
                uint8 b2 = self->buffer[i + 2];
                if (b2 == 0) break;
                uint8 b3 = self->buffer[i + 3];
                if (b3 == 0) break;
                code = ((uint)(b0 & ~(0xF8)) << 18) | ((uint)(b1 & ~(0xC0)) << 6) | ((uint)(b2 & ~(0xC0)) << 6) | (uint)(b3 & ~(0xC0));
                char_width = 4;
            } else
                break;
            self->utf8_buffer[char_count++] = code;
        }
    self->utf8_buffer[char_count] = 0;
    self->utf8_length = char_count;
    *length = self->utf8_length;
    return self->utf8_buffer;
}

String String_from_file(Class cl, const char *file) {
    FILE *f = fopen(file, "r");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    char *bytes = (char *)malloc(len + 1);
    size_t blocks_read = fread(bytes, len, 1, f);
    if (blocks_read != 1) {
        fclose(f);
        free(bytes);
        return NULL;
    }
    fclose(f);
    bytes[len] = 0;
    String self = class_call(String, from_bytes, (uint8 *)bytes, len);
    free(bytes);
    return self;
}

bool String_to_file(String self, const char *file) {
    FILE *f = fopen(file, "w");
    bool success = false;
    if (f) {
        success = fwrite(self->buffer, self->length, 1, f) == 1;
        fclose(f);
    }
    return success;
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

String String_new_from_cstring(Class cl, const char *buffer) {
    String self = new(String);
    self->length = strlen(buffer);
    self->buffer_size = self->length + 1;
    self->buffer = (char *)malloc(self->buffer_size);
    memcpy(self->buffer, buffer, self->buffer_size);
    return self;
}

String String_from_cstring(Class cl, const char *buffer) {
    String self = String_new_from_cstring((Class)cl, buffer);
    return autorelease(self);
}

String String_from_string(Class cl, String value) {
    return String_from_cstring(cl, value->buffer);
}

String String_to_string(String self) {
    return class_call(String, from_cstring, self->buffer);
}

Base String_infer_object(String self) {
    if (self->length == 4 && call(self, cmp, "true") == 0)
        return base(bool_object(true));
    else if (self->length == 5 && call(self, cmp, "false") == 0)
        return base(bool_object(true));
    
    int pnt = 0;
    bool numeric = self->length > 0;
    for (int i = 0; i < self->length; i++) {
        char c = self->buffer[i];
        if (c == '.' && i > 0 && i < (self->length - 1))
            pnt++;
        else if (!((i == 0 && c == '-') || isdigit(c))) {
            numeric = false;
            break;
        }
    }
    if (numeric && pnt <= 1) {
        if (pnt == 0) {
            int val = 0;
            sscanf(self->buffer, "%d", &val);
            return base(int32_object(val));
        } else {
            double val = 0;
            sscanf(self->buffer, "%lf", &val);
            return base(double_object(val));
        }
    }
    return base(string(self->buffer));
}

String String_new_from_bytes(Class cl, const uint8 *bytes, size_t length) {
    String self = new(String);
    self->length = length;
    self->buffer_size = length + 1;
    self->buffer = (char *)malloc(self->buffer_size);
    memcpy(self->buffer, bytes, length);
    self->buffer[length] = 0;
    return self;
}

String String_from_bytes(Class cl, const uint8 *bytes, size_t length) {
    String self = auto(String);
    self->length = length;
    self->buffer_size = length + 1;
    self->buffer = (char *)malloc(self->buffer_size);
    memcpy(self->buffer, bytes, length);
    self->buffer[length] = 0;
    return self;
}

void String_check_resize(String self, uint chars) {
    if (self->buffer_size <= chars + 1) {
        int buffer_size = self->buffer_size + chars + 1 + clamp((((int)self->buffer_size - 1) << 1), 32, 1024);
        char *copy = (char *)malloc(buffer_size);
        if (self->buffer)
            memcpy(copy, self->buffer, self->length);
        copy[buffer_size - 1] = 0;
        free(self->buffer);
        self->buffer = copy;
        self->buffer_size = buffer_size;
    }
}

int String_concat_char(String self, char c) {
    call(self, check_resize, self->length + 1);
    self->buffer[self->length] = c;
    self->buffer[++self->length] = 0;
    free(self->utf8_buffer);
    self->utf8_length = 0;
    return 1;
}

int String_concat_chars(String self, const char *p, int len) {
    if (!p || len == 0)
        return 0;
    call(self, check_resize, self->length + len);
    memcpy(&self->buffer[self->length], p, len);
    self->length += len;
    self->buffer[self->length] = 0;
    free(self->utf8_buffer);
    self->utf8_length = 0;
    return len;
}

int String_concat_cstring(String self, const char *p) {
    if (!p)
        return 0;
    int len = strlen(p);
    call(self, concat_chars, p, len);
}

int String_concat_string(String self, String b) {
    if (b) {
        call(self, concat_chars, b->buffer, b->length);
        free(self->utf8_buffer);
        self->utf8_length = 0;
    }
}

int String_concat_long(String self, long v, const char *format) {
    char buffer[64];
    sprintf(buffer, format, v);
    int len = strlen(buffer);
    call(self, concat_chars, buffer, len);
    free(self->utf8_buffer);
    self->utf8_length = 0;
    return len;
}

int String_concat_long_long(String self, uint64 v, const char *format) {
    char buffer[128];
    sprintf(buffer, format, v);
    int len = strlen(buffer);
    call(self, concat_chars, buffer, len);
    free(self->utf8_buffer);
    self->utf8_length = 0;
    return len;
}

int String_concat_double(String self, double v, const char *format) {
    char buffer[64];
    sprintf(buffer, format, v);
    int len = strlen(buffer);
    call(self, concat_chars, buffer, len);
    free(self->utf8_buffer);
    self->utf8_length = 0;
    return len;
}

int String_concat_object(String self, Base o) {
    String str = call(o, to_string);
    char *v = str ? str->buffer : (char *)"[null]";
    int len = strlen(v);
    call(self, concat_chars, v, len);
    free(self->utf8_buffer);
    self->utf8_length = 0;
    return len;
}

String String_format(Class cl, const char *format, ...) {
    if (!format)
        return NULL;
    String self = auto(String);
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
                        case 'p': {
                            Base b = va_arg(args, Base);
                            if (b)
                                call(self, concat_object, b);
                            else
                                call(self, concat_cstring, "[null]");
                            f_start = -1;
                            break;
                        }
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