#ifndef _OBJ_HEADER_PRIM_H_
#define _OBJ_HEADER_PRIM_H_

#define _Primitive(D,T,C) _Base(spr,T,C) \
	var(D,T,C,enum,TypeEnum enum_type) 
declare(Primitive,Base);

#define _UInt64(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	private_var(D,T,C,uint64,value) 
declare(UInt64,Primitive);

#define _Int64(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	private_method(D,T,C,void,test,(C)) \
	var(D,T,C,int64,value) 
declare(Int64,Primitive);

#define _UInt32(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,uint32,value) 
declare(UInt32,Primitive);

#define _Int32(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,int32,value) 
declare(Int32,Primitive);

#define _UInt16(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,uint16,value) 
declare(UInt16,Primitive);

#define _Int16(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,int16,value) 
declare(Int16,Primitive);

#define _UInt8(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,uint8,value) 
declare(UInt8,Primitive);

#define _Int8(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,int8,value) 
declare(Int8,Primitive);

#define _Long(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,long,value) 
declare(Long,Primitive);

#define _ULong(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,ulong,value) 
declare(ULong,Primitive)

#define _Float(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,float,value) 
declare(Float,Primitive)

#define _Boolean(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,bool,value) 
declare(Boolean,Primitive)

#define _Double(D,T,C) _Primitive(spr,T,C) \
	override(D,T,C,void,init,()) \
	override(D,T,C,String,to_string,(C)) \
	override(D,T,C,C,from_string,(String)) \
	override(D,T,C,int,compare,(C,C)) \
	var(D,T,C,double,value) 
declare(Double,Primitive)

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

#endif
