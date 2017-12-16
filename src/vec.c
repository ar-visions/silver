#include <obj.h>
#include <math.h>

implement(Vec)
implement(Vec3)
implement(Vec4)

void Vec_init(Vec this) {
    this->vec = &this->x;
    this->count = 2;
}

void Vec3_init(Vec3 this) {
    this->count = 3;
}

void Vec4_init(Vec4 this) { this->count = 4; }

ulong Vec_hash(Vec this) {
    ulong ret = 0;
    ulong p = 1;
    for (int i = 0; i < this->count; i++)
        ret += this->vec[i] * pow(31, p++);
    return ret;
}

Vec Vec_with(class c, ...) {
    Vec this = (Vec)new_obj((class_Base)c, 0);
    va_list args;
    va_start(args, c);
    for (int i = 0; i < this->count; i++)
        this->vec[i] = va_arg(args, double);
    va_end(args);
    return autorelease(this);
}

Vec Vec_with_count(int count) {
    Vec this = (Vec)new_obj((class_Base)Vec_cl, sizeof(double) * max(0, count - 2));
    return autorelease(this);
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
    Vec this = class_call(Vec, with_count, count);
    char *start = cvalue;
    int v = 0;
    for (int i = 0, len = strlen(cvalue); i <= len; i++) {
        if (cvalue[i] == ',' || cvalue[i] == 0) {
            cvalue[i] = 0;
            sscanf(start,"%lf",&this->vec[v++]);
            if (i != len)
                cvalue[i] = ',';
            start = &cvalue[i + 1];
        }
    }
    return this;
}

String Vec_to_string(Vec this) {
    char buf[64 + 64 * this->count];
    int len = 0;
    buf[0] = 0;
    for (int i = 0; i < this->count; i++) {
        len += sprintf(&buf[len], "%lf%s", this->vec[i], i < (this->count - 1) ? "," : "");
    }
    return class_call(String, from_cstring, buf);
}

Vec Vec_add(Vec this, Vec b) {
    Vec result = (Vec)new_obj((class_Base)this->class, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = this->vec[i] + b->vec[i];
    return result;
}

Vec Vec_sub(Vec this, Vec b) {
    Vec result = (Vec)new_obj((class_Base)this->class, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = this->vec[i] - b->vec[i];
    return result;
}

Vec Vec_mul(Vec this, Vec b) {
    Vec result = (Vec)new_obj((class_Base)this->class, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = this->vec[i] * b->vec[i];
    return result;
}

Vec Vec_scale(Vec this, double scale) {
    Vec result = (Vec)new_obj((class_Base)this->class, 0);
    for (int i = 0; i < result->count; i++)
        result->vec[i] = this->vec[i] * scale;
    return result;
}

double Vec_dot(Vec this, Vec b) {
    double result = 0.0;
    int count = min(this->count, b->count);
    for (int i = 0; i < count; i++)
        result += this->vec[i] * b->vec[i];
    return result;
}