#include <obj.h>
#include <str.h>

implement(String);

static char tolower(char in) {
    return (in >= 'A' && in <= 'Z') ? 'a' + (in - 'A') : in;
}

static char toupper(char in) {
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

static char *strlwr(char *str) {
    for (char *p = str; *p; p++)
        *p = tolower(*p);
    return str;
}

static char *strupr(char *str) {
    for (char *p = str; *p; p++)
        *p = toupper(*p);
    return str;
}

void String_init(String self) {
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

String String_lower(String self) {
    String c = cp(self);
    strlwr(c->buffer);
    return autorelease(c);
}

String String_upper(String self) {
    String c = cp(self);
    strupr(c->buffer);
    return autorelease(c);
}