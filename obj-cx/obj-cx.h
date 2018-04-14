
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

struct _object_ClassDec;
struct _object_MemberDec;

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
    String str;
    String file;
    bool skip;
    struct _object_ClassDec *cd;
} Token;

#define CODE_FLAG_ALLOC 1

#define _CX(D,T,C) _Base(spr,T,C)   \
    override(D,T,C,void,init,(C)) \
    method(D,T,C,String,super_out,(C,List,struct _object_ClassDec *,Token *,Token *)) \
    method(D,T,C,Token *,read_tokens,(C,List,List,int *)) \
    method(D,T,C,void,merge_class_tokens,(C,Token *,int *)) \
    method(D,T,C,struct _object_ClassDec *,find_class,(String)) \
    method(D,T,C,bool,read_template_types,(C,struct _object_ClassDec *, Token **)) \
    method(D,T,C,String,code_out,(C, List, Token *, Token *, Token **, struct _object_ClassDec *, \
                bool, String *, struct _object_MemberDec *, int *, int *, bool)) \
    method(D,T,C,int,read_expression,(C, Token *, Token **, Token **, const char *, int, bool)) \
    method(D,T,C,void,read_property_blocks,(C, struct _object_ClassDec *, struct _object_MemberDec *)) \
    method(D,T,C,bool,read_modules,(C)) \
    method(D,T,C,bool,read_classes,(C)) \
    method(D,T,C,bool,emit_implementation,(C, FILE *)) \
    method(D,T,C,void,declare_classes,(C, FILE *)) \
    method(D,T,C,void,define_module_constructor,(C, FILE *)) \
    method(D,T,C,void,effective_methods,(C, struct _object_ClassDec *, Pairs *)) \
    method(D,T,C,String,class_op_out,(C, List, Token *, \
        struct _object_ClassDec *, String, bool, Token **, String *, struct _object_MemberDec *, int *, int *, bool)) \
    method(D,T,C,String,args_out,(C, Pairs, struct _object_ClassDec *, \
                struct _object_MemberDec *, bool, bool, int, bool)) \
    method(D,T,C,struct _object_ClassDec *,scope_lookup,(C,List,String,Pairs *)) \
    method(D,T,C,void,resolve_supers,(C)) \
    method(D,T,C,void,token_out,(C, Token *, int, String)) \
    method(D,T,C,bool,process,(C, const char *)) \
    method(D,T,C,bool,emit_module_statics,(C, FILE *, bool)) \
    method(D,T,C,String,inheritance_cast,(C, struct _object_ClassDec *, struct _object_ClassDec *)) \
    method(D,T,C,String,casting_name,(C, struct _object_ClassDec *, String, String)) \
    method(D,T,C,String,gen_var,(C, List, struct _object_ClassDec *)) \
    method(D,T,C,String,start_tracking,(C, List, String, bool)) \
    method(D,T,C,String,scope_end,(C, List, Token *)) \
    method(D,T,C,int,read_block,(C,Token *,Token **,Token **)) \
    method(D,T,C,bool,is_tracking,(C, Pairs, String, bool *)) \
    method(D,T,C,String,var_gen_out,(C, List , Token *, struct _object_ClassDec *, String, \
        bool, Token **, String *, struct _object_MemberDec *, int *, bool)) \
    method(D,T,C,void,resolve_member_types,(C, struct _object_ClassDec *)) \
    var(D,T,C,String,name)                 \
    var(D,T,C,Token *,tokens)              \
    var(D,T,C,List,modules)                \
    var(D,T,C,int,gen_vars)                \
    var(D,T,C,List,forward_structs)        \
    var(D,T,C,List,includes)               \
    var(D,T,C,List,private_includes)       \
    var(D,T,C,Pairs,using_classes)         \
    var(D,T,C,Pairs,static_class_vars)     \
    var(D,T,C,Pairs,static_class_map)      \
    var(D,T,C,Pairs,aliases)               \
    var(D,T,C,Pairs,classes)               \
    var(D,T,C,Pairs,processed)
declare(CX, Base)

#define _MemberDec(D,T,C) _Base(spr,T,C)   \
    var(D,T,C,struct _object_ClassDec *,cd) \
    var(D,T,C,enum MemberType,member_type) \
    var(D,T,C,Token *,type)                \
    var(D,T,C,int,type_count)              \
    var(D,T,C,struct _object_ClassDec *,type_cd) \
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
    override(D,T,C,ulong,hash,(C))         \
    method(D,T,C,MemberDec,member_lookup,(C,String)) \
    var(D,T,C,C,parent)                    \
    var(D,T,C,Pairs,effective)             \
    var(D,T,C,Token *,start)               \
    var(D,T,C,Token *,end)                 \
    var(D,T,C,Token *,name)                \
    var(D,T,C,String,class_name)           \
    var(D,T,C,String,super_class)          \
    var(D,T,C,String,struct_class)         \
    var(D,T,C,String,struct_object)        \
    var(D,T,C,String,class_var)            \
    var(D,T,C,List,templates)              \
    var(D,T,C,Pairs,members)               \
    var(D,T,C,Pairs,meta)
declare(ClassDec, Base)