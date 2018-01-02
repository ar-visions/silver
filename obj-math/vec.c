#include <obj-math/math.h>

implement(Vec)
implement(Vec3)
implement(Vec4)

void Vec_init(Vec self) {
    self->vec = &self->x;
    self->count = 2;
}

void Vec3_init(Vec3 self) {
    self->count = 3;
}

void Vec4_init(Vec4 self) {
    self->count = 4;
}

ulong Vec_hash(Vec self) {
    ulong ret = 0;
    for (int i = 0; i < self->count; i++)
        ret += (ulong)(self->vec[i] * 100.0 * 31.0);
    return ret;
}

Vec Vec_with(Class c, ...) {
    Vec self = (Vec)new_obj((class_Base)c, 0);
    va_list args;
    va_start(args, c);
    for (int i = 0; i < self->count; i++)
        self->vec[i] = va_arg(args, double);
    va_end(args);
    return autorelease(self);
}

double Vec_length(Vec self) {
    double sqr = 0;
    for (int i = 0; i < self->count; i++)
        sqr += sqr(self->vec[i]);
    return sqrt(sqr);
}

int Vec_compare(Vec self, Vec b) {
    double a_len = call(self, length);
    double b_len = call(b, length);
    if (a_len > b_len)
        return 1;
    else if (a_len < b_len)
        return -1;
    return 0;
}

Vec Vec_with_count(int count) {
    Vec self = (Vec)new_obj((class_Base)Vec_cl, sizeof(double) * max(0, count - 2));
    return autorelease(self);
}

Vec Vec_from_cstring(const char *value) {
    if (!value)
        return NULL;
    String str = class_call(String, from_cstring, value);
    char *cvalue = str->buffer;
    int count = 0;
    for (int i = 0, len = strlen(cvalue); i <= len; i++)
        if (cvalue[i] == ',' || cvalue[i] == 0)
            count++;
    if (!count)
        return NULL;
    Vec self = class_call(Vec, with_count, count);
    char *start = cvalue;
    int v = 0;
    for (int i = 0, len = strlen(cvalue); i <= len; i++) {
        if (cvalue[i] == ',' || cvalue[i] == 0) {
            cvalue[i] = 0;
            sscanf(start,"%lf",&self->vec[v++]);
            if (i != len)
                cvalue[i] = ',';
            start = &cvalue[i + 1];
        }
    }
    return self;
}

String Vec_to_string(Vec self) {
    char *buf = (char *)alloc_bytes(64 + 64 * self->count);
    int len = 0;
    buf[0] = 0;
    for (int i = 0; i < self->count; i++) {
        len += sprintf(&buf[len], "%lf%s", self->vec[i], i < (self->count - 1) ? "," : "");
    }
    String ret = class_call(String, from_cstring, buf);
    free(buf);
    return ret;
}

Vec Vec_add(Vec self, Vec b) {
    Vec result = (Vec)new_obj((class_Base)self->cl, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = self->vec[i] + b->vec[i];
    return result;
}

Vec Vec_sub(Vec self, Vec b) {
    Vec result = (Vec)new_obj((class_Base)self->cl, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = self->vec[i] - b->vec[i];
    return result;
}

Vec Vec_mul(Vec self, Vec b) {
    Vec result = (Vec)new_obj((class_Base)self->cl, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = self->vec[i] * b->vec[i];
    return result;
}

Vec Vec_scale(Vec self, double scale) {
    Vec result = (Vec)new_obj((class_Base)self->cl, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = self->vec[i] * scale;
    return result;
}

double Vec_dot(Vec self, Vec b) {
    double result = 0.0;
    int count = min(self->count, b->count);
    for (int i = 0; i < count; i++)
        result += self->vec[i] * b->vec[i];
    return result;
}