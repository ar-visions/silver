
enum TokenType {
    TT_None = 0,
    TT_Separator,
    TT_Keyword,
    TT_Identifier,
    TT_Constant,
    TT_String_Literal,
    TT_Punctuator
};

enum MemberType {
    MT_Prop = 0,
    MT_Method
};

typedef struct _Token {
    enum TokenType type;
    enum TokenType sep;
    const char *value;
    const char *punct;
    size_t length;
    char string_term;
} Token;

#define _CX(D,T,C) _Base(spr,T,C)   \
    method(D,T,C,Token *,read_tokens,(C,String,int *)) \
    method(D,T,C,bool,read_template_types,(C,ClassDec, int *)) \
    method(D,T,C,bool,process,(C, const char *)) \
    var(D,T,C,Token *,tokens) \
declare(CX, Base)

#define _MemberDec(D,T,C) _Base(spr,T,C)   \
    var(D,T,C,enum MemberType,member_type) \
    var(D,T,C,Token *,type)                \
    var(D,T,C,Token *,name)                \
    var(D,T,C,bool,is_private)             \
    var(D,T,C,bool,is_static)              \
    var(D,T,C,Pairs,meta)                  \
    var(D,T,C,Pairs,args)
declare(MemberDec, Base)

#define _ClassDec(D,T,C) _Base(spr,T,C)    \
    var(D,T,C,Token *,name)                \
    var(D,T,C,List,templates)              \
    var(D,T,C,Pairs,members)               \
    var(D,T,C,Pairs,meta)
declare(ClassDec, Base)