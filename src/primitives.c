#include <obj.h>

implement(Primitive)
implement_primitive(UInt64)
implement_primitive(Int64)
implement_primitive(UInt32)
implement_primitive(Int32)
implement_primitive(UInt16)
implement_primitive(Int16)
implement_primitive(UInt8)
implement_primitive(Int8)
implement_primitive(Long)
implement_primitive(ULong)
implement_primitive(Float)
implement_primitive(Double)
implement_primitive(Bool)

Int8 int8_object(int8 v)        { Int8   o = new(Int8);     o->value = v; return autorelease(o); }
UInt8 uint8_object(uint8 v)     { UInt8  o = new(UInt8);    o->value = v; return autorelease(o); }
Int16 int16_object(int16 v)     { Int16  o = new(Int16);    o->value = v; return autorelease(o); }
UInt16 uint16_object(uint16 v)  { UInt16 o = new(UInt16);   o->value = v; return autorelease(o); }
Int32 int32_object(int32 v)     { Int32  o = new(Int32);    o->value = v; return autorelease(o); }
UInt32 uint32_object(uint32 v)  { UInt32 o = new(UInt32);   o->value = v; return autorelease(o); }
Int64 int64_object(int64 v)     { Int64  o = new(Int64);    o->value = v; return autorelease(o); }
UInt64 uint64_object(uint64 v)  { UInt64 o = new(UInt64);   o->value = v; return autorelease(o); }
Long long_object(long v)        { Long   o = new(Long);     o->value = v; return autorelease(o); }
ULong ulong_object(ulong v)     { ULong  o = new(ULong);    o->value = v; return autorelease(o); }
Bool bool_object(bool v)        { Bool   o = new(Bool);     o->value = v; return autorelease(o); }
Float float_object(float v)     { Float  o = new(Float);    o->value = v; return autorelease(o); }
Double double_object(double v)  { Double o = new(Double);   o->value = v; return autorelease(o); }

UInt64 UInt64_from_string(String value) {
    UInt64 self = new(UInt64);
    self->value = value ? strtoull(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String UInt64_to_string(UInt64 self) {
    char str[32];
    sprintf(str, "%lld", self->value);
    return string(str);
}

Int64 Int64_from_string(String value) {
    Int64 self = new(Int64);
    self->value = value ? strtoll(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String Int64_to_string(Int64 self) {
    char str[32];
    sprintf(str, "%llu", self->value);
    return string(str);
}

UInt32 UInt32_from_string(String value) {
    UInt32 self = new(UInt32);
    self->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String UInt32_to_string(UInt32 self) {
    char str[16];
    sprintf(str, "%u", self->value);
    return string(str);
}

Int32 Int32_from_string(String value) {
    Int32 self = new(Int32);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String Int32_to_string(Int32 self) {
    char str[16];
    sprintf(str, "%d", self->value);
    return string(str);
}

UInt16 UInt16_from_string(String value) {
    UInt16 self = new(UInt16);
    self->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String UInt16_to_string(UInt16 self) {
    char str[8];
    sprintf(str, "%hu", self->value);
    return string(str);
}

Int16 Int16_from_string(String value) {
    Int16 self = new(Int16);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String Int16_to_string(Int16 self) {
    char str[8];
    sprintf(str, "%hu", self->value);
    return string(str);
}

UInt8 UInt8_from_string(String value) {
    UInt8 self = new(UInt8);
    self->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String UInt8_to_string(UInt8 self) {
    char str[8];
    sprintf(str, "%u", self->value);
    return string(str);
}

Int8 Int8_from_string(String value) {
    Int8 self = new(Int8);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String Int8_to_string(Int8 self) {
    char str[8];
    sprintf(str, "%d", self->value);
    return string(str);
}

Long Long_from_string(String value) {
    Long self = new(Long);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String Long_to_string(Long self) {
    char str[8];
    sprintf(str, "%ld", self->value);
    return string(str);
}

ULong ULong_from_string(String value) {
    ULong self = new(ULong);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(self);
}

String ULong_to_string(ULong self) {
    char str[8];
    sprintf(str, "%lu", self->value);
    return string(str);
}

Float Float_from_string(String value) {
    Float self = new(Float);
    self->value = value ? atof(value->buffer) : 0;
    return autorelease(self);
}

String Float_to_string(Float self) {
    char str[256];
    sprintf(str, "%f", self->value);
    return string(str);
}

Double Double_from_string(String value) {
    Double self = new(Double);
    self->value = value ? atof(value->buffer) : 0;
    return autorelease(self);
}

String Double_to_string(Double self) {
    char str[256];
    sprintf(str, "%f", self->value);
    return string(str);
}

Bool Bool_from_string(String value) {
    Bool self = new(Bool);
    if (value->buffer) {
        String lower = call(value, lower);
        if (call(lower, cmp, "true") == 0 || call(lower, cmp, "1") == 0)
            self->value = true;
    }
    return autorelease(self);
}

String Bool_to_string(Bool self) {
    char str[256];
    sprintf(str, "%s", self->value == false ? "false" : "true");
    return string(str);
}