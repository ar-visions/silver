
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
    MT_Method,
    MT_Constructor
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
    char *stack_var;
    int line;
    String file;
} Token;

struct _object_ClassDec;
struct _object_MemberDec;

#define _CX(D,T,C) _Base(spr,T,C)   \
    override(D,T,C,void,init,(C)) \
    method(D,T,C,String,super_out,(C,List,struct _object_ClassDec *,Token *,Token *)) \
    method(D,T,C,Token *,read_tokens,(C,List,List,int *)) \
    method(D,T,C,struct _object_ClassDec *,find_class,(String)) \
    method(D,T,C,bool,read_template_types,(C,struct _object_ClassDec *, Token **)) \
    method(D,T,C,String,code_out,(C, List, Token *, Token *, Token **, struct _object_ClassDec *, bool)) \
    method(D,T,C,int,read_expression,(C, Token *, Token **, Token **, const char *)) \
    method(D,T,C,void,read_property_blocks,(C, struct _object_ClassDec *, struct _object_MemberDec *)) \
    method(D,T,C,bool,read_modules,(C)) \
    method(D,T,C,bool,read_classes,(C)) \
    method(D,T,C,bool,replace_classes,(C, FILE *)) \
    method(D,T,C,void,declare_classes,(C, FILE *)) \
    method(D,T,C,void,define_module_constructor,(C, FILE *)) \
    method(D,T,C,void,effective_methods,(C, struct _object_ClassDec *, Pairs *)) \
    method(D,T,C,String,class_op_out,(C, List, Token *, \
        struct _object_ClassDec *, String, bool, Token **)) \
    method(D,T,C,String,args_out,(C, Pairs, struct _object_ClassDec *, struct _object_MemberDec *, bool, bool, int, bool)) \
    method(D,T,C,String,token_string,(C, Token *)) \
    method(D,T,C,void,resolve_supers,(C)) \
    method(D,T,C,void,token_out,(C, Token *, int, String)) \
    method(D,T,C,bool,process,(C, const char *)) \
    var(D,T,C,String,name)                 \
    var(D,T,C,Token *,tokens)              \
    var(D,T,C,List,modules)                \
    var(D,T,C,List,forward_structs)        \
    var(D,T,C,List,includes)               \
    var(D,T,C,Pairs,classes)               \
    var(D,T,C,Pairs,processed)
declare(CX, Base)

#define _MemberDec(D,T,C) _Base(spr,T,C)   \
    var(D,T,C,struct _object_ClassDec *,cd) \
    var(D,T,C,enum MemberType,member_type) \
    var(D,T,C,Token *,type)                \
    var(D,T,C,int,type_count)              \
    var(D,T,C,String,type_str)             \
    var(D,T,C,String,str_name)             \
    var(D,T,C,Token *,setter_var)          \
    var(D,T,C,Token *,getter_start)        \
    var(D,T,C,Token *,getter_end)          \
    var(D,T,C,Token *,setter_start)        \
    var(D,T,C,Token *,setter_end)          \
    var(D,T,C,Token *,block_start)         \
    var(D,T,C,Token *,block_end)           \
    var(D,T,C,Token *,array_start)         \
    var(D,T,C,Token *,array_end)           \
    var(D,T,C,Token **,arg_names)          \
    var(D,T,C,Token **,arg_types)          \
    var(D,T,C,int,arg_types_count)         \
    var(D,T,C,int *,at_token_count)        \
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
    var(D,T,C,Pairs,effective)             \
    var(D,T,C,Token *,start)               \
    var(D,T,C,Token *,end)                 \
    var(D,T,C,Token *,name)                \
    var(D,T,C,String,class_name)           \
    var(D,T,C,String,super_class)          \
    var(D,T,C,List,templates)              \
    var(D,T,C,Pairs,members)               \
    var(D,T,C,Pairs,meta)
declare(ClassDec, Base)