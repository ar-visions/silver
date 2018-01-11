#include <obj/obj.h>
#include <obj-cx/obj-cx.h>

#define CX_VERSION  "0.4.0"

implement(CX)
implement(ClassDec)
implement(MemberDec)

Token *CX_read_tokens(CX self, String str, int *n_tokens) {
    Token *tokens = array_struct(Token, str->length + 1);
    int nt = 0;
    enum TokenType seps[256];
    const char *separators = "[](){}.,+-&*~!/%<>=^|&?:;#";
    const char *puncts[] = {
        "[", "]", "(", ")", "{", "}", ".", "->", "^[", // <-- custom ^[meta]
        "++", "--", "&", "*", "+", "-", "~", "!",
        "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "^", "|", "&&", "||",
        "?", ":", ";", "...",
        "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|=",
        ",", "#", "##",
        "<:", ":>", "<%", "%>", "%:", "%:%:"
    };
    const char punct_is_op[] = {
        0, 0, 0, 0, 0, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0,
        0, 0, 0, 0, 0, 0
    };
    const char *keywords[] = {
        "lignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit",
        "atomic_noexcept","auto", "bitand","bitor","bool","break",
        "case","catch","char","char16_t","char32_t","class","compl",
        "concept","const","constexpr","const_cast","continue","co_await",
        "co_return","co_yield","decltype","default","delete","do","double",
        "dynamic_cast","else","enum","explicit","export","extern","false",
        "float","for","friend","goto","if","import","inline","int","long",
        "module","mutable","namespace","new","noexcept","not","not_eq",
        "nullptr","operator","or","or_eq","private","protected","public",
        "register","reinterpret_cast","requires","return","short","signed",
        "sizeof","static","static_assert","static_cast","struct","switch",
        "synchronized","template","this","thread_local","throw","true","try",
        "typedef","typeid","typename","union","unsigned","using","virtual",
        "void","volatile","wchar_t","while","xor","xor_eq"
    };
    const char type_keywords[] = {
        0,0,0,0,0,0,0,
        0,0,0,0,0,0,
        0,0,1,1,1,0,0,
        0,0,0,0,0,0,
        0,0,0,0,0,0,1,
        0,0,1,0,0,0,0,
        1,0,0,0,0,0,0,1,1,
        0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,
        0,0,0,0,0,0,
        0,0,0,0,1,0,
        0,0,0,0,0,0,0,
        0,0,0,0,1,0,0,
        0,0,1,0,0,0
    };
    int n_keywords = sizeof(keywords) / sizeof(const char *);
    int keyword_lens[n_keywords];
    bool in_string = false;
    const int n_puncts = sizeof(puncts) / sizeof(const char *);
    int punct_lens[n_puncts];

    for (int i = 0; i < n_puncts; i++)
        punct_lens[i] = strlen(puncts[i]);

    for (int i = 0; i < n_keywords; i++)
        keyword_lens[i] = strlen(keywords[i]);
    
    seps[0] = 1;
    for (int i = 1; i <= 255; i++) {
        char *in_separators = strchr(separators, i);
        if (strchr(separators, i) != NULL)
            seps[i] = TT_Separator;
        else if (i == '"' || i == '\'')
            seps[i] = TT_String_Literal;
        else
            seps[i] = TT_None;
    }
    const char backslash = '\\';
    bool was_backslash = false;
    Token *t = NULL;
    for (int i = 0; i < (str->length + 1); i++) {
        const char *value = &str->buffer[i];
        const char b = *value;
        bool ws = isspace(b) || (b == 0);
        enum TokenType sep = seps[b];

        if (t && t->type == TT_String_Literal) {
            if (t->string_term == b && !was_backslash) {
                size_t length = (size_t)(value - t->value) + 1;
                t->length = length;
                was_backslash = b == backslash;
                nt++;
                t = NULL;
            }
            continue;
        }
        if (ws) {
            if (t) {
                size_t length = (size_t)(value - t->value);
                if (t->type == TT_Identifier) {
                    for (int k = 0; k < n_keywords; k++) {
                        const char *keyword = keywords[k];
                        if (length == keyword_lens[k] && memcmp(t->value, keyword, length) == 0) {
                            t->type_keyword = type_keywords[k] ? keyword : NULL;
                            t->type = TT_Keyword;
                            t->keyword = keyword;
                        }
                    }
                }
                t->length = length;
                nt++;
                t = NULL;
            }
        } else {
            if (t && sep != t->sep) {
                size_t length = (size_t)(value - t->value);
                t->length = length;
                nt++;
                t = NULL;
            }
            if (!t) {
                t = (Token *)&tokens[nt];
                t->type = (b == '"' || b == '\'') ? TT_String_Literal : TT_Identifier;
                t->sep = sep;
                t->value = value;
                t->length = 1;
                t->string_term = (t->type == TT_String_Literal) ? b : 0;
                if (t->string_term)
                    continue;
            }
            size_t length = (size_t)(value - t->value) + 1;
            if (sep) {
                bool cont = false;
                if (t->sep) {
                    for (int p = 0; p < n_puncts; p++) {
                        const char *punct = puncts[p];
                        int punct_len = punct_lens[p];
                        if (punct_len == length && memcmp(value, punct, punct_len) == 0) {
                            t->punct = punct;
                            t->type = TT_Punctuator;
                            t->length = punct_len;
                            t->operator = (bool)punct_is_op[p];
                            cont  = true;
                            break;
                        }
                    }
                }
                if (!cont) {
                    t->length = length - 1;
                    nt++;
                    t = (Token *)&tokens[nt];
                    t->type = (b == '"' || b == '\'') ? TT_String_Literal : TT_Identifier;
                    t->sep = sep;
                    t->value = value;
                    t->length = 1;
                    t->string_term = (t->type == TT_String_Literal) ? b : 0;
                    if (t->string_term)
                        continue;
                }
            }
        }
        was_backslash = b == backslash;
    }
    *n_tokens = nt;
    return tokens;
}

bool CX_read_template_types(CX self, ClassDec cd, Token **pt) {
    Token *t = *pt;
    if (t->punct != "<")
        return true;
    // read template types
    bool expect_identifier = true;
    for (t++; t->value; t++) {
        Token *tname = t;
        if (tname->punct == ">") {
            if (!cd->templates) {
                printf("expected template type\n");
                exit(0);
            }
            break;
        }
        // expect identifier at even iterations
        if (expect_identifier) {
            if (tname->type == TT_Identifier) {
                if (!cd->templates)
                    cd->templates = new(List);
                list_push(cd->templates, class_call(String, new_from_bytes, tname->value, tname->length));
            } else {
                printf("expected identifier in template expression\n");
                exit(0);
            }
        } else {
            if (tname->punct != ",") {
                printf("expected ',' separator in template expression\n");
                exit(0);
            }
        }
        expect_identifier = !expect_identifier;
    }
    *pt = ++t;
    return true;
}

int CX_read_expression(CX self, Token *t, Token **first_op, const char *op) {
    int brace_depth = 0;
    int count = 0;
    int paren_depth = 0;

    if (first_op)
        *first_op = NULL;
    while (t->value) {
        if (brace_depth == 0 && t->punct == ";") {
            if (paren_depth != 0) {
                printf("expected ')' before ';' in expression\n");
                exit(0);
            }
            return count;
        } else if (t->punct == "{") {
            brace_depth++;
        } else if (t->punct == "}") {
            if (--brace_depth < 0) {
                printf("unexpected '}' in expression\n");
                exit(0);
            }
        } else if (t->punct == "(") {
            paren_depth++;
        } else if (t->punct == ")") {
            if (--paren_depth < 0) {
                printf("unexpected ')' in expression\n");
                exit(0);
            }
        } else if (t->operator && first_op && (t->punct == op) && !(*first_op)) {
            *first_op = t;
        }
        t++;
        count++;
    }
    if (t->value == NULL) {
        printf("expected end of expression\n");
        exit(0);
    }
    return count;
}

void expect_type(Token *token, enum TokenType type) {
    if (token->type != type) {
        printf("wrong token type\n");
        exit(0);
    }
}

bool CX_process(CX self, const char *file) {
    String str = class_call(String, from_file, file);
    int n_tokens = 0;
    self->tokens = call(self, read_tokens, str, &n_tokens);
    // gather up all class declarations
    
    printf("tokens:\n");

    // pass 1: collect classes
    for (Token *t = self->tokens; t->value; t++) {
        if (t->length == 7 && strncmp(t->value, "declare", t->length) == 0) {
            ClassDec cd = new(ClassDec);
            cd->name = ++t; // validate
            cd->members = new(Pairs);
            t++;
            call(self, read_template_types, cd, &t);
            if (t->punct == ":") {
                t++;
                if (t->type != TT_Identifier) {
                    printf("expected super class identifier after :\n");
                    exit(0);
                }
                String str_super = class_call(String, new_from_bytes, t->value, t->length);
                cd->super_class = str_super;
                t++;
            }
            if ((t++)->punct != "{") {
                printf("expected '{' character\n");
                exit(0);
            }
            // read optional keywords such as static and private
            while (t->punct != "}") {
                MemberDec md = new(MemberDec);
                for (int i = 0; i < 3; i++) {
                    if (t->keyword == "static") {
                        md->is_static = true;
                        t++;
                    } else if (t->keyword == "private") {
                        md->is_private = true;
                        t++;
                    }
                }
                Token *equals = NULL;
                int token_count = call(self, read_expression, t, &equals, "=");
                if (token_count == 0) {
                    if (md->is_static || md->is_private) {
                        printf("expected expression\n");
                        exit(0);
                    }
                    t++;
                    continue;
                }
                Token *t_last = &t[token_count - 1];
                if (t_last->punct == "]") {
                    // meta data
                    Token *found = NULL;
                    for (Token *tt = t_last; tt != t; tt--) {
                        if (tt->punct == "^[") {
                            found = tt;
                            bool expect_sep = false;
                            int kv = 0;
                            String key = NULL;
                            String value = NULL;
                            for (Token *p = tt; p != t_last; p++) {
                                if (expect_sep && p->punct != ":") {
                                    printf("expected ':' in meta data\n");
                                    exit(0);
                                } else if (!expect_sep) {
                                    String v = class_call(String, new_from_bytes, p->value, p->length);
                                    if (!key) {
                                        key = v;
                                    } else {
                                        value = v;
                                        if (!md->meta)
                                            md->meta = new(Pairs);
                                        pairs_add(md->meta, key, value);
                                        key = value = NULL;
                                    }
                                }
                                expect_sep = !expect_sep;
                            }
                            break;
                        }
                    }
                    if (found)
                        t_last = --found;
                    // read backwards until you see a [
                }
                if (t_last->punct == ")") {
                    // method
                    int paren_depth = 1;
                    for (Token *tt = t_last; tt != t; tt--) {
                        if (tt->punct == "(") {
                            if (--paren_depth == 0) {
                                md->args = ++tt;
                                paren_depth = 1;
                                for (; tt != t_last; tt++) {
                                    if (tt->punct == "(") {
                                        paren_depth++;
                                    } else if (tt->punct == ")") {
                                        paren_depth--;
                                    }
                                    md->args_count++;
                                }
                                // parse arguments
                            }
                        } else if (tt->punct == ")") {
                            paren_depth++;
                        }
                    }
                    if (!md->args) {
                        printf("expected '(' before ')'\n");
                        exit(0);
                    }
                    t_last = md->args - 1;
                    md->member_type = MT_Method;
                } else {
                    // property
                    md->member_type = MT_Prop;
                    if (equals) {
                        Token *tt = ++equals;
                        if (tt->punct == ";") {
                            printf("expected rvalue in property assignment\n");
                            exit(0);
                        }
                        md->assign = tt;
                        md->assign_count = 1;
                        for (tt++; tt->value && tt->punct != ";"; tt++) {
                            md->assign_count++;
                        }
                    }
                }
                if (t_last == t) {
                    printf("expected member\n");
                    exit(0);
                }
                if (!((t_last->type == TT_Keyword && !t_last->type_keyword) || t_last->type == TT_Identifier)) {
                    printf("expected member name\n");
                    exit(0);
                }

                if (t_last->type != TT_Identifier) {
                    printf("expected identifier (member name)\n");
                    exit(0);
                }
                String str_name = class_call(String, new_from_bytes, t_last->value, t_last->length);
                md->str_name = str_name;
                md->name = t_last;
                md->type = t;
                for (Token *tt = t; tt != t_last; tt++) {
                    md->type_count++;
                }
                if (md->type_count == 0) {
                    printf("expected type (member)\n");
                    exit(0);
                }
                pairs_add(cd->members, str_name, md);
            }
        }
    }

    // read expressions, look for class declarations
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("libobj CX processor -- version %s\n", CX_VERSION);
        printf("usage: obj-cx input.cx\n");
        exit(1);
    }
    const char *input = (const char *)argv[1];
    CX cx = new(CX);
    if (!call(cx, process, input))
        return 1;
    return 0;
}