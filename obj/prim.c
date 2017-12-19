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

Int8 int8_object(int8 v)        { Int8   o = auto(Int8);     o->value = v; return o; }
UInt8 uint8_object(uint8 v)     { UInt8  o = auto(UInt8);    o->value = v; return o; }
Int16 int16_object(int16 v)     { Int16  o = auto(Int16);    o->value = v; return o; }
UInt16 uint16_object(uint16 v)  { UInt16 o = auto(UInt16);   o->value = v; return o; }
Int32 int32_object(int32 v)     { Int32  o = auto(Int32);    o->value = v; return o; }
UInt32 uint32_object(uint32 v)  { UInt32 o = auto(UInt32);   o->value = v; return o; }
Int64 int64_object(int64 v)     { Int64  o = auto(Int64);    o->value = v; return o; }
UInt64 uint64_object(uint64 v)  { UInt64 o = auto(UInt64);   o->value = v; return o; }
Long long_object(long v)        { Long   o = auto(Long);     o->value = v; return o; }
ULong ulong_object(ulong v)     { ULong  o = auto(ULong);    o->value = v; return o; }
Bool bool_object(bool v)        { Bool   o = auto(Bool);     o->value = v; return o; }
Float float_object(float v)     { Float  o = auto(Float);    o->value = v; return o; }
Double double_object(double v)  { Double o = auto(Double);   o->value = v; return o; }

UInt64 UInt64_from_string(String value) {
    UInt64 self = auto(UInt64);
    self->value = value ? strtoull(value->buffer, NULL, 10) : 0;
    return self;
}

String UInt64_to_string(UInt64 self) {
    return class_call(String, format, "%llu", self->value);
}

Int64 Int64_from_string(String value) {
    Int64 self = auto(Int64);
    self->value = value ? strtoll(value->buffer, NULL, 10) : 0;
    return self;
}

String Int64_to_string(Int64 self) {
    return class_call(String, format, "%lld", self->value);
}

UInt32 UInt32_from_string(String value) {
    UInt32 self = auto(UInt32);
    self->value = value ? strtoul(value->buffer, NULL, 10) : 0;
    return self;
}

String UInt32_to_string(UInt32 self) {
    return class_call(String, format, "%u", self->value);
}

Int32 Int32_from_string(String value) {
    Int32 self = auto(Int32);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return self;
}

String Int32_to_string(Int32 self) {
    return class_call(String, format, "%d", self->value);
}

UInt16 UInt16_from_string(String value) {
    UInt16 self = auto(UInt16);
    self->value = value ? (uint16)strtoul(value->buffer, NULL, 10) : 0;
    return self;
}

String UInt16_to_string(UInt16 self) {
    return class_call(String, format, "%hu", self->value);
}

Int16 Int16_from_string(String value) {
    Int16 self = auto(Int16);
    self->value = value ? (int16)strtol(value->buffer, NULL, 10) : 0;
    return self;
}

String Int16_to_string(Int16 self) {
    return class_call(String, format, "%hd", self->value);
}

UInt8 UInt8_from_string(String value) {
    UInt8 self = auto(UInt8);
    self->value = value ? (uint8)strtoul(value->buffer, NULL, 10) : 0;
    return self;
}

String UInt8_to_string(UInt8 self) {
    return class_call(String, format, "%u", self->value);
}

Int8 Int8_from_string(String value) {
    Int8 self = auto(Int8);
    self->value = value ? (int8)strtol(value->buffer, NULL, 10) : 0;
    return self;
}

String Int8_to_string(Int8 self) {
    return class_call(String, format, "%d", self->value);
}

Long Long_from_string(String value) {
    Long self = auto(Long);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return self;
}

String Long_to_string(Long self) {
    return class_call(String, format, "%ld", self->value);
}

ULong ULong_from_string(String value) {
    ULong self = auto(ULong);
    self->value = value ? strtol(value->buffer, NULL, 10) : 0;
    return self;
}

String ULong_to_string(ULong self) {
    return class_call(String, format, "%lu", self->value);
}

Float Float_from_string(String value) {
    Float self = auto(Float);
    self->value = value ? (float)atof(value->buffer) : 0;
    return self;
}

String Float_to_string(Float self) {
    return class_call(String, format, "%f", self->value);
}

Double Double_from_string(String value) {
    Double self = auto(Double);
    self->value = value ? atof(value->buffer) : 0;
    return self;
}

String Double_to_string(Double self) {
    return class_call(String, format, "%f", self->value);
}

Bool Bool_from_string(String value) {
    Bool self = auto(Bool);
    if (value->buffer) {
        String lower = call(value, lower);
        if (call(lower, cmp, "true") == 0 || call(lower, cmp, "1") == 0)
            self->value = true;
    }
    return self;
}

String Bool_to_string(Bool self) {
    return class_call(String, format, "%s", self->value == false ? "false" : "true");
}