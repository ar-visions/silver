
enum TokenType {
    TT_Keyword = 0,
    TT_Identifier,
    TT_Constant,
    TT_String_Literal,
    TT_Punctuator
};

typedef struct _Token {
    enum TokenType type;
    bool sep;
    const char *value;
    const char *punct;
    size_t length;
    char string_term;
} Token;

#define _CX(D,T,C) _Base(spr,T,C)   \
    method(D,T,C,Token *,read_tokens,(C,String,int *)) \
    method(D,T,C,bool,process,(C, const char *))
declare(CX, Base)
