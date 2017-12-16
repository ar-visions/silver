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

void String_init(String this) {
}

void String_free(String this) {
    free_ptr(this->buffer);
}

int String_char_index(String this, int c) {
    char *p = strchr(this->buffer, c);
    if (!p) return -1;
    return p - this->buffer;
}

int String_str_index(String this, const char *str) {
    char *p = strstr(this->buffer, str);
    if (!p) return -1;
    return p - this->buffer;
}

int String_str_rindex(String this, const char *str) {
    char *p = strrstr(this->buffer, str);
    if (!p) return -1;
    return p - this->buffer;
}

String String_copy(String this) {
    String c = (String)super(copy);
    c->buffer = malloc(this->buffer_size);
    memcpy(c->buffer, this->buffer, this->buffer_size);
    return c;
}

int String_compare(String this, String b) {
    return strcmp(this->buffer, b->buffer);
}

int String_cmp(String this, const char *str) {
    return strcmp(this->buffer, str);
}

ulong String_hash(String this) {
   ulong h = 0;
   for (uint8 *p = (uint8 *)this->buffer; *p; p++)
      h = 31 * h + *p;
   return h;
}

String String_new_string(const char *buffer) {
    String this = new(String);
    this->length = strlen(buffer);
    this->buffer_size = this->length + 1;
    this->buffer = (char *)malloc(this->buffer_size);
    memcpy(this->buffer, buffer, this->buffer_size);
    return this;
}

String String_from_cstring(const char *buffer) {
    String this = class_call(String, new_string, buffer);
    return autorelease(this);
}

String String_from_string(String value) {
    return class_call(String, from_cstring, value->buffer);
}

String String_to_string(String this) {
    return class_call(String, from_cstring, this->buffer);
}

String String_lower(String this) {
    String c = cp(this);
    strlwr(c->buffer);
    return autorelease(c);
}

String String_upper(String this) {
    String c = cp(this);
    strupr(c->buffer);
    return autorelease(c);
}