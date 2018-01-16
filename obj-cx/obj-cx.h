
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
    const char *keyword;
    const char *type_keyword;
    bool assign;
    bool operator;
    size_t length;
    char string_term;
} Token;

struct _object_ClassDec;

#define _CX(D,T,C) _Base(spr,T,C)   \
    method(D,T,C,Token *,read_tokens,(C,String,int *)) \
    method(D,T,C,bool,read_template_types,(C,struct _object_ClassDec *, Token **)) \
    method(D,T,C,void,replace_method_block,(C, List, Token *, Token *, Token **, int *)) \
    method(D,T,C,int,read_expression,(C, Token *, Token **, Token **)) \
    method(D,T,C,bool,read_classes,(C)) \
    method(D,T,C,bool,replace_classes,(C)) \
    method(D,T,C,bool,replace_class_op,(C, Token *, Token *, \
        struct _object_ClassDec *, String)) \
    method(D,T,C,String,token_string,(C, Token *)) \
    method(D,T,C,void,resolve_supers,(C)) \
    method(D,T,C,bool,process,(C, const char *)) \
    var(D,T,C,Token *,tokens) \
    var(D,T,C,Pairs,classes)
declare(CX, Base)

#define _MemberDec(D,T,C) _Base(spr,T,C)   \
    var(D,T,C,enum MemberType,member_type) \
    var(D,T,C,Token *,type)                \
    var(D,T,C,int,type_count)              \
    var(D,T,C,String,str_name)             \
    var(D,T,C,Token *,block_start)         \
    var(D,T,C,Token *,block_end)           \
    var(D,T,C,Token *,name)                \
    var(D,T,C,Token *,assign)              \
    var(D,T,C,int,assign_count)            \
    var(D,T,C,Token *,args)                \
    var(D,T,C,int,args_count)              \
    var(D,T,C,bool,is_private)             \
    var(D,T,C,bool,is_static)              \
    var(D,T,C,bool,is_const)               \
    var(D,T,C,Pairs,meta)
declare(MemberDec, Base)

#define _ClassDec(D,T,C) _Base(spr,T,C)    \
    method(D,T,C,MemberDec,member_lookup,(C,String)) \
    var(D,T,C,C,parent)                    \
    var(D,T,C,Token *,start)               \
    var(D,T,C,Token *,end)                 \
    var(D,T,C,Token *,name)                \
    var(D,T,C,String,class_name)           \
    var(D,T,C,String,super_class)          \
    var(D,T,C,List,templates)              \
    var(D,T,C,Pairs,members)               \
    var(D,T,C,Pairs,meta)
declare(ClassDec, Base)