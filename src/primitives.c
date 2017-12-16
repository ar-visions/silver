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
    UInt64 this = new(UInt64);
    this->value = value ? strtoull(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String UInt64_to_string(UInt64 this) {
    char str[32];
    sprintf(str, "%lld", this->value);
    return string(str);
}

Int64 Int64_from_string(String value) {
    Int64 this = new(Int64);
    this->value = value ? strtoll(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String Int64_to_string(Int64 this) {
    char str[32];
    sprintf(str, "%llu", this->value);
    return string(str);
}

UInt32 UInt32_from_string(String value) {
    UInt32 this = new(UInt32);
    this->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String UInt32_to_string(UInt32 this) {
    char str[16];
    sprintf(str, "%u", this->value);
    return string(str);
}

Int32 Int32_from_string(String value) {
    Int32 this = new(Int32);
    this->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String Int32_to_string(Int32 this) {
    char str[16];
    sprintf(str, "%d", this->value);
    return string(str);
}

UInt16 UInt16_from_string(String value) {
    UInt16 this = new(UInt16);
    this->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String UInt16_to_string(UInt16 this) {
    char str[8];
    sprintf(str, "%hu", this->value);
    return string(str);
}

Int16 Int16_from_string(String value) {
    Int16 this = new(Int16);
    this->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String Int16_to_string(Int16 this) {
    char str[8];
    sprintf(str, "%hu", this->value);
    return string(str);
}

UInt8 UInt8_from_string(String value) {
    UInt8 this = new(UInt8);
    this->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String UInt8_to_string(UInt8 this) {
    char str[8];
    sprintf(str, "%u", this->value);
    return string(str);
}

Int8 Int8_from_string(String value) {
    Int8 this = new(Int8);
    this->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String Int8_to_string(Int8 this) {
    char str[8];
    sprintf(str, "%d", this->value);
    return string(str);
}

Long Long_from_string(String value) {
    Long this = new(Long);
    this->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String Long_to_string(Long this) {
    char str[8];
    sprintf(str, "%ld", this->value);
    return string(str);
}

ULong ULong_from_string(String value) {
    ULong this = new(ULong);
    this->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return autorelease(this);
}

String ULong_to_string(ULong this) {
    char str[8];
    sprintf(str, "%lu", this->value);
    return string(str);
}

Float Float_from_string(String value) {
    Float this = new(Float);
    this->value = value ? atof(value->buffer) : 0;
    return autorelease(this);
}

String Float_to_string(Float this) {
    char str[256];
    sprintf(str, "%f", this->value);
    return string(str);
}

Double Double_from_string(String value) {
    Double this = new(Double);
    this->value = value ? atof(value->buffer) : 0;
    return autorelease(this);
}

String Double_to_string(Double this) {
    char str[256];
    sprintf(str, "%f", this->value);
    return string(str);
}

Bool Bool_from_string(String value) {
    Bool this = new(Bool);
    if (value->buffer) {
        String lower = call(value, lower);
        if (call(lower, cmp, "true") == 0 || call(lower, cmp, "1") == 0)
            this->value = true;
    }
    return autorelease(this);
}

String Bool_to_string(Bool this) {
    char str[256];
    sprintf(str, "%s", this->value == false ? "false" : "true");
    return string(str);
}