
class Primitive {
    enum Type enum_type;
};

class UInt64 : Primitive {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,uint64,value)
};

class Int64 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,int64,value)
};

class UInt32 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,uint32,value)
};

class Int32 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,int32,value)
};

class UInt16 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,uint16,value)
};

class Int16 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,int16,value)
};

class UInt8 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,uint8,value)
};

class Int8 : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,int8,value)
};

class Long : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,long,value)
};

class ULong : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,ulong,value)
};

class Float : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,float,value)
};

class Boolean : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,bool,value)
};

class Double : Primitive  {
    override void init(C);
    override String to_string(C);
    override C from_string(String);
    override int compare(C,C);
    var(D,T,C,double,value)
};

#define implement_primitive(C)  \
implement(C)                    \
                                \
void C##_init(C self) {         \
    self->enum_type = Type_##C; \
}                               \
int C##_compare(C self, C b) {  \
    if (self->value > b->value) \
        return 1;              \
    else if (self->value < b->value) \
        return -1;              \
    return 0;                   \
}
extern Int8     int8_object(int8);
extern UInt8    uint8_object(uint8);
extern Int16    int16_object(int16);
extern UInt16   uint16_object(uint16);
extern Int32    int32_object(int32);
extern UInt32   uint32_object(uint32);
extern Int64    int64_object(int64);
extern UInt64   uint64_object(uint64);
extern Long     long_object(long);
extern ULong    ulong_object(ulong);
extern Boolean  bool_object(bool);
extern Float    float_object(float);
extern Double   double_object(double);

#define Int     Int32
#define UInt    UInt32
