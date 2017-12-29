#ifndef _OBJ_HEADER_TEST_HH_
#define _OBJ_HEADER_TEST_HH_

#define _Test(D,T,C) _SuperTest(spr,T,C) \
	override(D,T,C,void,init,()) \
	var(D,T,C,int,var_test) 
declare(Test,SuperTest)

#define _EnumTest(D,T,C) _Enum(spr,T,C) \
	enum_object(D,T,C,Enum1,10) \
	enum_object(D,T,C,Enum2,30) \
	enum_object(D,T,C,Enum3,50) \
	enum_object(D,T,C,Enum4,70) 
enum_declare(EnumTest, Enum)

#endif
