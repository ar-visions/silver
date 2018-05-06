
#define CLOSURE_FLAG_SCOPED 1

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
struct _object_ClosureInst;
struct _object_ArrayClass;

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
    bool use_braces;
    struct _object_ClassDec *cd;
} Token;

#define CODE_FLAG_ALLOC 1

#define _CX(D,T,C) _Base(spr,T,C)   \
    override(D,T,C,void,init,(C)) \
    method(D,T,C,void,classdec_info,(C,struct _object_ClassDec *,String)) \
    method(D,T,C,void,pretty_token,(C,int,Token *,String)) \
    method(D,T,C,String,pretty_print,(C,String)) \
    method(D,T,C,String,closure_out,(C, List, struct _object_ClosureInst *, bool)) \
    method(D,T,C,struct _object_ClosureInst *,gen_closure,(C,List,Token *,int,Token *,Token *,struct _object_MemberDec *)) \
    method(D,T,C,String,forward_type,(C,struct _object_ClassDec *,struct _object_MemberDec *)) \
    method(D,T,C,Base,read_type_at,(C, Token *)) \
    method(D,T,C,String,code_block_out,(C, List, struct _object_ClassDec *, Token *, Token *, Token **, struct _object_MemberDec *, int *)) \
    method(D,T,C,void,code_block_end,(C, List, Token *, int *, String)) \
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
        struct _object_ClassDec *, String, bool, Token **, String *, struct _object_MemberDec *, int *, int *, bool, bool)) \
    method(D,T,C,String,args_out,(C, Pairs, struct _object_ClassDec *, \
                struct _object_MemberDec *, bool, bool, int, bool)) \
    method(D,T,C,struct _object_ClassDec *,scope_lookup,(C,List,String,Pairs *,String *,bool *)) \
    method(D,T,C,void,resolve_supers,(C)) \
    method(D,T,C,void,token_out,(C, Token *, int, String)) \
    method(D,T,C,bool,process,(C, const char *)) \
    method(D,T,C,bool,output,(C, const char *)) \
    method(D,T,C,bool,emit_module_statics,(C, FILE *, bool)) \
    method(D,T,C,String,inheritance_cast,(C, struct _object_ClassDec *, struct _object_ClassDec *)) \
    method(D,T,C,String,casting_name,(C, struct _object_ClassDec *, String, String)) \
    method(D,T,C,String,gen_var,(C, List, struct _object_ClassDec *, bool)) \
    method(D,T,C,String,start_tracking,(C, List, String, bool)) \
    method(D,T,C,String,scope_end,(C, List, Token *)) \
    method(D,T,C,int,read_block,(C,Token *,Token **,Token **)) \
    method(D,T,C,bool,is_tracking,(C, Pairs, String, bool *)) \
    method(D,T,C,String,var_op_out,(C, List , Token *, struct _object_ClassDec *, String, \
        bool, Token **, String *, int *, struct _object_MemberDec *, int *, bool, bool)) \
    method(D,T,C,String,var_gen_out,(C, List, struct _object_ClassDec *, String)) \
    method(D,T,C,void,resolve_member_types,(C, struct _object_ClassDec *)) \
    method(D,T,C,void,line_directive,(C, Token *, String)) \
    method(D,T,C,void,code_return,(C, List, Token *, Token **, Token **, struct _object_MemberDec *, String *, int *, String)) \
    method(D,T,C,C,find,(String))          \
    method(D,T,C,void,define_template_users,(C)) \
    method(D,T,C,struct _object_ArrayClass *,instance_array_dec,(C, String, Token *)) \
    method(D,T,C,struct _object_ClassDec *,read_class_from,(C, Token *, struct _object_ClassDec *)) \
    method(D,T,C,void,resolve_token,(C, Token *)) \
    var(D,T,C,Pairs,referenced_templates)  \
    var(D,T,C,int,directive_last_line)     \
    var(D,T,C,String,directive_last_file)  \
    var(D,T,C,String,name)                 \
    var(D,T,C,Token *,tokens)              \
    var(D,T,C,int,n_tokens)                \
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
    var(D,T,C,List,closures)               \
    var(D,T,C,List,misc_code)              \
    var(D,T,C,const char *,location)       \
    var(D,T,C,Pairs,processed)
declare(CX, Base)

#define _MemberDec(D,T,C) _Base(spr,T,C)   \
    method(D,T,C,void,read_args,(C, Token *, int, bool)) \
    var(D,T,C,struct _object_ClassDec *,cd) \
    var(D,T,C,enum MemberType,member_type) \
    var(D,T,C,Token *,type)                \
    var(D,T,C,int,type_count)              \
    var(D,T,C,struct _object_ClassDec *,type_cd) \
    var(D,T,C,String,type_str)             \
    var(D,T,C,String,str_name)             \
    var(D,T,C,String,str_args)             \
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
    var(D,T,C,bool *,arg_preserve)         \
    var(D,T,C,int,arg_types_count)         \
    var(D,T,C,int *,at_token_count)        \
    var(D,T,C,Token *,assign)              \
    var(D,T,C,int,assign_count)            \
    var(D,T,C,Token *,args)                \
    var(D,T,C,int,args_count)              \
    var(D,T,C,bool,is_weak)                \
    var(D,T,C,bool,is_private)             \
    var(D,T,C,bool,is_static)              \
    var(D,T,C,bool,is_const)               \
    var(D,T,C,bool,is_preserve)            \
    var(D,T,C,Pairs,meta)
declare(MemberDec, Base)

#define _ClosureInst(D,T,C) _MemberDec(spr,T,C)   \
    var(D,T,C,C,parent)                     \
    var(D,T,C,Pairs,ref_scope)              \
    var(D,T,C,String,code)
declare(ClosureInst, MemberDec)

#define _ClassDec(D,T,C) _Base(spr,T,C)    \
    override(D,T,C,void,init,(C))          \
    override(D,T,C,ulong,hash,(C))         \
    method(D,T,C,MemberDec,member_lookup,(C,String,C *)) \
    method(D,T,C,C,templated_instance,(C, CX, Token *, List)) \
    method(D,T,C,void,register_template_user,(C, List)) \
    method(D,T,C,void,replace_template_tokens,(C, CX)) \
    var(D,T,C,CX,m)                        \
    var(D,T,C,List,template_instances)     \
    var(D,T,C,List,defined_instances)      \
    var(D,T,C,C,instance_of)               \
    var(D,T,C,List,instance_args)          \
    var(D,T,C,List,template_users)         \
    var(D,T,C,C,parent)                    \
    var(D,T,C,Pairs,effective)             \
    var(D,T,C,Token *,start)               \
    var(D,T,C,Token *,end)                 \
    var(D,T,C,Token *,name)                \
    var(D,T,C,String,type_str)             \
    var(D,T,C,String,class_name)           \
    var(D,T,C,String,class_name_c)         \
    var(D,T,C,String,super_class)          \
    var(D,T,C,String,struct_class)         \
    var(D,T,C,String,struct_object)        \
    var(D,T,C,String,class_var)            \
    var(D,T,C,bool,is_template_dec)        \
    var(D,T,C,List,template_args)          \
    var(D,T,C,List,type_args)              \
    var(D,T,C,Pairs,members)               \
    var(D,T,C,Pairs,meta)
declare(ClassDec, Base)


#define _ClosureClass(D,T,C) _ClassDec(spr,T,C)    \
    override(D,T,C,void,init,(C))                  \
    var(D,T,C,MemberDec,md)                        \
    var(D,T,C,List,args)
declare(ClosureClass, ClassDec)

#define _ArrayClass(D,T,C) _ClassDec(spr,T,C)    \
    override(D,T,C,void,init,(C))                \
    var(D,T,C,String,array_type)                 \
    var(D,T,C,Token *,delim_start)               \
    var(D,T,C,Token *,delim_end)                 \
    var(D,T,C,ClassDec,object_container)
declare(ArrayClass, ClassDec)