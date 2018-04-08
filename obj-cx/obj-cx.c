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

Token *token_next(Token **t) {
    do {
        (*t)++;
    } while ((*t)->skip);
    return *t;
}

MemberDec ClassDec_member_lookup(ClassDec self, String name) {
    for (ClassDec cd = self; cd; cd = cd->parent) {
        MemberDec md = pairs_value(cd->members, name, MemberDec);
        if (md)
            return md;
    }
    return NULL;
}

ulong ClassDec_hash(ClassDec self) {
    return call(self->class_name, hash);
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

Token *CX_read_tokens(CX self, List module_contents, List module_files, int *n_tokens) {
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
    int content_index = 0;

    each(module_contents, contents) {
        String current_file = (String)call(module_files, object_at, content_index++);
        int line_number = 1;
        for (int i = 0; i < (contents->length + 1); i++) {
            const char *value = &contents->buffer[i];
            const char b = *value;
            bool ws = isspace(b) || (b == 0);
            enum TokenType sep = seps[b];
            if (b == 10)
                line_number++;
            if (t && t->type == TT_String_Literal) {
                if (t->string_term == b && !was_backslash) {
                    size_t length = (size_t)(value - t->value) + 1;
                    t->length = length;
                    was_backslash = b == backslash;
                    t->str = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
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
                    t->str = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                    nt++;
                    t = NULL;
                }
            } else {
                if (t && sep != t->sep) {
                    size_t length = (size_t)(value - t->value);
                    t->length = length;
                    t->str = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                    nt++;
                    t = NULL;
                }
                if (!t) {
                    t = (Token *)&tokens[nt];
                    t->file = retain(current_file);
                    t->line = line_number;
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
                        t->str = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                        nt++;
                        t = (Token *)&tokens[nt];
                        t->file = retain(current_file);
                        t->line = line_number;
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

void CX_merge_class_tokens(CX self, Token *tokens, int *n_tokens) {
    // merge multiple tokens that assemble a single class identity together
    // also flag tokens as classes with their corresponding ClassDec
    for (int i = 0; i < *n_tokens; i++) {
        Token *t = &tokens[i];
        if (t->type == TT_Identifier) {
            t->cd = pairs_value(self->static_class_map, t->str, ClassDec);
        } else if (t->punct == "." && i > 0 && (t - 1)->length < 512 && (t + 1)->length < 512) {
            char buf[1024];
            int total = (t - 1)->length + 1 + (t + 1)->length;
            memcpy(buf, (t - 1)->value, total);
            buf[total] = 0;
            ClassDec cd = pairs_value(self->static_class_map, t->str, ClassDec);
            if (cd) {
                Token *t_start = t - 1;
                t_start->cd = cd;
                t->skip = true;
                (t + 1)->skip = true;
                t->length = total;
            }
        }
    }
}

bool CX_read_template_types(CX self, ClassDec cd, Token **pt) {
    Token *t = *pt;
    if (t->punct != "<")
        return true;
    // read template types
    bool expect_identifier = true;
    for (t++; t->value; t++) {
        if (t->skip) continue;
        Token *tname = t;
        if (tname->punct == ">") {
            if (!cd->templates) {
                printf("expected template type\n");
                exit(1);
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
                exit(1);
            }
        } else {
            if (tname->punct != ",") {
                printf("expected ',' separator in template expression\n");
                exit(1);
            }
        }
        expect_identifier = !expect_identifier;
    }
    token_next(&t);
    *pt = t;
    return true;
}

int CX_read_expression(CX self, Token *t, Token **b_start, Token **b_end, const char *end, int p_depth, bool args) {
    int brace_depth = 0;
    int count = 0;
    int paren_depth = p_depth;
    bool mark_block = b_start != NULL;
    const char *p1 = strchr(end, '{') ? "{" : "N/A";
    const char *p2 = strchr(end, ';') ? ";" : "N/A";
    const char *p3 = strchr(end, ')') ? ")" : "N/A";
    const char *p4 = strchr(end, ',') ? "," : "N/A";
    Token *t_start = t;

    while (t->value) {
        if (t->punct == "(") {
            paren_depth++;
        } else if (t->punct == ")") {
            paren_depth--;
        } else if (args) {
            if (t->punct == p4) {
                if (p_depth == paren_depth) {
                    paren_depth--;
                }
            }
        }
        if (brace_depth == 0 && (t->punct == p1 || t->punct == p2 || t->punct == p3 || t->punct == p4)) {
            if (paren_depth < 0 || (paren_depth == 0 && (t->punct == p1 || t->punct == p2 || t->punct == p4))) {
                if (mark_block && (t->punct == "{" || (token_next(&t))->punct == "{")) {
                    brace_depth = 1;
                    *b_start = t;
                    while (token_next(&t)->value) {
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
                    exit(1);
                }
                return count;
            }
        } else if (t->punct == "{") {
            brace_depth++;
        } else if (t->punct == "}") {
            if (--brace_depth < 0) {
                printf("unexpected '}' in expression\n");
                exit(1);
            }
        }
        token_next(&t);
        count++;
    }
    if (t->value == NULL) {
        printf("expected end of expression\n");
        exit(1);
    }
    return count;
}

void expect_type(Token *token, enum TokenType type) {
    if (token->type != type) {
        printf("wrong token type\n");
        exit(1);
    }
}

void CX_resolve_supers(CX self) {
    KeyValue kv;
    CX m;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
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
        if (t->skip)
            continue;
        if (t->punct == "{") {
            if (++brace_depth == 2) {
                // read backwards to find signature
                if (!keyword_found) {
                    fprintf(stderr, "expected get/set\n");
                    exit(1);
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
    for (Token *t = self->tokens; t->value; t++) {
        if (t->skip)
            continue;
        if (strncmp(t->value, "class", t->length) == 0) {
            Token *t_start = t;
            Token *t_name = token_next(&t);
            String class_str = class_call(String, new_from_bytes, (uint8 *)t_name->value, t_name->length);
            ClassDec cd = pairs_value(self->classes, class_str, ClassDec); // only augment classes within module, so do not search globally
            if (!cd) {
                cd = new(ClassDec);
                pairs_add(self->classes, class_str, cd);
                cd->name = t_name; // validate
                
                cd->class_name = class_str;
                cd->struct_object = new(String);
                call(cd->struct_object, concat_string, self->name);
                call(cd->struct_object, concat_char, '_');
                call(cd->struct_object, concat_string, class_str);

                cd->struct_class = new(String);
                call(cd->struct_class, concat_string, self->name);
                call(cd->struct_class, concat_char, '_');
                call(cd->struct_class, concat_string, class_str);
                if (call(class_str, cmp, "Class") != 0)
                    call(cd->struct_class, concat_cstring, "Class");
                
                cd->class_var = new(String);
                call(cd->class_var, concat_string, self->name);
                call(cd->class_var, concat_char, '_');
                call(cd->class_var, concat_string, class_str);
                call(cd->class_var, concat_cstring, "_var");

                cd->members = new(Pairs);
                cd->start = t_start;
                // add _init
                MemberDec md = new(MemberDec);
                md->member_type = MT_Method;
                pairs_add(cd->members, string("_init"), md);
            }
            token_next(&t);
            call(self, read_template_types, cd, &t);
            if (t->punct == ":") {
                token_next(&t);
                if (t->type != TT_Identifier) {
                    printf("expected super class identifier after :\n");
                    exit(1);
                }
                String str_super = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                cd->super_class = str_super;
                token_next(&t);
            } else if (call(class_str, cmp, "Base") != 0) {
                cd->super_class = new_string("Base");
            }
            if (t->punct != "{") {
                printf("expected '{' character\n");
                exit(1);
            }
            token_next(&t);
            // read optional keywords such as static and private
            while (t->punct != "}") {
                MemberDec md = new(MemberDec);
                md->cd = cd;
                for (int i = 0; i < 3; i++) {
                    if (t->keyword == "static") {
                        md->is_static = true;
                        token_next(&t);
                    } else if (t->keyword == "private") {
                        md->is_private = true;
                        token_next(&t);
                    }
                }
                Token *block_start = NULL, *block_end = NULL;
                int token_count = call(self, read_expression, t, &block_start, &block_end, "{;),", 0, false);
                if (token_count == 0) {
                    if (md->is_static || md->is_private) {
                        printf("expected expression\n");
                        exit(1);
                    }
                    token_next(&t);
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
                        if (tt->skip)
                            continue;
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
                            if (tt->skip)
                                continue;
                            if (tt->punct == "^[") {
                                found = tt;
                                bool expect_sep = false;
                                int kv = 0;
                                String key = NULL;
                                String value = NULL;
                                for (Token *p = ++tt; p != t_last; p++) {
                                    if (p->skip)
                                        continue;
                                    if (expect_sep && p->punct != ":") {
                                        printf("expected ':' in meta data\n");
                                        exit(1);
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
                            if (tt->skip)
                                continue;
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
                                    exit(1);
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
                        if (tt->skip)
                            continue;
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
                        exit(1);
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
                            exit(1);
                        }
                        md->assign = tt;
                        md->assign_count = 1;
                        for (tt++; tt->value && tt->punct != ";"; tt++) {
                            if (tt->skip)
                                continue;
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
                        exit(1);
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
                        exit(1);
                    }
                    if (t_last->type != TT_Identifier) {
                        printf("expected identifier (member name)\n");
                        exit(1);
                    }
                }
                String str_name = class_call(String, new_from_bytes, (uint8 *)t_name->value, t_name->length);
                md->str_name = str_name;
                md->block_start = block_start;
                md->block_end = block_end;
                md->type = t;
                for (Token *tt = t; tt != t_last; tt++) {
                    if (tt->skip)
                        continue;
                    md->type_count++;
                }
                if (md->type_count == 0) {
                    printf("expected type (member)\n");
                    exit(1);
                }
                if (md->type_count > 0) {
                    md->type_str = new(String);
                    bool first = true;
                    for (int i = 0; i < md->type_count; i++) {
                        if (md->type[i].skip)
                            continue;
                        if (!first)
                            call(md->type_str, concat_char, ' ');
                        call(md->type_str, concat_chars, md->type[i].value, md->type[i].length);
                        first = false;
                    }
                }
                if (md->member_type == MT_Prop && block_start) {
                    // read get and set blocks
                    call(self, read_property_blocks, cd, md);
                }
                if (call(md->str_name, cmp, "cast") == 0) {
                    md->is_static = true;
                    String arg_str = new(String);
                    for (int i = 0; i < md->args_count - 1; i++) {
                        call(arg_str, concat_string, md->args[i].str);
                    }
                    md->str_name = call(self, casting_name, cd, arg_str, md->type_str);
                    release(arg_str);
                }
                pairs_add(cd->members, md->str_name, md);
                if (block_end) {
                    t = block_end + 1;
                    if (md->member_type == MT_Prop) {
                        if (t->punct == "=") {
                            md->assign = token_next(&t);
                            Token *assign_start = NULL;
                            Token *assign_end = NULL;
                            md->assign_count = call(self, read_expression, t, &assign_start, &assign_end, ";", 0, false);
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

String CX_inheritance_cast(CX self, ClassDec expected, ClassDec given) {
    if (expected && given && (expected != given)) {
        ClassDec cd_cur = given;
        while (cd_cur && cd_cur != expected)
            cd_cur = cd_cur->parent;
        if (cd_cur)
            return cd_cur->struct_object;
    }
    return NULL;
}

void concat_cast_name(String out, String type) {
    bool a = true;
    for (int i = 0; i < type->length; i++) {
        char b = type->buffer[i];
        if (isalnum(b)) {
            if (!a) {
                call(out, concat_char, '_');
                a = true;
            }
            call(out, concat_char, b);
        } else if (b == '*') {
            b = 'p';
            call(out, concat_char, '_');
            call(out, concat_char, 'p');
            a = false;
        }
    }
}

String CX_casting_name(CX self, ClassDec cd, String from, String to) {
    String out = new(String);
    call(out, concat_cstring, "__cast__");
    if (call(from, compare, cd->class_name) == 0)
        from = string("Class");
    concat_cast_name(out, from);
    call(out, concat_cstring, "__");
    if (call(to, compare, cd->class_name) == 0)
        to = string("Class");
    concat_cast_name(out, to);
    return out;
}

String CX_class_op_out(CX self, List scope, Token *t,
        ClassDec cd, String target, bool is_instance, Token **t_after, String *type_last) {
    String output = new(String);
    char buf[1024];
    Token *t_start = t;
    String s_token = t->str;
    Token *t_member = t;
    MemberDec md = NULL;
    ClassDec cd_last = NULL;
    
    if (call(target, cmp, "class") == 0)
        cd = CX_find_class(string("Class"));

    if (cd)
        md = call(cd, member_lookup, s_token);
    if (md) {
        cd_last = md->type_cd;
        *type_last = md->type_cd ? md->type_cd->class_name : NULL;
    }
    bool constructor = false;
    if (cd && !md) {
        if (s_token->length == cd->class_name->length && 
                call(s_token, compare, cd->class_name) == 0) {
            constructor = true;
        }/* else {
            const char *class_name = cd->class_name->buffer;
            printf("member: %s not found on class: %s\n", s_token->buffer, class_name);
            exit(1);
        }*/
    }
    const char *class_var = cd ? cd->class_var->buffer : NULL;
    token_next(&t);
    if (cd && t->punct == "(") {
        if (constructor) {
            sprintf(buf, "(%s)Base->new_object(Base, (base_Class)%s, 0, %s",
                cd->struct_object->buffer, class_var,
                is_token(t_start - 1, "auto") ? "true" : "false");
            call(output, concat_cstring, buf);
            token_next(&t);
        } else {
            sprintf(buf, "%s->", class_var);
            call(output, concat_cstring, buf);
            call(self, token_out, t_member, '(', output);

            ClassDec cd_target = pairs_value(self->static_class_map, target, ClassDec);
            if (cd_target) {
                call(output, concat_string, cd_target->class_var);
            } else {
                call(output, concat_string, target);
            }
            if (t->punct == ")")
                call(output, concat_char, ' ');
            else if (md->args_count > 0)
                call(output, concat_char, ',');

            if (md->member_type == MT_Prop) {
                fprintf(stderr, "invalid call to property\n");
                exit(1);
            }
            int c_arg = 0;
            t++;
            for (;;) {
                String arg_output = new(String);
                int n = call(self, read_expression, t, NULL, NULL, ",)", 0, true);
                if (n == 0)
                    break;
                if (c_arg > 0)
                    call(output, concat_cstring, ", ");
                

                String type_returned = NULL;
                String code = call(self, code_out, scope, t, &t[n - 1], t_after, NULL, false, &type_returned);
                call(arg_output, concat_string, code);
                String cast = NULL;

                if (type_returned && c_arg < md->arg_types_count) {
                    ClassDec cd_returned = pairs_value(self->static_class_map, type_returned, ClassDec);
                    ClassDec cd_expected = NULL;
                    for (int ii = 0; ii < md->at_token_count[c_arg]; ii++) {
                        Token *t = &(md->arg_types[c_arg])[ii];
                        if (t->cd) {
                            cd_expected = t->cd;
                            break;
                        }
                    }
                    if (cd_returned && cd_expected) {
                        cast = call(self, inheritance_cast, cd_expected, cd_returned);
                        if (cast) {
                            sprintf(buf, "(%s)(", cast->buffer);
                            call(output, concat_cstring, buf);
                            call(arg_output, concat_char, ')');
                        }
                    }
                }
                call(output, concat_string, arg_output);
                release(arg_output);
                
                t += n;
                if (t->punct == ")")
                    break;
                else
                    t++;
                c_arg++;
            }
        }

        if (*t_after < t)
            *t_after = t;

        if (t->punct != ")") {
            fprintf(stderr, "%s:%d: expected ')'\n",
                t->file->buffer, t->line);
            exit(1);
        }
        call(self, token_out, t, ' ', output);
    } else if (constructor) {
        fprintf(stderr, "expected '(' for constructor\n");
        exit(1);
    } else {
        if (t->assign) {
            int n = call(self, read_expression, t, NULL, NULL, "{;),", 0, false);
            Token *t_assign = t;
            token_next(&t);
            if (--n <= 0) {
                fprintf(stderr, "expected token after %s\n", t->punct);
                exit(1);
            }
            String set_str = new(String);
            String expected_type;
            if (md) {
                expected_type = md->type_str;
            }
            if (md && md->setter_start) {
                // call explicit setter [todo: handle the various assigner operators]
                sprintf(buf, "%s->set_", class_var);
                call(set_str, concat_cstring, buf);

                call(self, token_out, t_member, '(', set_str);
                if (is_instance) {
                    sprintf(buf, "%s, ", target->buffer);
                    call(set_str, concat_cstring, buf);
                }
            } else {
                if (md) {
                    // set object var
                    if (is_instance) {
                        call(set_str, concat_string, target);
                        sprintf(buf, "->");
                        call(set_str, concat_cstring, buf);
                    } else {
                        sprintf(buf, "%s->", class_var);
                        call(set_str, concat_cstring, buf);
                    }
                } else {
                    ClassDec cd_lookup = call(self, scope_lookup, scope, target);
                    if (cd_lookup)
                        expected_type = cd_lookup->class_name;
                }
                call(self, token_out, t_member, 0, set_str);
                call(self, token_out, t_assign, 0, set_str);
            }

            String type_returned = NULL;
            String code = call(self, code_out, scope, t, &t[n - 1], t_after, NULL, false, &type_returned);
            t = *t_after;
            String cast = NULL;
            if (type_returned) {
                ClassDec cd_returned = pairs_value(self->static_class_map, type_returned, ClassDec);
                ClassDec cd_expected = pairs_value(self->static_class_map, expected_type, ClassDec);
                cast = call(self, inheritance_cast, cd_expected, cd_returned);
            }
            if (cast) {
                sprintf(buf, "(%s)(%s)", cast->buffer, code->buffer);
                call(output, concat_cstring, buf);
            } else {
                call(set_str, concat_string, code);
            }
            call(output, concat_string, set_str);
            if (md && md->setter_start) {
                sprintf(buf, ")");
                call(output, concat_cstring, buf);
            }
            release(set_str);
            t = *t_after;
        } else if (md) {
            if (md->getter_start) {
                // call explicit getter (class or instance)
                sprintf(buf, "%s->get_", class_var);
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
                    sprintf(buf, "%s->", class_var);
                    call(output, concat_cstring, buf);
                }
                call(self, token_out, t_member, ' ', output);
            }
            t = *t_after = t_member;
        } else {
            ClassDec cd_lookup = call(self, scope_lookup, scope, target);
            cd_last = cd_lookup;
            *type_last = cd_lookup->class_name; // todo: map to aliased name
            call(self, token_out, t_member, ' ', output);
            t = *t_after = t_member;
        }
    }
    // unsure about this one -- i need to use the effective type eval'd from this code above
    // that should be cd_lookup
    if (cd_last && (t + 1)->punct == ".") {
        output = call(self, class_op_out, scope, t + 2, cd_last, output, true, t_after, type_last); 
    }
    /*
    if (md && (t + 1)->punct == ".") {
        for (int i = 0; i < md->type_count; i++) {
            String s = md->type[i].str;
            cd = CX_find_class(s);
            if (cd) {
                output = call(self, class_op_out, scope, t + 2, cd, output, true, t_after, cd_last); 
                break;
            }
            t += 2;
            break;
        }
    }*/
    return output;
}

ClassDec CX_scope_lookup(CX self, List scope, String var) {
    LList *list = &scope->list;
    Pairs p = (Pairs)list->last->data;
    for (LItem *_i = list->last; _i; _i = _i->prev,
            p = _i ? (typeof(p))_i->data : NULL) {
        ClassDec cd = pairs_value(p, var, ClassDec);
        if (cd)
            return cd;
    }
    return NULL;
}

String CX_code_out(CX self, List scope, Token *method_start, Token *method_end, Token **t_after,
        ClassDec super_mode, bool line_no, String *type_last) {
    *type_last = NULL;
    String output = new(String);
    char buf[1024];
    int brace_depth = 0;
    bool first = true;
    for (Token *t = method_start; ; t++) {
        if (t->skip)
            continue;
        // find 
        if (t->keyword == "new" || t->keyword == "auto")
            token_next(&t);
        if (t->punct == "{" || t->keyword == "for") {
            call(self, token_out, t, ' ', output);
            if (first) {
                while (output->length > 0) {
                    if (isspace(output->buffer[output->length - 1])) {
                        output->length--;
                        output->buffer[output->length] = 0;
                    } else {
                        break;
                    }
                }
                if (super_mode) {
                    String super_code = call(self, super_out, scope, super_mode, method_start, method_end);
                    if (super_code) {
                        call(output, concat_string, super_code);
                        release(super_code);
                    }
                }
                if (line_no) {
                    char line_number[1024];
                    int extra_lines = 1;
                    if (output->length > 0 && output->buffer[output->length - 1] != '\n') {
                        call(output, concat_char, '\n');
                        //extra_lines = 0;
                    }
                    sprintf(line_number, "# %d \"%s\"\n", method_start->line + extra_lines, method_start->file->buffer);
                    call(output, concat_cstring, line_number);
                }
            }

            if (brace_depth++ != 0)
                list_push(scope, new(Pairs));
            first = false;
        } else if (t->punct == "}") {
            call(self, token_out, t, ' ', output);
            brace_depth--;
            if (brace_depth != 0)
                list_pop(scope, Pairs);
        } else if (t->punct == "(") {
            Token *t_prev = t - 1;
            Token *t_next = t + 1;
            if (strncmp(t_next->value, "char", 4) == 0) {
                int test = 0;
                test++;
            }
            bool token_out = true;
            if ((t_next->type == TT_Identifier || t_next->type_keyword) && t_prev->type != TT_Identifier && t_prev->type != TT_Keyword) {
                Token *t_from;
                String type_expected = new(String);
                bool first = true;
                for (Token *t_search = t + 1; t_search; t_search++) {
                    if (t_search == method_end)
                        break;
                    else if (t_search->punct == ")") {
                        t_from = t_search + 1;
                        break;
                    }
                    if (!first)
                        call(type_expected, concat_char, ' ');
                    call(type_expected, concat_string, t_search->str);
                    first = false;
                }
                if (t_after)
                    *t_after = t_from;
                int token_count = call(self, read_expression, t_from, NULL, NULL, "{;),", 0, false);
                String code = call(self, code_out, scope, t_from, &t_from[token_count - 1], t_after, NULL, false, type_last);
                String type_returned = *type_last;
                
                if (type_returned && type_expected && call(type_returned, compare, type_expected) != 0) {
                    ClassDec cd_from = pairs_value(self->static_class_map, type_returned, ClassDec);
                    ClassDec cd_to = pairs_value(self->static_class_map, type_expected, ClassDec);
                    bool class_to = true;
                    if (!cd_from != !cd_to) {
                        ClassDec cd;
                        if (cd_from) {
                            // Class to primitive
                            cd = cd_from;
                            class_to = false;
                        } else {
                            // primitive to Class
                            cd = cd_to;
                        }
                        String name = call(self, casting_name, cd, type_returned, type_expected);
                        ClassDec cd_search = cd;
                        MemberDec md_cast = NULL;
                        while (cd_search) {
                            md_cast = pairs_value(cd_search->members, name, MemberDec);
                            if (md_cast)
                                break;
                            cd_search = cd_search->parent;
                        }
                        if (md_cast) {
                            sprintf(buf, "(%s->%s(%s, (%s)))",
                                cd->class_name->buffer, md_cast->str_name->buffer,
                                cd->class_name->buffer, code->buffer);
                            call(output, concat_cstring, buf);
                            t = &t_from[token_count - 1];
                            token_out = false;
                        }
                    }
                }
                release(type_expected);
            }
            if (token_out)
                call(self, token_out, t, 0, output);
        } else if (t->type == TT_Identifier) {
            String target = t->str;
            ClassDec cd = NULL;
            bool is_class = false;
            // check for 'Class' in the token
            if (target->length > 5 && strcmp(&target->buffer[target->length - 5], "Class") == 0) {
                String sub = class_call(String, new_from_bytes, (uint8 *)target->buffer, t->length - 5); 
                cd = pairs_value(self->static_class_map, sub, ClassDec);
                is_class = cd != NULL;
            } else
                cd = t->cd;

            if (cd) {
                // check for new/auto keyword before
                Token *t_start = t;
                token_next(&t);
                if (t->type == TT_Identifier) {
                    call(output, concat_string, is_class ? cd->struct_class : cd->struct_object);
                    call(output, concat_char, ' ');
                    // variable declared
                    String str_name = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                    Pairs top = (Pairs)call(scope, last);
                    target = t->str;
                    pairs_add(top, str_name, cd);
                    String out = call(self, class_op_out, scope, t, cd, target, false, &t, type_last);
                    call(output, concat_string, out);
                } else if (t->punct == ".") {
                    token_next(&t);
                    if (t->type == TT_Identifier) {
                        String out = call(self, class_op_out, scope, t, cd, target, false, &t, type_last);
                        call(output, concat_string, out);
                    }
                } else if (t->punct == "(") {
                    if (is_class) {
                        fprintf(stderr, "constructor cannot be called directly on class type\n");
                        exit(1);
                    }
                    // construct (default)
                    String out = call(self, class_op_out, scope, t - 1, cd, target, false, &t, type_last);
                    call(output, concat_string, out);
                } else {
                    call(output, concat_string, is_class ? cd->struct_class : cd->struct_object);
                    call(output, concat_char, ' ');
                    call(self, token_out, t, ' ', output);
                }
            } else {
                ClassDec cd = call(self, scope_lookup, scope, target);
                bool found = false;
                if (cd) {
                    if ((t + 1)->punct == ".") {
                        if ((t + 2)->type == TT_Identifier) {
                            String out = call(self, class_op_out, scope, t + 2, cd, target, true, &t, type_last);
                            call(output, concat_string, out);
                            found = true;
                        }
                    }
                }
                if (!found)
                    call(self, token_out, t, ' ', output);
            }
        } else {
            if (t->type == TT_String_Literal)
                *type_last = string("char *");
            call(self, token_out, t, 0, output);
        }
        if (t_after)
            *t_after = t;
        if (t >= method_end)
            break;
    }
    if (!*type_last && output->length > 0) {
        ClassDec cd_found = call(self, scope_lookup, scope, output);
        if (cd_found)
            *type_last = cd_found->class_name;
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
    ClassDec cd_origin = md->cd;
    String output = new(String);
    char buf[1024];
    char scope_class[256];

    if (aout > 0)
        call(output, concat_cstring, ", ");
    
    sprintf(scope_class, "%s",
        md->is_static ? cd->struct_class->buffer : cd->struct_object->buffer);
    if (forwards) {
        sprintf(buf, "struct _%s *", scope_class);
        call(output, concat_cstring, buf);
    } else {
        call(output, concat_cstring, scope_class);
        call(output, concat_cstring, " ");
    }
    const char *self_var = md->is_static ? "class" : "self";
    if (names)
        call(output, concat_cstring, self_var);
    if (top)
        pairs_add(top, string(self_var), cd);
    aout++;
    bool preserve = false;

    for (int i = 0; i < md->arg_types_count; i++) {
        if (aout > 0)
            call(output, concat_cstring, ", ");
        
        for (int ii = 0; ii < md->at_token_count[i]; ii++) {
            Token *t = &(md->arg_types[i])[ii];
            if (call(t->str, cmp, "preserve") == 0) {
                preserve = true;
                continue;
            }
            if (t->skip) continue;
            String ts = t->str;
            if (!preserve && call(t->str, compare, cd_origin->class_name) == 0) {
                ts = cd->class_name;
            }
            ClassDec cd_2 = pairs_value(self->static_class_map, ts, ClassDec);

            if (forwards) {
                if (cd_2) {
                    sprintf(buf, "struct _%s *", cd_2->struct_object->buffer);
                    call(output, concat_cstring, buf);
                    continue;
                }
            }
            if (cd_2) {
                sprintf(buf, "%s ", cd_2->struct_object->buffer);
                call(output, concat_cstring, buf);
            } else {
                call(output, concat_string, ts);
                call(output, concat_cstring, " ");
            }
        }
        if (names) {
            Token *t_name = md->arg_names[i];
            call(self, token_out, t_name, ' ', output);
            if (top && md->at_token_count[i] == 1) {
                String s_type = md->arg_types[i]->str;
                ClassDec cd_type = pairs_value(self->static_class_map, s_type, ClassDec);
                if (cd_type) {
                    String s_var = t_name->str;
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

int sort_classes(ClassDec a, ClassDec b) {
    if (a == b)
        return 0;
    for (ClassDec p = b->parent; p; p = p->parent)
        if (p == a)
            return -1;
    for (ClassDec p = a->parent; p; p = p->parent)
        if (p == b)
            return 1;
    return 0;
}

void CX_declare_classes(CX self, FILE *file_output) {
    String output = new(String);
    char buf[1024];
    KeyValue kv;
    List classes = new(List);
    each_pair(self->classes, kv)
        list_push(classes, kv->value);
    call(classes, sort, true, (SortMethod)sort_classes);
    ClassDec cd;
    each(classes, cd) {
        String class_name = cd->class_name;
        bool is_class = call(class_name, cmp, "Class") == 0;
        Pairs m = new(Pairs);
        call(self, effective_methods, cd, &m);
        cd->effective = m;
        KeyValue mkv;
        {
            sprintf(buf,
                "\ntypedef struct _%s {\n"          \
                "\tstruct _%s_%sClass *cl;\n"       \
                "\tint refs;\n"                     \
                "\tstruct _%s_%sClass *parent;\n"      \
                "\tconst char *name;\n"             \
                "\tvoid(*_init)(struct _%s *);\n"   \
                "\tuint32_t flags;\n"                 \
                "\tuint32_t object_size;\n"           \
                "\tuint32_t member_count;\n"          \
                "\tchar *member_types;\n"           \
                "\tconst char **member_names;\n"    \
                "\tMethod *members;\n",
                    cd->struct_class->buffer,
                    self->name->buffer,
                    !is_class ? "" : "Base",
                    self->name->buffer,
                    !is_class ? (!cd->parent ? "" : cd->parent->class_name->buffer) : "",
                    cd->struct_object->buffer);
            call(output, concat_cstring, buf);

            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                ClassDec origin = md->cd;
                // need to find actual symbol name
                switch (md->member_type) {
                    case MT_Method:
                        // need a way to gather forwards referenced in method returns, arguments, and property types
                        sprintf(buf, "\t");
                        call(output, concat_cstring, buf);
                        for (int i = 0; i < md->type_count; i++) {
                            String ts = md->type[i].str;
                            ClassDec forward = pairs_value(self->static_class_map, ts, ClassDec);
                            // if this return type is st
                            if (forward == origin) {
                                // read through type to be sure it should be [forwardable?]
                                forward = cd;
                            }
                            if (forward) {
                                sprintf(buf, "struct _%s *", forward->struct_object->buffer);
                                call(output, concat_cstring, buf);
                            } else {
                                call(self, token_out, &md->type[i], ' ', output);
                            }
                        }
                        String args = call(self, args_out, NULL, cd, md, md->member_type == MT_Method && !md->is_static, false, 0, true);
                        sprintf(buf, " (*%s)(%s);\n",
                            md->str_name->buffer, args->buffer ? args->buffer : "");
                        call(output, concat_cstring, buf);
                        break;
                    case MT_Prop:
                        sprintf(buf, "\t");
                        call(output, concat_cstring, buf);
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            if (t->skip) continue;
                            String ts = t->str;
                            ClassDec forward = pairs_value(self->static_class_map, ts, ClassDec);
                            if (forward) {
                                sprintf(buf, "struct _%s *", forward->struct_object->buffer);
                                call(output, concat_cstring, buf);
                            } else
                                call(self, token_out, t, ' ', output);
                        }
                        if (md->is_static) {
                            sprintf(buf, " (*get_%s)(struct _%s *);\n", md->str_name->buffer,
                                cd->struct_class->buffer);
                            call(output, concat_cstring, buf);
                        } else {
                            sprintf(buf, " (*get_%s)(struct _%s *);\n", md->str_name->buffer,
                                cd->struct_object->buffer);
                            call(output, concat_cstring, buf);
                        }
                        sprintf(buf, "\t");
                        call(output, concat_cstring, buf);
                        if (md->is_static) {
                            sprintf(buf, "void (*set_%s)(struct _%s *, ", md->str_name->buffer,
                                cd->struct_class->buffer);
                            call(output, concat_cstring, buf);
                        } else {
                            sprintf(buf, "void (*set_%s)(struct _%s *, ", md->str_name->buffer,
                                cd->struct_object->buffer);
                            call(output, concat_cstring, buf);
                        }
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            if (t->skip) continue;
                            String ts = t->str;
                            ClassDec forward = pairs_value(self->static_class_map, ts, ClassDec);
                            if (forward) {
                                sprintf(buf, "struct _%s *", forward->struct_object->buffer);
                                call(output, concat_cstring, buf);
                            } else
                                call(self, token_out, t, ' ', output);
                        }
                        sprintf(buf, ");\n");
                        call(output, concat_cstring, buf);
                        break;
                }
            }
            sprintf(buf, "} *%s;\n\n", cd->struct_class->buffer);
            call(output, concat_cstring, buf);

            sprintf(buf, "EXPORT %s %s;\n\n",
                cd->struct_class->buffer,
                cd->class_var->buffer);
            call(output, concat_cstring, buf);
        }

        if (!is_class) {
            sprintf(buf, "\ntypedef struct _%s {\n", cd->struct_object->buffer);
            call(output, concat_cstring, buf);

            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                if (md->member_type == MT_Prop && !md->getter_start) {
                    sprintf(buf, "\t");
                    call(output, concat_cstring, buf);
                    if (call(md->str_name, cmp, "cl") == 0) {
                        sprintf(buf, "%s cl;\n", cd->struct_class->buffer);
                        call(output, concat_cstring, buf);
                    } else {
                        int is_self = -1;
                        for (int i = 0; i < md->type_count; i++) {
                            Token *t = &md->type[i];
                            if (t->skip) continue;
                            if (t->length == cd->class_name->length &&
                                    strncmp(t->value, cd->class_name->buffer, t->length) == 0) {
                                        sprintf(buf, "struct _%s *", cd->struct_object->buffer);
                                call(output, concat_cstring, buf);
                            } else {
                                String s_token = t->str;
                                ClassDec found = pairs_value(self->static_class_map, s_token, ClassDec);
                                if (found) {
                                    call(output, concat_string, found->struct_object);
                                    call(output, concat_char, ' ');
                                } else
                                    call(self, token_out, t, ' ', output);
                            }
                        }
                        sprintf(buf, " %s;\n", md->str_name->buffer);
                        call(output, concat_cstring, buf);
                    }
                }
            }
            sprintf(buf, "} *%s;\n\n", cd->struct_object->buffer);
            call(output, concat_cstring, buf);
        }
    }
    sprintf(buf, "EXPORT bool module_%s;\n\n",
        self->name->buffer);
    call(output, concat_cstring, buf);
    fprintf(file_output, "\n");
    each(classes, cd) {
        fprintf(file_output, "struct _%s;\n", cd->struct_class->buffer);
        fprintf(file_output, "struct _%s;\n", cd->struct_object->buffer);
    }
    fprintf(file_output, "%s", output->buffer);
}

void CX_define_module_constructor(CX self, FILE *file_output) {
    String output = new(String);
    char buf[1024];

    sprintf(buf, "\nbool module_%s;\n",
        self->name->buffer);
    call(output, concat_cstring, buf);

    KeyValue kv;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        const char *class_name = cd->class_name->buffer;
        sprintf(buf, "%s %s;\n",
            cd->struct_class->buffer,
            cd->class_var->buffer);
        call(output, concat_cstring, buf);
    }
    sprintf(buf, "\n");
    call(output, concat_cstring, buf);

    sprintf(buf, "static bool module_retry() {\n");
    call(output, concat_cstring, buf);

    // list all of the modules needed; then simply create a statement to check for them
    if (list_count(self->modules) > 0) {
        sprintf(buf, "\tif (");
        call(output, concat_cstring, buf);
        bool first = true;
        CX m;
        each(self->modules, m) {
            if (!first) {
                sprintf(buf, " || ");
                call(output, concat_cstring, buf);
            }
            sprintf(buf, "!module_%s", m->name->buffer);
            call(output, concat_cstring, buf);
            first = false;
        }
        sprintf(buf, ")\n");
        call(output, concat_cstring, buf);
        sprintf(buf, "\t\treturn false;\n\n");
        call(output, concat_cstring, buf);
    }
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        const char *class_name = cd->class_name->buffer;
        const char *struct_class = cd->struct_class->buffer;
        const char *struct_object = cd->struct_object->buffer;
        const char *class_var = cd->class_var->buffer;
        sprintf(buf, "\t%s = (%s)alloc_bytes(sizeof(*%s));\n",
            class_var, struct_class, class_var);
        call(output, concat_cstring, buf);
        sprintf(buf, "\tmemset(%s, 0, sizeof(*%s));\n",
            class_var, class_var);
        call(output, concat_cstring, buf);
    }

    sprintf(buf, "\n");
    call(output, concat_cstring, buf);

    // set values to statics declared prior
    each_pair(self->static_class_vars, kv) {
        ClassDec cd = (ClassDec)kv->key;
        String alias = (String)kv->value;
        if (alias->length > 0) {
            sprintf(buf, "\t%s.%s = %s;\n",
                alias->buffer,
                cd->class_name->buffer,
                cd->class_var->buffer);
            call(output, concat_cstring, buf);
        } else {
            sprintf(buf, "\t%s = %s;\n",
                cd->class_name->buffer,
                cd->class_var->buffer);
            call(output, concat_cstring, buf);
        }
    }

    sprintf(buf, "\n");
    call(output, concat_cstring, buf);

    // set class members
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        int method_count = 0;
        KeyValue mkv;
        const char *class_name = cd->class_name->buffer;
        const char *struct_class = cd->struct_class->buffer;
        const char *struct_object = cd->struct_object->buffer;
        const char *class_var = cd->class_var->buffer;

        sprintf(buf, "\t%s->name = \"%s\";\n", class_name, class_name);
        call(output, concat_cstring, buf);
        sprintf(buf, "\t%s->cl = (typeof(%s->cl))%s;\n", class_name, class_name,
            strcmp(class_name, "Class") == 0 ? "base_Base_var" : "base_Class_var");
        call(output, concat_cstring, buf);
        if (cd->parent) {
            const char *parent_name = cd->parent->class_name->buffer;
            sprintf(buf, "\t%s->parent = (typeof(%s->parent))%s;\n",
                class_name, class_name, cd->parent->class_var->buffer);
            call(output, concat_cstring, buf);
        }
        sprintf(buf, "\t%s->object_size = sizeof(struct _%s);\n", class_name, struct_object);
        call(output, concat_cstring, buf);
        sprintf(buf, "\t%s->_init = %s__init;\n", class_name, class_name);
        call(output, concat_cstring, buf);

        each_pair(cd->effective, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            ClassDec origin = md->cd;
            char ptr[256];
            if (!md->is_private)
                switch (md->member_type) {
                    case MT_Method:
                        if (md->cd == cd)
                            sprintf(ptr, "%s_%s", origin->class_name->buffer, md->str_name->buffer);
                        else
                            sprintf(ptr, "%s->%s", origin->class_name->buffer, md->str_name->buffer);
                        sprintf(buf, "\t%s->%s = (typeof(%s->%s))%s;\n",
                            class_name, md->str_name->buffer,
                            class_name, md->str_name->buffer,
                            ptr);
                        call(output, concat_cstring, buf);
                        method_count++;
                        break;
                    case MT_Prop:
                        if (md->cd == cd)
                            sprintf(ptr, "%s_get_%s", origin->class_name->buffer, md->str_name->buffer);
                        else
                            sprintf(ptr, "%s->get_%s", origin->class_name->buffer, md->str_name->buffer);
                        sprintf(buf, "\t%s->get_%s = (typeof(%s->get_%s))%s;\n",
                            class_name, md->str_name->buffer,
                            class_name, md->str_name->buffer,
                            ptr);
                        call(output, concat_cstring, buf);
                        if (md->cd == cd)
                            sprintf(ptr, "%s_set_%s", origin->class_name->buffer, md->str_name->buffer);
                        else
                            sprintf(ptr, "%s->set_%s", origin->class_name->buffer, md->str_name->buffer);
                        sprintf(buf, "\t%s->set_%s = (typeof(%s->set_%s))%s;\n",
                            class_name, md->str_name->buffer,
                            class_name, md->str_name->buffer,
                            ptr);
                        call(output, concat_cstring, buf);
                        method_count += 2;
                        break;
                }
        }
        // member types
        sprintf(buf, "\t%s->member_types = (char *)malloc(%d);\n",
            class_name, method_count);
        call(output, concat_cstring, buf);

        int mc = 0;
        each_pair(cd->effective, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            if (!md->is_private)
                switch (md->member_type) {
                    case MT_Method:
                        sprintf(buf, "\t%s->member_types[%d] = %d;\n",
                            class_name, mc, md->member_type); mc++;
                        call(output, concat_cstring, buf);
                        break;
                    case MT_Prop:
                        sprintf(buf, "\t%s->member_types[%d] = %d;\n",
                            class_name, mc, md->member_type); mc++;
                        call(output, concat_cstring, buf);

                        sprintf(buf, "\t%s->member_types[%d] = %d;\n",
                            class_name, mc, md->member_type); mc++;
                        call(output, concat_cstring, buf);
                        break;
                }
        }
        // member names
        sprintf(buf, "\t%s->member_names = (const char **)malloc(%d * sizeof(const char *));\n",
            class_name, method_count);
        call(output, concat_cstring, buf);

        mc = 0;
        each_pair(cd->effective, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            if (!md->is_private)
                switch (md->member_type) {
                    case MT_Method:
                        sprintf(buf, "\t%s->member_names[%d] = \"%s\";\n",
                            class_name, mc, md->str_name->buffer); mc++;
                        call(output, concat_cstring, buf);
                        break;
                    case MT_Prop:
                        sprintf(buf, "\t%s->member_names[%d] = \"%s\";\n",
                            class_name, mc, md->str_name->buffer); mc++;
                        call(output, concat_cstring, buf);

                        sprintf(buf, "\t%s->member_names[%d] = \"%s\";\n",
                            class_name, mc, md->str_name->buffer); mc++;
                        call(output, concat_cstring, buf);
                        break;
                }
        }

        sprintf(buf, "\t%s->member_count = %d;\n", class_name, method_count);
        call(output, concat_cstring, buf);
    }
    sprintf(buf, "\tmodule_%s = true;\n", self->name->buffer);
    call(output, concat_cstring, buf);
    sprintf(buf, "\treturn true;\n");
    call(output, concat_cstring, buf);
    sprintf(buf, "}\n\n");
    call(output, concat_cstring, buf);

    sprintf(buf, "global_construct(module_constructor) {\n");
    call(output, concat_cstring, buf);
    sprintf(buf, "\tbool success = module_retry();\n");
    call(output, concat_cstring, buf);
    sprintf(buf, "\tmodule_loader_continue(success ? null : module_retry);\n");
    call(output, concat_cstring, buf);
    sprintf(buf, "}\n");
    call(output, concat_cstring, buf);

    fprintf(file_output, "%s", output->buffer);
}

String CX_super_out(CX self, List scope, ClassDec cd, Token *t_start, Token *t_end) {
    Pairs top = (Pairs)call(scope, last);
    if (!cd->parent || !top)
        return NULL;
    for (Token *t = t_start; ; t++) {
        if (t->skip) continue;
        if (t->type == TT_Identifier && t->length == 5 && strncmp(t->value, "super", 5) == 0) {
            pairs_add(top, string("super"), cd->parent);
            char buf[1024];
            const char *super = cd->parent->struct_object->buffer;
            sprintf(buf, "%s super = (%s)self; ", super, super);
            return string(buf);
        }
        if (t == t_end)
            break;
    }
    return NULL;
}

// verify the code executes as planned
bool CX_emit_implementation(CX self, FILE *file_output) {
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
            cd->class_name->buffer, cd->struct_object->buffer);
        call(output, concat_cstring, buf);
        const char *class_name = cd->class_name->buffer;
        const char *class_type = cd->struct_object->buffer;
        const char *class_var = cd->class_var->buffer;

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
                    String type_last = NULL;
                    String code = call(self, code_out, scope, md->assign, &md->assign[md->assign_count - 1], NULL, NULL, false, &type_last);
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
            ClassDec super_mode = !md->is_static ? cd : NULL;

            // resolve types
            String type_str = new(String);
            if (md->type_count > 0) {
                bool first = true;
                for (int i = 0; i < md->type_count; i++) {
                    Token *tt = &md->type[i];
                    if (tt->skip)
                        continue;
                    if (!first)
                        call(type_str, concat_char, ' ');
                    ClassDec cd_found = pairs_value(self->static_class_map, tt->str, ClassDec);
                    if (cd_found) {
                        call(type_str, concat_string, cd_found->struct_object);
                        md->type_cd = cd_found;
                    } else
                        call(type_str, concat_string, tt->str);
                    first = false;
                }
                md->type_str = type_str;
            }

            if (!md->block_start) {
                // for normal var declarations, output stub for getter / setter
                if (md->member_type == MT_Prop && !md->is_private) {
                    const char *tn = type_str->buffer;
                    const char *cn = class_name;
                    const char *vn = md->str_name->buffer;
                    ClassDec cd_tn = pairs_value(self->static_class_map, string(tn), ClassDec);
                    if (cd_tn)
                        tn = cd_tn->struct_object->buffer;

                    // todo: for types [only], modify Token->value to remap types to their actual C names
                    if (md->is_static) {
                        sprintf(buf, "%s %s_get_%s() { return (%s)%s->%s; }\n",
                            tn, cn, vn, tn, class_var, vn);
                        call(output, concat_cstring, buf);

                        sprintf(buf, "%s %s_set_%s(%s self, %s value) { return (%s)(%s->%s = (typeof(self->%s))value); }\n",
                            tn, cn, vn, class_type, vn, tn, class_var, vn, vn);
                        call(output, concat_cstring, buf);
                    } else {
                        sprintf(buf, "%s %s_get_%s(%s self) { return (%s)self->%s; }\n",
                            tn, cn, vn, class_type, tn, vn);
                        call(output, concat_cstring, buf);
                        
                        sprintf(buf, "%s %s_set_%s(%s self, %s value) { return (%s)(self->%s = (typeof(self->%s))value); }\n",
                            tn, cn, vn, class_type, tn, tn, vn, vn);
                        call(output, concat_cstring, buf);
                    }
                }
                continue;
            }
            Pairs top = new(Pairs);
            list_push(scope, top);
            switch (md->member_type) {
                case MT_Method: {
                    call(output, concat_string, type_str);
                    sprintf(buf, " %s_%s(", class_name, md->str_name->buffer);
                    call(output, concat_cstring, buf);

                    String code = call(self, args_out, top, cd, md, !md->is_static, true, 0, false);
                    call(output, concat_string, code);

                    sprintf(buf, ") ");
                    call(output, concat_cstring, buf);
                    String type_last = NULL;
                    code = call(self, code_out, scope, md->block_start, md->block_end, NULL, super_mode, true, &type_last);
                    call(output, concat_string, code);

                    sprintf(buf, "\n");
                    call(output, concat_cstring, buf);

                    if (md->is_static && call(md->str_name, cmp, "main") == 0) {
                        if (md->type_count != 1) {
                            fprintf(stderr, "main must return void or int\n");
                            exit(1);
                        }
                        String s_ret = md->type[0].str;
                        bool is_int = call(s_ret, cmp, "int") == 0;
                        if (call(s_ret, cmp, "void") != 0 && !is_int) {
                            fprintf(stderr, "main must return void or int\n");
                            exit(1);
                        }
                        sprintf(buf, "%s main(int argc, char *argv[]) {\n", s_ret->buffer);
                        call(output, concat_cstring, buf);
                        sprintf(buf, "\tbase_Array args = (base_Array)Base->new_object(Base, (base_Class)Array, 0, false);\n");
                        call(output, concat_cstring, buf);
                        sprintf(buf, "\tfor (int i = 0; i < argc; i++) {\n");
                        call(output, concat_cstring, buf);
                        sprintf(buf, "\t\tbase_String str = String->from_cstring(String, argv[i]);\n");
                        call(output, concat_cstring, buf);
                        sprintf(buf, "\t\tArray->push((base_Array)args, (base_Base)str);\n");
                        call(output, concat_cstring, buf);
                        sprintf(buf, "\t}\n");
                        call(output, concat_cstring, buf);
                        if (is_int) {
                            sprintf(buf, "\treturn %s_%s(%s, args);",
                                class_name,
                                md->str_name->buffer,
                                class_name);
                            call(output, concat_cstring, buf);
                        }
                        sprintf(buf, "\n}\n");
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
                            sprintf(buf, "%s_get_", class_name);
                            call(output, concat_cstring, buf);
                            call(output, concat_string, md->str_name);
                            call(output, concat_char, '(');
                        } else {
                            for (int i = 0; i < md->type_count; i++)
                                call(self, token_out, &md->type[i], ' ', output);
                            sprintf(buf, "%s_set_", class_name);
                            call(output, concat_cstring, buf);
                            call(output, concat_string, md->str_name);
                            call(output, concat_char, '(');
                        }
                        if (md->is_static) {
                            sprintf(buf, "%s class", cd->struct_class->buffer);
                            pairs_add(top, string("class"), cd);
                        } else {
                            sprintf(buf, "%s self", cd->struct_object->buffer);
                            pairs_add(top, string("self"), cd);
                        }
                        call(output, concat_cstring, buf);
                        if (i == 1) {
                            sprintf(buf, ", ");
                            call(output, concat_cstring, buf);
                        }
                        if (i == 1) {
                            for (int i = 0; i < md->type_count; i++)
                                call(self, token_out, &md->type[i], ' ', output);
                            call(self, token_out, md->setter_var, ' ', output);
                            String s_type = md->setter_var->str;
                            ClassDec cd_type = pairs_value(self->static_class_map, s_type, ClassDec);
                            if (cd_type) {
                                String s_var = md->setter_var->str;
                                pairs_add(top, s_var, cd_type);
                            }
                        }
                        sprintf(buf, ") {\n");
                        call(output, concat_cstring, buf);
                        if (block_start->punct == "{")
                            block_start++;
                        if (block_end->punct == "}")
                            block_end--;
                        sprintf(buf, "# %d \"%s\"\n", block_start->line, block_start->file->buffer);
                        call(output, concat_cstring, buf);
                        String type_last = NULL;
                        if (i == 0) {
                            String code = call(self, code_out, scope, block_start, block_end, NULL, super_mode, true, &type_last);
                            call(output, concat_string, code);
                        } else {
                            String code = call(self, code_out, scope, block_start, block_end, NULL, super_mode, true, &type_last);
                            if (md->is_static)
                                sprintf(buf, "return %s->get_%s();", class_name, md->str_name->buffer);
                            else
                                sprintf(buf, "return self->cl->get_%s(self);", md->str_name->buffer);
                            call(output, concat_string, code);
                            call(output, concat_cstring, buf);
                        }
                        sprintf(buf, "\n}\n");
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
        if (self)
            list_push(self->modules, m); // should be a pair, where key is what the module is referred to as
        if (!modules)
            modules = new(List);
        list_push(modules, m);
        call(m, process, name);
        release(m);
    } else if (self && call(self->modules, index_of, base(m)) == -1) {
        list_push(self->modules, m);
    }
    return m;
}

bool CX_read_modules(CX self) {
    if (call(self->name, cmp, "base") != 0) {
        load_module(self, "base");
    }
    for (Token *t = self->tokens; t->value; t++) {
        if (t->skip) continue;
        bool is_include = t->length == 7 && strncmp(t->value, "include", t->length) == 0;
        bool is_module = t->length == 6 && strncmp(t->value, "module", t->length) == 0;
        if (is_include || is_module) {
            bool is_private = false;
            if (is_include) {
                if (t != self->tokens) {
                    Token *tt = t - 1;
                    is_private = strncmp(tt->value, "private", tt->length) == 0;
                }
            }
            int count = call(self, read_expression, t, NULL, NULL, ";", 0, false);
            bool expect_comma = false;
            for (int i = 1; i < count; i++) {
                Token *tt = &t[i];
                if (expect_comma) {
                    if (tt->punct != ",") {
                        fprintf(stderr, "expected separator ',' between %s", (is_include ? "includes" : "modules"));
                        exit(1);
                    }
                } else {
                    // expect module name (TT_Identifier)
                    if (tt->type != TT_Identifier || tt->length >= 256) {
                        fprintf(stderr, "expected identifier for %s name", (is_include ? "include" : "module"));
                        exit(1);
                    }
                    char name[256];
                    memcpy(name, tt->value, tt->length);
                    name[tt->length] = 0;
                    if (is_include) {
                        if (is_private)
                            list_push(self->private_includes, string(name));
                        else
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
    self->classes = new(Pairs);
    self->aliases = new(Pairs); // key = module, value = alias that this module is referred to as; each alias will create a static struct with all of the classes of that module contained inside
    self->includes = new(List);
    self->private_includes = new(List);
    self->forward_structs = new(List);
}

void emit_module_statics(CX self, CX module, FILE *file_output, Pairs emitted, Pairs vars, Pairs class_map, String alias, bool collect_only) {
    ClassDec cd;
    KeyValue kv;
    each_pair(module->classes, kv) {
        cd = (ClassDec)kv->value;
        if (!pairs_value(emitted, cd->class_var, ClassDec)) {
            if (file_output) {
                fprintf(file_output, "static %s%s %s;\n",
                    alias ? "\t" : "",
                    cd->struct_class->buffer,
                    cd->class_name->buffer);
            }
            pairs_add(emitted, cd->class_var, cd);
            pairs_add(vars, cd, (alias ? alias : string("")));
            if (!alias || alias->length == 0)
                pairs_add(class_map, cd->class_name, cd);
            else {
                String whole = new(String);
                call(whole, concat_string, alias);
                call(whole, concat_char, '.');
                call(whole, concat_string, cd->class_name);
                pairs_add(class_map, whole, cd);
            }
        }
    }
    CX m;
    each(module->modules, m) {
        emit_module_statics(self, m, file_output, emitted, vars, class_map, alias, collect_only);
    }
}

bool CX_emit_module_statics(CX self, FILE *file_output, bool collect_only) {
    CX m;
    bool first = true;
    Pairs emitted = new(Pairs);
    Pairs vars = new(Pairs);
    Pairs class_map = new(Pairs);

    emit_module_statics(self, self, file_output, emitted, vars, class_map, string(""), collect_only);
    
    each(self->modules, m) {
        if (!first && file_output)
            fprintf(file_output, "\n");
        String alias = pairs_value(m->aliases, self->name, String);
        if (alias) {
            fprintf(file_output, "typedef struct _alias_%s {\n", alias->buffer);
            emit_module_statics(self, m, file_output, emitted, vars, class_map, alias, collect_only);
            fprintf(file_output, "} alias_%s;\n", alias->buffer);
            fprintf(file_output, "static alias_%s %s;\n",
                alias->buffer, alias->buffer);
        } else {
            emit_module_statics(self, m, file_output, emitted, vars, class_map, string(""), collect_only);
        }
        first = false;
    }
    if (file_output)
        fprintf(file_output, "\n");
    self->using_classes = emitted;
    self->static_class_vars = vars;
    self->static_class_map = class_map;
    return true;
}

bool CX_process(CX self, const char *location) {
    // read in module location, not a file
    // open all files in directory
    self->name = retain(class_call(String, from_cstring, location));
    DIR *dir = opendir(location);
    if (!dir) {
        fprintf(stderr, "cannot open module '%s'\n", location);
        exit(1);
    }
    struct dirent *ent;
    List module_contents = NULL;
    List module_files = NULL;
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
        String file_name = string(file);
        String contents = class_call(String, from_file, file_name->buffer);//file->buffer);
        if (!module_contents) {
            module_contents = new(List);
            module_files = new(List);
        }
        list_push(module_contents, contents);
        list_push(module_files, file_name);
    }
    closedir(dir);
    int n_tokens = 0;
    self->tokens = call(self, read_tokens, module_contents, module_files, &n_tokens);

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
    call(self, emit_module_statics, NULL, false);
    call(self, declare_classes, module_header);
    fprintf(module_header, "#endif\n");
    fclose(module_header);

    // c code output
    FILE *module_code = module_file(location, "module.c", "w+");
    fprintf(module_code, "#include \"module.h\"\n");
    each(self->private_includes, include) {
        fprintf(module_code, "#include \"%s.h\"\n", include->buffer);
    }
    fprintf(module_code, "\n");
    call(self, emit_module_statics, module_code, true);
    call(self, merge_class_tokens, self->tokens, &n_tokens);
    call(self, emit_implementation, module_code);
    call(self, define_module_constructor, module_code);
    fclose(module_code);
    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("silver modules generator -- version %s\n", CX_VERSION);
        printf("usage: silver-mod module.json\n");
        exit(1);
    }
    const char *input = (const char *)argv[1];
    load_module(NULL, input);
    return 0;
}
