#include <obj/obj.h>
#include <obj-cx/obj-cx.h>
#include <stdio.h>
#include <dirent.h>

#define CX_VERSION  "0.4.0"

implement(CX)
implement(ClassDec)
implement(MemberDec)

bool is_token(Token *t, const char *str) {
    int len = strlen(str);
    return t->length == len && strncmp(t->value, str, len) == 0;
}

MemberDec ClassDec_member_lookup(ClassDec self, String name) {
    for (ClassDec cd = self; cd; cd = cd->parent) {
        MemberDec md = pairs_value(cd->members, name, MemberDec);
        if (md)
            return md;
    }
    return NULL;
}

static List modules;
static CX find_module(const char *name) {
    CX m;
    each(modules, m) {
        if (call(m->name, cmp, name) == 0)
            return m;
    }
    return NULL;
}

FILE *module_file(const char *module_name, const char *file, const char *mode) {
    char buffer[256];
    sprintf(buffer, "%s/%s", module_name, file);
    return fopen(buffer, mode);
}

Token *CX_read_tokens(CX self, List module_contents, int *n_tokens) {
    int total_len = 0;
    String contents;
    each(module_contents, contents) {
        total_len += contents->length;
    }
    Token *tokens = array_struct(Token, total_len + 32 + 1);
    int nt = 0;
    enum TokenType seps[256];
    const char *separators = "[](){}.,+-&*~!/%<>=^|&?:;#";
    const char *puncts[] = {
        "[", "]", "(", ")", "{", "}", ".", "->", // <-- custom ^[meta]
        "++", "--", "&", "*", "+", "-", "~", "!",
        "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "^", "|", "&&", "||",
        "?", ":", ";", "...",
        "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|=",
        ",", "#", "##", "^[",
        "<:", ":>", "<%", "%>", "%:", "%:%:"
    };
    const char punct_is_op[] = {
        0, 0, 0, 0, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 1,
        0, 0, 0, 0, 0, 0
    };
    const char punct_is_assign[] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0,
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
        "void","volatile","wchar_t","while","xor","xor_eq",
        "get","set","construct"
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
    each(module_contents, contents) {
        for (int i = 0; i < (contents->length + 1); i++) {
            const char *value = &contents->buffer[i];
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
                    bool punct_find = true;
find_punct:
                    if (t->sep) {
                        for (int p = 0; p < n_puncts; p++) {
                            const char *punct = puncts[p];
                            int punct_len = punct_lens[p];
                            if (punct_len == length && memcmp(t->value, punct, punct_len) == 0) {
                                t->punct = punct;
                                t->type = TT_Punctuator;
                                t->length = punct_len;
                                t->operator = (bool)punct_is_op[p];
                                t->assign = (bool)punct_is_assign[p];
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
                        length = 1;
                        t->string_term = (t->type == TT_String_Literal) ? b : 0;

                        if (t->string_term)
                            continue;
                        if (punct_find) {
                            punct_find = false;
                            goto find_punct;
                        }
                    }
                }
            }
            was_backslash = b == backslash;
        }
    }
    #if 0
    for (int i = 0; i < nt; i++) {
        Token *t = &tokens[i];
        char buf[256];
        memcpy(buf, t->value, t->length);
        buf[t->length] = 0;
        printf("token:%s\n", buf);
    }
    #endif
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
                list_push(cd->templates, class_call(String, new_from_bytes, (uint8 *)tname->value, tname->length));
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

int CX_read_expression(CX self, Token *t, Token **b_start, Token **b_end, const char *end) {
    int brace_depth = 0;
    int count = 0;
    int paren_depth = 0;
    bool mark_block = b_start != NULL;
    const char *p1 = strchr(end, '{') ? "{" : "N/A";
    const char *p2 = strchr(end, ';') ? ";" : "N/A";
    const char *p3 = strchr(end, ')') ? ")" : "N/A";
    const char *p4 = strchr(end, ',') ? "," : "N/A";

    while (t->value) {
        if (t->punct == "(") {
            paren_depth++;
        } else if (t->punct == ")") {
            if (--paren_depth < 0) {
                printf("unexpected ')' in expression\n");
                exit(0);
            }
        }
        if (brace_depth == 0 && (t->punct == p1 || t->punct == p2 || t->punct == p3 || t->punct == p4)) {
            if (paren_depth == 0) {
                if (t->punct == "{" || (++t)->punct == "{") {
                    brace_depth = 1;
                    *b_start = t;
                    while ((++t)->value) {
                        if (t->punct == "{") {
                            brace_depth++;
                        } else if (t->punct == "}") {
                            if (--brace_depth == 0) {
                                *b_end = t;
                                return count;
                            }
                        }
                    }
                    printf("expected '}' to end block\n");
                    exit(0);
                }
                return count;
            }
        } else if (t->punct == "{") {
            brace_depth++;
        } else if (t->punct == "}") {
            if (--brace_depth < 0) {
                printf("unexpected '}' in expression\n");
                exit(0);
            }
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

void CX_resolve_supers(CX self) {
    KeyValue kv;
    CX m;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        if (strncmp(cd->name->value, "Super", 5) == 0) {
            int test = 0;
            test++;
        }
        if (!cd->parent && cd->super_class) {
            each(modules, m) {
                cd->parent = pairs_value(m->classes, cd->super_class, ClassDec);
                if (cd->parent)
                    break;
            }
        }
    }
}

void CX_read_property_blocks(CX self, ClassDec cd, MemberDec md) {
    int brace_depth = 0;
    int parens_depth = 0;
    const char *keyword_found = NULL;
    Token *start = NULL;
    for (Token *t = md->block_start; t->value; t++) {
        if (t->punct == "{") {
            if (++brace_depth == 2) {
                // read backwards to find signature
                if (!keyword_found) {
                    fprintf(stderr, "expected get/set\n");
                    exit(0);
                }
                start = t;
            }
        } else if (t->punct == "}") {
            if (--brace_depth == 1) {
                if (keyword_found == "get") {
                    md->getter_start = start;
                    md->getter_end = t;
                } else {
                    md->setter_start = start;
                    md->setter_end = t;
                }
            }
            if (brace_depth == 0)
                break;
        } else if (t->punct == "(") {
            parens_depth++;
        } else if (t->punct == ")") {
            parens_depth--;
        } else if (parens_depth == 1 && (t->type == TT_Identifier || t->type == TT_Keyword)) {
            if (keyword_found == "set")
                md->setter_var = t;
        }
        if (brace_depth == 1 && parens_depth == 0) {
            if (t->keyword == "get" || t->keyword == "set")
                keyword_found = t->keyword;
        }
    }
}

ClassDec CX_find_class(String name) {
    CX m;
    each(modules, m) {
        ClassDec cd = pairs_value(m->classes, name, ClassDec);
        if (cd)
            return cd;
    }
    return NULL;
}

/* read class information, including method and property blocks */
/* method expressions in the class must include a code block; this is optional for properties */
bool CX_read_classes(CX self) {
    self->classes = new(Pairs);
    for (Token *t = self->tokens; t->value; t++) {
        if (strncmp(t->value, "class", t->length) == 0) {
            Token *t_start = t;
            Token *t_name = ++t;
            String class_str = class_call(String, new_from_bytes, (uint8 *)t_name->value, t_name->length);
            ClassDec cd = pairs_value(self->classes, class_str, ClassDec); // only augment classes within module, so do not search globally
            if (!cd) {
                cd = new(ClassDec);
                pairs_add(self->classes, class_str, cd);
                cd->name = t_name; // validate
                cd->class_name = class_str;
                cd->members = new(Pairs);
                cd->start = t_start;
                // add _init
                MemberDec md = new(MemberDec);
                md->member_type = MT_Method;
                pairs_add(cd->members, string("_init"), md);
            }
            t++;
            call(self, read_template_types, cd, &t);
            if (t->punct == ":") {
                t++;
                if (t->type != TT_Identifier) {
                    printf("expected super class identifier after :\n");
                    exit(0);
                }
                String str_super = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
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
                md->cd = cd;
                for (int i = 0; i < 3; i++) {
                    if (t->keyword == "static") {
                        md->is_static = true;
                        t++;
                    } else if (t->keyword == "private") {
                        md->is_private = true;
                        t++;
                    }
                }
                Token *block_start = NULL, *block_end = NULL;
                int token_count = call(self, read_expression, t, &block_start, &block_end, "{;),");
                if (strncmp(t->value, "Method", 6) == 0) {
                    int test = 0;
                    test++;
                }
                if (token_count == 0) {
                    if (md->is_static || md->is_private) {
                        printf("expected expression\n");
                        exit(0);
                    }
                    t++;
                    continue;
                }
                Token *equals = NULL;
                bool fail_if_prop = false;
                if (!block_end) {
                    for (int e = 0; e < token_count; e++) {
                        if (t[e].punct == "=") {
                            equals = &t[e]; // set t_new_last here maybe
                            break;
                        }
                    }
                } else
                    fail_if_prop = true;
                Token *t_last = &t[token_count - 1];
                if (t[token_count].punct == ")") {
                    t_last++;
                    token_count++;
                }
                Token *t_new_last = t_last;
                if (t_last->punct == "]") {
                    bool found_meta = false;
                    for (Token *tt = t_last; tt != t; tt--) {
                        if (tt->punct == "^[") {
                            found_meta = true;
                            t_new_last = tt - 1;
                            break;
                        }
                    }
                    if (found_meta) {
                        // ^[meta data]
                        Token *found = NULL;
                        for (Token *tt = t_last; tt != t; tt--) {
                            if (tt->punct == "^[") {
                                found = tt;
                                bool expect_sep = false;
                                int kv = 0;
                                String key = NULL;
                                String value = NULL;
                                for (Token *p = ++tt; p != t_last; p++) {
                                    if (expect_sep && p->punct != ":") {
                                        printf("expected ':' in meta data\n");
                                        exit(0);
                                    } else if (!expect_sep) {
                                        String v = class_call(String, new_from_bytes, (uint8 *)p->value, p->length);
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
                    }

                    // parse array if this is one
                    if (t_new_last->punct == "]") {
                        bool check_start = false;
                        for (Token *tt = t_new_last; tt != t; tt--) {
                            if (tt->punct == "[") {
                                check_start = true;
                                continue;
                            }
                            if (check_start) {
                                if (tt->type == TT_Identifier) {
                                    md->array_start = tt + 1;
                                    md->array_end = t_new_last;
                                    t_new_last = tt;
                                    break;
                                } else if (tt->punct != "]") {
                                    printf("Invalid array definition\n");
                                    exit(0);
                                }
                            }
                        }
                    }
                }
                t_last = t_new_last;
                if (t_last->punct == ")") {
                    // method (or constructor)
                    int paren_depth = 0;
                    int n_args = -1;
                    for (Token *tt = t_last; tt != t; tt--, n_args++) {
                        if (tt->punct == "(") {
                            if (--paren_depth == 0) {
                                md->args = ++tt;
                                md->args_count = n_args;
                                break;
                            }
                        } else if (tt->punct == ")") {
                            paren_depth++;
                        }
                    }
                    if (n_args > 0) {
                        md->arg_names = alloc_bytes(sizeof(Token *) * n_args);
                        md->arg_types = alloc_bytes(sizeof(Token *) * n_args);
                        md->at_token_count = alloc_bytes(sizeof(int) * n_args);
                        Token *t_start = md->args;
                        Token *t_cur = md->args;
                        int paren_depth = 0;
                        int type_tokens = 0;
                        while (paren_depth >= 0) {
                            if (t_cur->punct == "," || (paren_depth == 0 && t_cur->punct == ")")) {
                                int tc = md->arg_types_count;
                                md->arg_names[tc] = t_cur - 1;
                                md->arg_types[tc] = t_start;
                                md->at_token_count[tc] = type_tokens - 1;
                                md->arg_types_count++;
                                t_start = t_cur + 1;
                                type_tokens = 0;
                            } else {
                                type_tokens++;
                            }
                            if (t_cur->punct == "(") {
                                paren_depth++;
                            } else if (t_cur->punct == ")") {
                                paren_depth--;
                            }
                            t_cur++;
                        }
                    }
                    if (!md->args) {
                        printf("expected '(' before ')'\n");
                        exit(0);
                    }
                    t_last = md->args - 2;
                    md->member_type = MT_Method;
                } else {
                    // property
                    md->member_type = MT_Prop;
                    if (equals) {
                        Token *tt = equals + 1;
                        if (tt->punct == ";") {
                            printf("expected rvalue in property assignment\n");
                            exit(0);
                        }
                        md->assign = tt;
                        md->assign_count = 1;
                        for (tt++; tt->value && tt->punct != ";"; tt++) {
                            md->assign_count++;
                        }
                        t_last = equals - 1;
                    }
                }
                Token *t_name = t_last;
                bool construct_default = false;
                if (t_last == t) {
                    if (t->keyword != "construct") {
                        printf("expected member\n");
                        exit(0);
                    } else {
                        // default constructor has its name set as the class
                        t_name = cd->name;
                        construct_default = true;
                    }
                }
                if (!construct_default && !md->array_start) {
                    // possible issue where arrays without identifiers could pass through; check above
                    if (!((t_last->type == TT_Keyword && !t_last->type_keyword) || t_last->type == TT_Identifier)) {
                        printf("expected member name\n");
                        exit(0);
                    }
                    if (t_last->type != TT_Identifier) {
                        printf("expected identifier (member name)\n");
                        exit(0);
                    }
                }
                String str_name = class_call(String, new_from_bytes, (uint8 *)t_name->value, t_name->length);
                md->str_name = str_name;
                md->name = t_name;
                md->block_start = block_start;
                md->block_end = block_end;
                md->type = t;
                if (t->keyword == "construct") {
                    md->member_type = MT_Constructor;
                    md->type = cd->name;
                    md->type_count = 1;
                } else {
                    for (Token *tt = t; tt != t_last; tt++) {
                        md->type_count++;
                    }
                    if (md->type_count == 0) {
                        printf("expected type (member)\n");
                        exit(0);
                    }
                }
                if (md->type_count > 0) {
                    md->type_str = new(String);
                    for (int i = 0; i < md->type_count; i++) {
                        call(md->type_str, concat_chars, md->type[i].value, md->type[i].length);
                        call(md->type_str, concat_char, ' ');
                    }
                }
                if (md->member_type == MT_Prop && block_start) {
                    // read get and set blocks
                    call(self, read_property_blocks, cd, md);
                }
                pairs_add(cd->members, str_name, md);
                if (block_end) {
                    t = block_end + 1;
                    if (md->member_type == MT_Prop) {
                        if (t->punct == "=") {
                            md->assign = ++t;
                            Token *assign_start = NULL;
                            Token *assign_end = NULL;
                            md->assign_count = call(self, read_expression, t, &assign_start, &assign_end, ";");
                            t += md->assign_count;
                        }
                    }
                } else
                    t += token_count + 1;
            }
            cd->end = t;
        }
    }
    return true;
}

String CX_token_string(CX self, Token *t) {
    if (!t->value)
        return NULL;
    return class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
}

String CX_class_op_out(CX self, List scope, Token *t,
        ClassDec cd, String target, bool is_instance, Token **t_after) {
    String output = new(String);
    char buf[1024];
    Token *t_start = t;
    String s_token = call(self, token_string, t);
    Token *t_member = t;
    MemberDec md = call(cd, member_lookup, s_token);
    
    if (!md) {
        printf("member: %s not found on class: %s\n", s_token->buffer, cd->class_name->buffer);
        exit(0);
    }
    if ((++t)->punct == "(") {
        sprintf(buf, "%s_cl->", cd->class_name->buffer);
        call(output, concat_cstring, buf);

        call(self, token_out, t_member, '(', output);
        if (is_instance) {
            call(output, concat_string, target);
            if (t->punct == ")")
                call(output, concat_char, ' ');
            else if (md->args_count > 0)
                call(output, concat_char, ',');
        }
        bool prepend_arg = false;
        if (md->member_type == MT_Prop) {
            fprintf(stderr, "invalid call to property\n");
            exit(0);
        }
        if (md->member_type == MT_Constructor) {
            prepend_arg = true;
            sprintf(buf, "%s_cl, %s", cd->class_name->buffer,
                is_token(t_start - 1, "auto") ? "true" : "false");
            call(output, concat_cstring, buf);
        }
        int n = call(self, read_expression, t, NULL, NULL, ")");
        if (n > 1) {
            if (prepend_arg) {
                sprintf(buf, ", ");
                call(output, concat_cstring, buf);
            }
            String code = call(self, code_out, scope, &t[1], &t[n - 1], t_after);
            call(output, concat_string, code);
        }
        t += n;
        if (t->punct != ")") {
            printf("expected ')'\n");
            exit(0);
        }
        call(self, token_out, t, ' ', output);
        //*t_after = t;
    } else if (t->assign) {
        Token *t_assign = t++;
        int n = call(self, read_expression, t, NULL, NULL, "{;),");
        if (n <= 0) {
            fprintf(stderr, "expected token after %s\n", t->punct);
            exit(0);
        }
        if (md->setter_start) {
            // call explicit setter [todo: handle the various assigner operators]
            sprintf(buf, "%s_cl->set_", cd->class_name->buffer);
            call(output, concat_cstring, buf);

            call(self, token_out, t_member, '(', output);
            if (is_instance) {
                sprintf(buf, "%s,", target->buffer);
                call(output, concat_cstring, buf);
            }
            String code = call(self, code_out, scope, t, &t[n - 1], t_after);
            call(output, concat_string, code);

            sprintf(buf, ")");
            call(output, concat_cstring, buf);

            call(self, token_out, &t[n], 0, output);
            t = *t_after;
        } else {
            // set object var
            if (is_instance) {
                call(output, concat_string, target);
                sprintf(buf, "->");
                call(output, concat_cstring, buf);
            } else {
                sprintf(buf, "%s_cl->", cd->class_name->buffer);
                call(output, concat_cstring, buf);
            }
            call(self, token_out, t_member, 0, output);
            call(self, token_out, t_assign, 0, output);

            String code = call(self, code_out, scope, t, &t[n], t_after);
            call(output, concat_string, code);
            t = *t_after;
        }
        // call setter if it exists (note: allow setters to be sub-classable, calling super.prop = value; and such)
    } else {
        if (md->getter_start) {
            // call explicit getter (class or instance)
            sprintf(buf, "%s_cl->get_", cd->class_name->buffer);
            call(output, concat_cstring, buf);
            call(self, token_out, t_member, '(', output);
            if (is_instance)
                call(output, concat_string, target);
            sprintf(buf, ")");
            call(output, concat_cstring, buf);
        } else {
            // get object->var (class or instance)
            if (is_instance) {
                call(output, concat_string, target);
                sprintf(buf, "->");
                call(output, concat_cstring, buf);
            } else {
                sprintf(buf, "%s_cl->", cd->class_name->buffer);
                call(output, concat_cstring, buf);
            }
            call(self, token_out, t_member, ' ', output);
        }
        t = *t_after = t_member;
    }
    if ((t + 1)->punct == ".") {
        for (int i = 0; i < md->type_count; i++) {
            String s = call(self, token_string, &md->type[i]);
            cd = CX_find_class(s);
            if (cd) {
                // needs to handle non-instance, something that returns Classes would be the case to handle
                output = call(self, class_op_out, scope, t + 2, cd, output, true, t_after); 
                break;
            }
            t += 2;
            break;
        }
    }
    return output;
}

String CX_code_out(CX self, List scope, Token *method_start, Token *method_end, Token **t_after) {
    String output = new(String);
    char buf[1024];
    int brace_depth = 0;
    int iter = 0;
    for (Token *t = method_start; ; t++) {
        iter++;
        // find 
        if (t->keyword == "new" || t->keyword == "auto")
            t++;
        
        if (t->punct == "{" || t->keyword == "for") {
            call(self, token_out, t, ' ', output);
            if (brace_depth++ != 0)
                list_push(scope, new(Pairs));
        } else if (t->punct == "}") {
            call(self, token_out, t, ' ', output);
            brace_depth--;
            if (brace_depth != 0)
                list_pop(scope, Pairs);
        } else if (t->type == TT_Identifier) {
            String target = call(self, token_string, t);
            // check if identifier exists as class
            String str_ident = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
            ClassDec cd = CX_find_class(str_ident);

            // convert:
            // self.method(2).prop = 1
            // B->set_prop(A->method(self, 2), 1)
            // refrain from using a fprintf() until the entire expression is processed

            if (cd) {
                // check for new/auto keyword before
                Token *t_start = t;
                if ((++t)->type == TT_Identifier) {
                    call(self, token_out, t - 1, ' ', output);
                    call(self, token_out, t, ' ', output);
                    // variable declared
                    String str_name = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                    Pairs top = (Pairs)call(scope, last);
                    pairs_add(top, str_name, cd);
                } else if (t->punct == ".") {
                    if ((++t)->type == TT_Identifier) {
                        String out = call(self, class_op_out, scope, t, cd, target, false, &t);
                        call(output, concat_string, out);
                    }
                } else if (t->punct == "(") {
                    // construct (default)
                    String out = call(self, class_op_out, scope, t - 1, cd, target, false, &t);
                    call(output, concat_string, out);
                } else {
                    call(self, token_out, t - 1, ' ', output);
                    call(self, token_out, t, ' ', output);
                }
            } else {
                ClassDec cd;
                LList *list = &scope->list;
                Pairs p = (Pairs)list->last->data;
                bool found = false;
                for (LItem *_i = list->last; _i; _i = _i->prev,
                        p = _i ? (typeof(p))_i->data : NULL) {
                    ClassDec cd = pairs_value(p, str_ident, ClassDec);
                    if (cd) {
                        if ((t + 1)->punct == ".") {
                            if ((t + 2)->type == TT_Identifier) {
                                String out = call(self, class_op_out, scope, t + 2, cd, target, true, &t);
                                call(output, concat_string, out);
                                found = true;
                            }
                        }
                        break;
                    }
                }
                if (!found)
                    call(self, token_out, t, ' ', output);
            }
        } else {
            call(self, token_out, t, 0, output);
        }
        if (t_after)
            *t_after = t;
        if (t >= method_end)
            break;
    }
    return output;
}

void CX_token_out(CX self, Token *t, int sep, String output) {
    char buf[1024];
    int n = 0;
    buf[0] = 0;

    for (int i = 0; i < t->length; i++) {
        n += sprintf(buf + n, "%c", t->value[i]);
    }
    const char *after = &t->value[t->length];
    while (*after && isspace(*after)) {
        n += sprintf(buf + n, "%c", *after);
        after++;
    }
    if (sep != ' ' && sep > 0)
        n += sprintf(buf + n, "%c", sep);

    call(output, concat_cstring, buf);
}

String CX_args_out(CX self, Pairs top, ClassDec cd, MemberDec md, bool inst, bool names, int aout, bool forwards) {
    String output = new(String);
    char buf[1024];
    if (inst) {
        if (aout > 0)
            call(output, concat_cstring, ", ");
        if (forwards) {
            sprintf(buf, "struct _%s *", cd->class_name->buffer);
            call(output, concat_cstring, buf);
        } else
            call(self, token_out, cd->name, ' ', output);
        if (names)
            call(output, concat_cstring, "self");
        if (top)
            pairs_add(top, new_string("self"), cd);
        aout++;
    }
    for (int i = 0; i < md->arg_types_count; i++) {
        if (aout > 0)
            call(output, concat_cstring, ", ");
        
        for (int ii = 0; ii < md->at_token_count[i]; ii++) {
            Token *t = &(md->arg_types[i])[ii];
            if (forwards) {
                String ts = call(self, token_string, t);
                ClassDec forward = CX_find_class(ts);
                if (forward) {
                    sprintf(buf, "struct _%s *", forward->class_name->buffer);
                    call(output, concat_cstring, buf);
                    continue;
                }
            }
            call(self, token_out, t, ' ', output);
        }
        if (names) {
            Token *t_name = md->arg_names[i];
            call(self, token_out, t_name, ' ', output);
            if (top && md->at_token_count[i] == 1) {
                String s_type = call(self, token_string, md->arg_types[i]);
                ClassDec cd_type = CX_find_class(s_type);
                if (cd_type) {
                    String s_var = call(self, token_string, t_name);
                    pairs_add(top, s_var, cd_type);
                }
            }
        }
        aout++;
    }
    return output;
}

void CX_effective_methods(CX self, ClassDec cd, Pairs *pairs) {
    if (cd->parent)
        call(self, effective_methods, cd->parent, pairs);
    KeyValue kv;
    each_pair(cd->members, kv) {
        String skey = inherits(kv->key, String);
        if (call(skey, cmp, "_init") != 0)
            pairs_add(*pairs, kv->key, kv->value);
    }
}

void CX_declare_classes(CX self, FILE *file_output) {
    String output = new(String);
    char buf[1024];
    KeyValue kv;
    each_pair(self->classes, kv) {
        String class_name = (String)kv->key;
        ClassDec cd = (ClassDec)kv->value;
        String super_name = cd->parent ? cd->parent->class_name : new_string("Base");
        bool is_class = call(class_name, cmp, "Class") == 0;
        Pairs m = new(Pairs);
        call(self, effective_methods, cd, &m);
        cd->effective = m;
        KeyValue mkv;
        {
            sprintf(buf,
                "\ntypedef struct _%s%s {\n"        \
                "\tstruct _%sClass *cl;\n"          \
                "\tint refs;\n"                     \
                "\tstruct _%sClass *parent;\n"      \
                "\tconst char *class_name;\n"       \
                "\tuint_t flags;\n"                 \
                "\tuint_t object_size;\n"           \
                "\tuint_t *member_count;\n"         \
                "\tchar *member_types;\n"           \
                "\tconst char **member_names;\n"    \
                "\tMethod **members[1];\n",
                    !is_class ? class_name->buffer : "Class",
                    !is_class ? "Class" : "",
                    !is_class ? "" : "Base",
                    !is_class ? (!cd->parent ? "" : cd->parent->class_name->buffer) : "");
            call(output, concat_cstring, buf);

            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                ClassDec origin = md->cd;
                // need to find actual symbol name
                switch (md->member_type) {
                    case MT_Constructor:
                    case MT_Method:
                        // need a way to gather forwards referenced in method returns, arguments, and property types
                        sprintf(buf, "\t");
                        call(output, concat_cstring, buf);
                        if (md->member_type == MT_Constructor) {
                            sprintf(buf, "%s", cd->class_name->buffer);
                            call(output, concat_cstring, buf);
                        } else {
                            for (int i = 0; i < md->type_count; i++) {
                                String ts = call(self, token_string, &md->type[i]);
                                ClassDec forward = CX_find_class(ts);
                                if (forward) {
                                    sprintf(buf, "struct _%s *", forward->class_name->buffer);
                                    call(output, concat_cstring, buf);
                                } else {
                                    call(self, token_out, &md->type[i], ' ', output);
                                }
                            }
                        }
                        String args = call(self, args_out, NULL, cd, md, md->member_type == MT_Method && !md->is_static, false, 0, true);
                        sprintf(buf, " (*%s)(%s);\n", md->str_name->buffer, args);
                        call(output, concat_cstring, buf);
                        break;
                    case MT_Prop:
                        sprintf(buf, "\t");
                        call(output, concat_cstring, buf);
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            String ts = call(self, token_string, t);
                            ClassDec forward = CX_find_class(ts);
                            if (forward) {
                                sprintf(buf, "struct _%s *", forward->class_name->buffer);
                                call(output, concat_cstring, buf);
                            } else
                                call(self, token_out, t, ' ', output);
                        }
                        if (md->is_static) {
                            sprintf(buf, " (*get_%s)();\n", md->str_name->buffer);
                            call(output, concat_cstring, buf);
                        } else {
                            sprintf(buf, " (*get_%s)(struct _%s *);\n", md->str_name->buffer,
                                cd->class_name->buffer);
                            call(output, concat_cstring, buf);
                        }
                        sprintf(buf, "\t");
                        call(output, concat_cstring, buf);
                        if (md->is_static) {
                            sprintf(buf, "void (*set_%s)(", md->str_name->buffer);
                            call(output, concat_cstring, buf);
                        } else {
                            sprintf(buf, "void (*set_%s)(struct _%s *, ", md->str_name->buffer, cd->class_name->buffer);
                            call(output, concat_cstring, buf);
                        }
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            String ts = call(self, token_string, t);
                            ClassDec forward = CX_find_class(ts);
                            if (forward) {
                                sprintf(buf, "struct _%s *", forward->class_name->buffer);
                                call(output, concat_cstring, buf);
                            } else
                                call(self, token_out, t, ' ', output);
                        }
                        sprintf(buf, ");\n");
                        call(output, concat_cstring, buf);
                        break;
                }
            }
            sprintf(buf, "} *%s%s;\n\n", cd->class_name->buffer, is_class ? "" : "Class");
            call(output, concat_cstring, buf);

            sprintf(buf, "extern %s%s %s_cl;\n\n", cd->class_name->buffer,
                is_class ? "" : "Class", cd->class_name->buffer);
            call(output, concat_cstring, buf);
        }

        if (!is_class) {
            sprintf(buf, "\ntypedef struct _%s {\n", class_name->buffer);
            call(output, concat_cstring, buf);

            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                if (md->member_type == MT_Prop && !md->getter_start) {
                    sprintf(buf, "\t");
                    call(output, concat_cstring, buf);
                    if (call(md->str_name, cmp, "cl") == 0) {
                        sprintf(buf, "%sClass cl;\n", cd->class_name->buffer);
                        call(output, concat_cstring, buf);
                    } else {
                        int is_self = -1;
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            if (t->length == cd->class_name->length &&
                                strncmp(t->value, cd->class_name->buffer, t->length) == 0)
                                    is_self = i;
                        }
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            if (i == is_self) {
                                sprintf(buf, "struct _%s *", cd->class_name->buffer);
                                call(output, concat_cstring, buf);
                            } else
                                call(self, token_out, t, ' ', output);
                        }
                        sprintf(buf, " %s;\n", md->str_name->buffer);
                        call(output, concat_cstring, buf);
                    }
                }
            }
            sprintf(buf, "} *%s;\n\n", cd->class_name->buffer);
            call(output, concat_cstring, buf);
        }
    }
    fprintf(file_output, "%s", output->buffer);
}

void CX_define_module_constructor(CX self, FILE *file_output) {
    String output = new(String);
    char buf[1024];

    KeyValue kv;
    {
        each_pair(self->classes, kv) {
            ClassDec cd = (ClassDec)kv->value;
            const char *class_name = cd->class_name->buffer;
            sprintf(buf, "%s%s %s_cl;\n",
                class_name, strcmp(class_name, "Class") == 0 ? "" : "Class",
                class_name);
            call(output, concat_cstring, buf);
        }
    }
    sprintf(buf, "\n");
    call(output, concat_cstring, buf);

    sprintf(buf, "static void module_constructor(void) __attribute__(constructor) {\n");
    call(output, concat_cstring, buf);

    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        int method_count = 0;
        KeyValue mkv;
        const char *class_name = cd->class_name->buffer;
        // function pointers
        sprintf(buf, "\t%s_cl = (typeof(%s_cl))alloc_bytes(sizeof(*%s_cl));\n",
            class_name, class_name, class_name);
        call(output, concat_cstring, buf);
        if (cd->parent) {
            sprintf(buf, "\t%s_cl->parent = %s_cl;\n",
                class_name, cd->parent->class_name->buffer);
            call(output, concat_cstring, buf);
        }
        sprintf(buf, "\t%s_cl->_init = %s__init;\n", class_name, class_name);
        call(output, concat_cstring, buf);

        each_pair(cd->effective, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            ClassDec origin = md->cd;
            switch (md->member_type) {
                case MT_Constructor:
                case MT_Method:
                    if (md->member_type == MT_Constructor) {
                        sprintf(buf, "\t%s_cl->%s = construct_%s_%s;\n",
                            class_name, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                        call(output, concat_cstring, buf);
                    } else {
                        sprintf(buf, "\t%s_cl->%s = %s_%s;\n",
                            class_name, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                        call(output, concat_cstring, buf);
                    }
                    method_count++;
                    break;
                case MT_Prop:
                    sprintf(buf, "\t%s_cl->get_%s = %s_get_%s;\n",
                        class_name, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                    call(output, concat_cstring, buf);
                    sprintf(buf, "\t%s_cl->set_%s = %s_set_%s;\n",
                        class_name, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                    call(output, concat_cstring, buf);
                    method_count += 2;
                    break;
            }
        }
        // member types
        {
            sprintf(buf, "\t%s_cl->member_types = (char *)malloc(%d);\n",
                class_name, method_count);
            call(output, concat_cstring, buf);

            int mc = 0;
            each_pair(cd->effective, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                switch (md->member_type) {
                    case MT_Constructor:
                    case MT_Method:
                        sprintf(buf, "\t%s_cl->member_types[%d] = %d;\n",
                            class_name, mc, md->member_type); mc++;
                        call(output, concat_cstring, buf);
                        break;
                    case MT_Prop:
                        sprintf(buf, "\t%s_cl->member_types[%d] = %d;\n",
                            class_name, mc, md->member_type); mc++;
                        call(output, concat_cstring, buf);

                        sprintf(buf, "\t%s_cl->member_types[%d] = %d;\n",
                            class_name, mc, md->member_type); mc++;
                        call(output, concat_cstring, buf);
                        break;
                }
            }
        }
        // member names
        {
            sprintf(buf, "\t%s_cl->member_names = (const char **)malloc(%d * sizeof(const char *));\n",
                class_name, method_count);
            call(output, concat_cstring, buf);

            int mc = 0;
            each_pair(cd->effective, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                switch (md->member_type) {
                    case MT_Constructor:
                    case MT_Method:
                        sprintf(buf, "\t%s_cl->member_names[%d] = \"%s\";\n",
                            class_name, mc, md->str_name->buffer); mc++;
                        call(output, concat_cstring, buf);
                        break;
                    case MT_Prop:
                        sprintf(buf, "\t%s_cl->member_names[%d] = \"%s\";\n",
                            class_name, mc, md->str_name->buffer); mc++;
                        call(output, concat_cstring, buf);

                        sprintf(buf, "\t%s_cl->member_names[%d] = \"%s\";\n",
                            class_name, mc, md->str_name->buffer); mc++;
                        call(output, concat_cstring, buf);
                        break;
                }
            }
        }
        sprintf(buf, "\t%s_cl->member_count = %d;\n", class_name, method_count);
        call(output, concat_cstring, buf);
    }
    sprintf(buf, "}\n");
    call(output, concat_cstring, buf);

    fprintf(file_output, "%s", output->buffer);
}

// verify the code executes as planned
bool CX_replace_classes(CX self, FILE *file_output) {
    String output = new(String);
    char buf[1024];

    List scope = new(List);
    list_push(scope, new(Pairs));
    KeyValue kv;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        KeyValue mkv;
        // output init
        sprintf(buf, "void %s__init(%s self) {\n",
            cd->class_name->buffer, cd->class_name->buffer);
        call(output, concat_cstring, buf);

        Pairs m = new(Pairs);
        call(self, effective_methods, cd, &m);
        {
            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                if (md->member_type == MT_Prop && md->assign) {
                    if (md->setter_start) {
                        sprintf(buf, "self->cl->set_%s(self, ", md->str_name->buffer);
                        call(output, concat_cstring, buf);
                    } else {
                        sprintf(buf, "self->%s = ", md->str_name->buffer);
                        call(output, concat_cstring, buf);
                    }
                    String code = call(self, code_out, scope, md->assign, &md->assign[md->assign_count - 1], NULL);
                    call(output, concat_string, code);

                    if (md->setter_start) {
                        sprintf(buf, ")");
                        call(output, concat_cstring, buf);
                    }
                    sprintf(buf, ";\n");
                    call(output, concat_cstring, buf);
                }
            }
        }
        sprintf(buf, "}\n");
        call(output, concat_cstring, buf);

        each_pair(cd->members, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            if (!md->block_start) {
                // for normal var declarations, output stub for getter / setter
                if (md->member_type == MT_Prop && !md->is_private && md->type_str) {
                    const char *tn = md->type_str->buffer;
                    const char *cn = cd->class_name->buffer;
                    const char *vn = md->str_name->buffer;
                    if (md->is_static) {
                        sprintf(buf, "%s %s_get_%s() { return (%s)%s_cl->%s; }\n",
                            tn, cn, vn, tn, cn, vn);
                        call(output, concat_cstring, buf);

                        sprintf(buf, "%s %s_set_%s(%s self, %s value) { return (%s)(%s_cl->%s = (typeof(self->%s))value); }\n",
                            tn, cn, vn, cn, vn, tn, cn, vn, vn);
                        call(output, concat_cstring, buf);
                    } else {
                        sprintf(buf, "%s %s_get_%s(%s self) { return (%s)self->%s; }\n",
                            tn, cn, vn, cn, tn, vn);
                        call(output, concat_cstring, buf);
                        
                        sprintf(buf, "%s %s_set_%s(%s self, %s value) { return (%s)(self->%s = (typeof(self->%s))value); }\n",
                            tn, cn, vn, cn, tn, tn, vn, vn);
                        call(output, concat_cstring, buf);
                    }
                }
                continue;
            }
            Pairs top = new(Pairs);
            list_push(scope, top);
            switch (md->member_type) {
                case MT_Constructor:
                    call(self, token_out, cd->name, ' ', output);
                    sprintf(buf, "%s_construct_%s(", cd->class_name->buffer, md->str_name->buffer);
                    call(output, concat_cstring, buf);

                    sprintf(buf, "struct _%sClass *cl, bool ar", cd->class_name->buffer);
                    call(output, concat_cstring, buf);

                    String args = call(self, args_out, top, cd, md, false, true, 1, false);
                    call(output, concat_string, args);

                    sprintf(buf, ") {\n");
                    call(output, concat_cstring, buf);

                    sprintf(buf, "%s self = Base_cl->object_new(cl, 0);\n",
                        cd->class_name->buffer);
                    call(output, concat_cstring, buf);

                    pairs_add(top, new_string("self"), cd);
                    if (md->block_start != md->block_end) {
                        String code = call(self, code_out, scope, md->block_start + 1, md->block_end - 1, NULL);
                        call(output, concat_string, code);
                    }
                    sprintf(buf, "return ar ? self->cl->auto(self) : self;\n}\n\n");
                    call(output, concat_cstring, buf);
                    break;
                case MT_Method: {
                    for (int i = 0; i < md->type_count; i++)
                        call(self, token_out, &md->type[i], ' ', output);
                    sprintf(buf, "%s_%s(", cd->class_name->buffer, md->str_name->buffer);
                    call(output, concat_cstring, buf);

                    String code = call(self, args_out, top, cd, md, !md->is_static, true, 0, false);
                    call(output, concat_string, code);

                    sprintf(buf, ")");
                    call(output, concat_cstring, buf);

                    code = call(self, code_out, scope, md->block_start, md->block_end, NULL);
                    call(output, concat_string, code);

                    sprintf(buf, "\n");
                    call(output, concat_cstring, buf);
                    // C#-style Main entry point; todo: use List or Pairs class instead of const char **
                    if (md->is_static && call(md->str_name, cmp, "Main") == 0) {
                        for (int i = 0; i < md->type_count; i++)
                            call(self, token_out, &md->type[i], ' ', output);
                        sprintf(buf, "main(");
                        call(output, concat_cstring, buf);

                        String args = call(self, args_out, top, cd, md, !md->is_static, true, 0, false);
                        call(output, concat_string, args);

                        sprintf(buf, ") {\n");
                        call(output, concat_cstring, buf);

                        sprintf(buf, "\treturn %s_%s(", cd->class_name->buffer, md->str_name->buffer);
                        call(output, concat_cstring, buf);

                        for (int i = 0; i < md->arg_types_count; i++) {
                            if (i > 0) {
                                sprintf(buf, ", ");
                                call(output, concat_cstring, buf);
                            }
                            Token *t_name = md->arg_names[i];
                            call(self, token_out, t_name, ' ', output);
                        }
                        sprintf(buf, ");\n}\n");
                        call(output, concat_cstring, buf);
                    }
                    break;
                }
                case MT_Prop: {
                    for (int i = 0; i < 2; i++) {
                        Token *block_start, *block_end;
                        if (i == 0) {
                            block_start = md->getter_start;
                            block_end = md->getter_end;
                        } else {
                            block_start = md->setter_start;
                            block_end = md->setter_end;
                        }
                        if (!block_start)
                            continue;
                        if (i == 0) {
                            for (int i = 0; i < md->type_count; i++)
                                call(self, token_out, &md->type[i], ' ', output);
                            sprintf(buf, "%s_get_", cd->class_name->buffer);
                            call(output, concat_cstring, buf);

                            call(self, token_out, md->name, '(', output);
                        } else {
                            for (int i = 0; i < md->type_count; i++)
                                call(self, token_out, &md->type[i], ' ', output);
                            sprintf(buf, "%s_set_", cd->class_name->buffer);
                            call(output, concat_cstring, buf);

                            call(self, token_out, md->name, '(', output);
                        }
                        if (!md->is_static) {
                            call(self, token_out, cd->name, ' ', output);
                            sprintf(buf, "self");
                            call(output, concat_cstring, buf);

                            pairs_add(top, new_string("self"), cd);
                            if (i == 1) {
                                sprintf(buf, ", ");
                                call(output, concat_cstring, buf);
                            }
                        }
                        if (i == 1) {
                            for (int i = 0; i < md->type_count; i++)
                                call(self, token_out, &md->type[i], ' ', output);
                            call(self, token_out, md->setter_var, ' ', output);
                            String s_type = call(self, token_string, md->setter_var);
                            ClassDec cd_type = CX_find_class(s_type);
                            if (cd_type) {
                                String s_var = call(self, token_string, md->setter_var);
                                pairs_add(top, s_var, cd_type);
                            }
                        }
                        sprintf(buf, ")");
                        call(output, concat_cstring, buf);
                        if (i == 0) {
                            String code = call(self, code_out, scope, block_start, block_end, NULL);
                            call(output, concat_string, code);
                        } else {
                            String code = call(self, code_out, scope, block_start, block_end - 1, NULL);
                            if (md->is_static)
                                sprintf(buf, "return %s_cl->get_%s();", cd->class_name->buffer, md->str_name->buffer);
                            else
                                sprintf(buf, "return self->get_%s(self);", md->str_name->buffer);
                            call(output, concat_cstring, buf);

                            code = call(self, code_out, scope, block_end, block_end, NULL);
                            call(output, concat_string, code);
                        }
                        sprintf(buf, "\n");
                        call(output, concat_cstring, buf);

                        call(top, clear);
                    }
                    break;
                }
            }
        }
    }
    fprintf(file_output, "%s", output->buffer);
    return true;
}

void CX_process_module(CX self, const char *location) {
}

static CX load_module(CX self, const char *name) {
    CX m = find_module(name);
    if (!m) {
        m = new(CX);
        list_push(self->modules, m); // should be a pair, where key is what the module is referred to as
        if (!modules)
            modules = new(List);
        list_push(modules, m);
        call(m, process, name);
        release(m);
    } else if (call(self->modules, index_of, base(m)) == -1) {
        list_push(self->modules, m);
    }
}

bool CX_read_modules(CX self) {
    if (call(self->name, cmp, "base") != 0) {
        load_module(self, "base");
    }
    for (Token *t = self->tokens; t->value; t++) {
        bool is_include = t->length == 7 && strncmp(t->value, "include", t->length) == 0;
        bool is_module = t->length == 6 && strncmp(t->value, "module", t->length) == 0;
        if (is_include || is_module) {
            int count = call(self, read_expression, t, NULL, NULL, ";");
            bool expect_comma = false;
            for (int i = 1; i < count; i++) {
                Token *tt = &t[i];
                if (expect_comma) {
                    if (tt->punct != ",") {
                        fprintf(stderr, "expected separator ',' between %s", (is_include ? "includes" : "modules"));
                        exit(0);
                    }
                } else {
                    // expect module name (TT_Identifier)
                    if (tt->type != TT_Identifier || tt->length >= 256) {
                        fprintf(stderr, "expected identifier for %s name", (is_include ? "include" : "module"));
                        exit(0);
                    }
                    char name[256];
                    memcpy(name, tt->value, tt->length);
                    name[tt->length] = 0;
                    if (is_include) {
                        list_push(self->includes, string(name));
                    } else
                        load_module(self, name);
                }
                expect_comma = !expect_comma;
            }
        }
    }
    return true;
}

void CX_init(CX self) {
    self->modules = new(List);
    self->includes = new(List);
}

bool CX_process(CX self, const char *location) {
    // read in module location, not a file
    // open all files in directory
    self->name = retain(class_call(String, from_cstring, location));
    DIR *dir = opendir(location);
    if (!dir) {
        fprintf(stderr, "cannot open module '%s'\n", location);
        exit(0);
    }
    struct dirent *ent;
    List module_contents = NULL;
    while ((ent = readdir(dir))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        int len = strlen(ent->d_name);
        if (len < 3)
            continue;
        if (strcmp(&ent->d_name[len - 3], ".sv") != 0)
            continue;
        char file[256];
        sprintf(file, "%s/%s", location, ent->d_name);
        //String file = class_call(String, format, "%s/%s", location, ent->d_name); [todo: fix bug somewhere in format, stepping on stack/memory somehow]
        String contents = class_call(String, from_file, string(file)->buffer);//file->buffer);
        if (!module_contents)
            module_contents = new(List);
        list_push(module_contents, contents);
    }
    closedir(dir);
    int n_tokens = 0;
    self->tokens = call(self, read_tokens, module_contents, &n_tokens);

    // read modules and includes first
    call(self, read_modules); // read includes as well
    call(self, read_classes);
    call(self, resolve_supers);

    // header output
    FILE *module_header = module_file(location, "module.h", "w+");
    String upper = call(self->name, upper);
    fprintf(module_header, "#ifndef __%s_H__\n", upper->buffer);
    fprintf(module_header, "#define __%s_H__\n", upper->buffer);
    CX mod;
    each(self->modules, mod) {
        fprintf(module_header, "#include <%s/module.h>\n", mod->name->buffer);
    }
    String include;
    each(self->includes, include) {
        fprintf(module_header, "#include <%s.h>\n", include->buffer);
    }
    call(self, declare_classes, module_header);
    fprintf(module_header, "#endif\n");
    fclose(module_header);

    // c code output
    FILE *module_code = module_file(location, "module.c", "w+");
    fprintf(module_code, "#include \"module.h\"\n");
    call(self, replace_classes, module_code);
    call(self, define_module_constructor, module_code);
    fclose(module_code);
    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("silver module preprocessor -- version %s\n", CX_VERSION);
        printf("usage: silver-mod module.json\n");
        exit(1);
    }
    const char *input = (const char *)argv[1];
    CX cx = new(CX);
    cx->name = retain(string(input));
    if (!call(cx, process, input))
        return 1;
    return 0;
}
