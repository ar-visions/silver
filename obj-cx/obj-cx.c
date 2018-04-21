#include <obj/obj.h>
#include <obj-cx/obj-cx.h>
#include <stdio.h>
#include <dirent.h>

#define CX_VERSION  "0.4.0"

implement(CX)
implement(ClassDec)
implement(MemberDec)
implement(ClosureDec)
implement(ClosureClass)
implement(ArrayClass)

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

void token_out(Token *t, int sep, String output) {
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

MemberDec ClassDec_member_lookup(ClassDec self, String name, ClassDec *type) {
    *type = NULL;
    for (ClassDec cd = self; cd; cd = cd->parent) {
        MemberDec md = pairs_value(cd->members, name, MemberDec);
        if (md) {
            if (md->type_cd == cd) {
                if (md->is_preserve) {
                    *type = md->type_cd;
                } else {
                    *type = self;
                }
            } else {
                *type = md->type_cd;
            }
            return md;
        }
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

void MemberDec_read_args(MemberDec self, Token *t, int n_tokens, bool names) {
    MemberDec md = self;
    md->args = t;
    md->arg_names = alloc_bytes(sizeof(Token *) * n_tokens);
    md->arg_types = alloc_bytes(sizeof(Token *) * n_tokens);
    md->arg_preserve = alloc_bytes(sizeof(bool) * n_tokens);
    md->at_token_count = alloc_bytes(sizeof(int) * n_tokens);
    Token *t_start = md->args;
    Token *t_cur = md->args;
    int paren_depth = 0;
    int type_tokens = 0;
    md->str_args = new(String);
    for (int i = 0; i < n_tokens; i++) {
        token_out(&t[i], 0, md->str_args);
    }
    while (paren_depth >= 0) {
        if (t_cur->punct == "," || (paren_depth == 0 && t_cur->punct == ")")) {
            int tc = md->arg_types_count;
            bool is_preserve = (t_start && t_start->keyword == "preserve");
            md->arg_names[tc] = t_cur - 1;
            md->arg_types[tc] = is_preserve ? t_start + 1 : t_start;
            md->arg_preserve[tc] = is_preserve;
            md->at_token_count[tc] = type_tokens - 1 - (is_preserve ? 1 : 0);
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
        "<:", ":>", "<%", "%>", "%:", "%:%:", "^{"
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
        "get","set","construct","weak","preserve","uint","ulong","uchar","ushort"
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
        0,0,1,0,0,0,0,0,1,1,1,1
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
        String current_file = module_files ? (String)call(module_files, object_at, content_index++) : NULL;
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
                    nt++;
                    t = NULL;
                }
                if (!t) {
                    t = (Token *)&tokens[nt];
                    t->file = current_file ? retain(current_file) : NULL;
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

int CX_read_block(CX self, Token *t, Token **start, Token **end) {
    int c = 0, brace_depth = 1;
    *start = t;
    while (token_next(&t)->value) {
        c++;
        if (t->punct == "{") {
            brace_depth++;
        } else if (t->punct == "}") {
            if (--brace_depth == 0) {
                *end = t;
                return c;
            }
        }
    }
    printf("expected '}' to end block\n");
    exit(1);
    return 0;
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
                if (mark_block && (t->punct == "{" || (token_next(&t))->punct == "{"))
                    call(self, read_block, t, b_start, b_end);
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
                if (t->keyword == "static") {
                    md->is_static = true;
                    token_next(&t);
                }
                if (t->keyword == "private") {
                    md->is_private = true;
                    token_next(&t);
                }
                if (t->keyword == "preserve") {
                    md->is_preserve = true;
                    token_next(&t);
                }
                if (t->keyword == "weak") {
                    if (md->type != MT_Prop) {
                        fprintf(stderr, "weak keyword is only allowed on vars\n");
                        exit(1);
                    }
                    md->is_weak = true;
                    token_next(&t);
                }
                Token *block_start = NULL, *block_end = NULL;
                int token_count = call(self, read_expression, t, &block_start, &block_end, "{;),", 0, false);
                if (token_count == 0) {
                    if (md->is_static || md->is_private) {
                        fprintf(stderr, "expected expression\n");
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
                        call(md, read_args, md->args, n_args, true);
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
    if (expected && expected == given)
        return expected->struct_object;
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

String CX_start_tracking(CX self, List scope, String var, bool check_only) {
    Pairs top = (Pairs)call(scope, last);
    Pairs tracking = (Pairs)top->user_data;
    if (!tracking) {
        tracking = (Pairs)(top->user_data = (Base)new(Pairs));
    }
    pairs_add(tracking, var, string(check_only ? "true" : "false"));
    return NULL;
}

bool CX_is_tracking(CX self, Pairs top, String var, bool *check_only) {
    Pairs tracking = (Pairs)top->user_data;
    if (tracking) {
        String b = pairs_value(tracking, var, String);
        if (b) {
            *check_only = call(b, cmp, "true") == 0;
            return true;
        }
    }
    return false;
}

String CX_gen_var(CX self, List scope, ClassDec cd, bool hidden) {
    Pairs top = (Pairs)call(scope, last);
    char buf[64];
    sprintf(buf, "_gen_%d", ++self->gen_vars);
    if (self->gen_vars == 3) {
        int test = 0;
        test++;
    }
    String ret = string(buf);
    pairs_add(top, ret, cd);
    if (!hidden)
        call(self, start_tracking, scope, ret, true);
    return ret;
}

String CX_var_gen_out(CX self, List scope, ClassDec cd, String code) {
    char buf[1024];
    String output = new(String);
    String gen_var = call(self, gen_var, scope, cd, false);
    sprintf(buf, "(%s = ", gen_var->buffer);
    call(output, concat_cstring, buf);
    call(output, concat_string, code);
    call(output, concat_char, ')');
    return output;
}

String CX_var_op_out(CX self, List scope, Token *t,
        ClassDec cd, String target, bool is_instance, Token **t_after, String *type_last,
        int *p_flags, MemberDec method, int *brace_depth, bool assign, bool closure_scoped) {
    int flags = 0;
    String code = call(self, class_op_out, scope, t, cd, target, is_instance, t_after,
                        type_last, method, brace_depth, &flags, false, closure_scoped);
    if (!assign && *type_last && (flags & CODE_FLAG_ALLOC) != 0) {
        ClassDec cd_returned = pairs_value(self->static_class_map, (*type_last), ClassDec);
        if (cd_returned) {
            String gen_out = call(self, var_gen_out, scope, cd_returned, code);
            release(code);
            return gen_out;
        }
    } else if (assign) {
        *p_flags |= flags;
    }
    return code;
}
                    

String CX_class_op_out(CX self, List scope, Token *t,
        ClassDec cd, String target, bool is_instance, Token **t_after, String *type_last,
        MemberDec method, int *brace_depth, int *flags, bool assign, bool closure_scoped) {
    String output = new(String);
    char buf[1024];
    Token *t_start = t;
    String s_token = t->str;
    Token *t_member = t;
    MemberDec md = NULL;
    ClassDec cd_last = NULL;
    
    if (call(target, cmp, "class") == 0)
        cd = CX_find_class(string("Class"));
    ClassDec md_type = NULL;
    if (cd && (t - 1)->punct == ".") {
        md = call(cd, member_lookup, s_token, &md_type);
    }
    if (md) {
        cd_last = md_type;
        *type_last = md_type ? md_type->class_name : NULL;
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
            *flags |= CODE_FLAG_ALLOC;
            String ctor = new(String);
            sprintf(buf, "(%s)Base->new_object(Base, (base_Class)%s, 0)",
                cd->struct_object->buffer, class_var);
            call(ctor, concat_cstring, buf);
            token_next(&t);
            *type_last = cd->class_name;
            String gen_var = NULL;
            String last_set = NULL;
            for (;;) {
                int n = call(self, read_expression, t, NULL, NULL, ",)", 0, true);
                if (n == 0)
                    break;
                Token *var = t;
                Token *assign = t + 1;
                Token *value_start = t + 2;
                if (n < 3 || assign->punct != "=" || var->type != TT_Identifier) {
                    fprintf(stderr, "Expected constructor inline setter syntax: prop=val\n");
                    exit(1);
                }
                int value_code_flags = 0;
                String value_type_returned = NULL;
                String value_code = call(self, code_out, scope, value_start, &t[n - 1], t_after, NULL, false,
                    &value_type_returned, method, brace_depth, &value_code_flags, false);
                ClassDec cd_returned = pairs_value(self->static_class_map, value_type_returned, ClassDec);
                ClassDec setter_cd_type;
                MemberDec setter_lookup = call(cd, member_lookup, var->str, &setter_cd_type);
                String generic_type;
                if (!setter_lookup) {
                    fprintf(stderr, "inline setter: property %s::%s not found\n", cd->class_name->buffer, var->str->buffer);
                    exit(1);
                }
                if (setter_cd_type) {
                    generic_type = string("object");
                } else {
                    generic_type = setter_lookup->type_str;
                }
                if (cd_returned) {
                    ClassDec cd_expected = setter_lookup->type_cd;
                    String cast = call(self, inheritance_cast, cd_expected, cd_returned);
                    if (!cast) {
                        fprintf(stderr, "inline setter: incompatible object (%s) given to property %s:%s (%s)\n",
                            cd_returned->class_name->buffer,
                            cd->class_name->buffer, var->str->buffer,
                            cd_expected->class_name->buffer);
                        exit(1);
                    }
                }
                String setter_call = new(String);
                sprintf(buf, "(%s)set_", cd->struct_object->buffer);
                call(setter_call, concat_cstring, buf);
                call(setter_call, concat_string, generic_type);
                call(setter_call, concat_cstring, "((base_Base)(");
                call(setter_call, concat_string, ctor);
                call(setter_call, concat_cstring, "), ");
                sprintf(buf, "(Setter_%s)%s->set_%s, ", generic_type->buffer, cd->class_name->buffer, var->str->buffer);
                call(setter_call, concat_cstring, buf);
                call(setter_call, concat_string, value_code);
                call(setter_call, concat_cstring, ")");
                release(ctor);
                ctor = setter_call;

                t += n;
                if (t->punct == ")")
                    break;
                else
                    t++;
            }
            call(output, concat_string, ctor);
        } else {
            if (md_type)
                *flags |= CODE_FLAG_ALLOC;
            sprintf(buf, "%s->", class_var);
            call(output, concat_cstring, buf);
            token_out(t_member, '(', output);

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
                // code_out needs to be aware that this is being received into a var (generated var)
                int arg_code_flags = 0;
                String code = call(self, code_out, scope, t, &t[n - 1], t_after, NULL, false,
                    &type_returned, method, brace_depth, &arg_code_flags, false);
                ClassDec cd_returned = pairs_value(self->static_class_map, type_returned, ClassDec);
                call(arg_output, concat_string, code);
                if (type_returned && c_arg < md->arg_types_count) {
                    ClassDec cd_expected = NULL;
                    for (int ii = 0; ii < md->at_token_count[c_arg]; ii++) {
                        Token *t = &(md->arg_types[c_arg])[ii];
                        if (t->cd) {
                            cd_expected = t->cd;
                            break;
                        }
                    }
                    if (cd_returned && cd_expected) {
                        String cast = call(self, inheritance_cast, cd_expected, cd_returned);
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
        if (t->punct != ")") {
            fprintf(stderr, "%s:%d: expected ')'\n",
                t->file->buffer, t->line);
            exit(1);
        }
        if (*t_after < t)
            *t_after = t;
        if (!constructor)
            call(self, token_out, t, ' ', output);

    } else if (constructor) {
        fprintf(stderr, "expected '(' for constructor\n");
        exit(1);
    } else {
        if (t->assign) {
            // check if var is tracked
            Pairs sc = NULL;
            ClassDec cd_lookup = call(self, scope_lookup, scope, target, &sc, NULL, NULL);
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
            bool setter_method = false;
            String type_returned = NULL;
            // code_out needs to be aware that this is being received into a var (true)
            int assign_code_flags = 0;
            String code = call(self, code_out, scope, t, &t[n - 1], t_after, NULL, false, &type_returned,
                method, brace_depth, &assign_code_flags, true);
            t = *t_after;
            if (md && md->setter_start) {
                setter_method = true;
                // call explicit setter [todo: handle the various assigner operators]
                sprintf(buf, "%s->set_", class_var);
                call(set_str, concat_cstring, buf);

                call(self, token_out, t_member, '(', set_str);
                if (is_instance) {
                    if (closure_scoped)
                        call(set_str, concat_cstring, "__scope->");
                    sprintf(buf, "%s, ", target->buffer);
                    call(set_str, concat_cstring, buf);
                }
            } else {
                if (md) {
                    // set object var
                    if (is_instance) {
                        if (closure_scoped)
                            call(set_str, concat_cstring, "__scope->");
                        call(set_str, concat_string, target);
                        sprintf(buf, "->");
                        call(set_str, concat_cstring, buf);
                    } else {
                        sprintf(buf, "%s->", class_var);
                        call(set_str, concat_cstring, buf);
                    }
                    call(self, token_out, t_member, 0, set_str);
                } else {
                    if (cd_lookup)
                        expected_type = cd_lookup->class_name;
                    if (closure_scoped)
                        call(set_str, concat_cstring, "__scope->");
                    call(self, token_out, t_member, 0, set_str);
                }
            }
            String cast = NULL;
            bool check_only = false;
            bool tracked = !md && call(self, is_tracking, sc, target, &check_only);
            bool possible_alloc = ((assign_code_flags & CODE_FLAG_ALLOC) != 0) && (!md || !md->is_weak);
            if (type_returned) {
                ClassDec cd_returned = pairs_value(self->static_class_map, type_returned, ClassDec);
                ClassDec cd_expected = pairs_value(self->static_class_map, expected_type, ClassDec);
                cast = call(self, inheritance_cast, cd_expected, cd_returned);
                if (t_assign->punct != "=") {
                    fprintf(stderr, "Invalid assignment on object: %s\n", t_assign->punct);
                    exit(1);
                }
                // start tracking if not
                if (setter_method) {
                    if (cast) {
                        sprintf(buf, "%s (%s)%s)", set_str->buffer, cast->buffer, code->buffer);
                    } else {
                        sprintf(buf, "%s %s)", set_str->buffer, code->buffer);
                    }
                    call(output, concat_cstring, buf);
                } else if (!tracked && possible_alloc) {
                    call(self, start_tracking, scope, target, false);

                    // proof: [logic has been augmented to not start tracking when setting to member on object]
                    // obj.member (1)
                    // Base b = obj.member; (2)
                    // Base ret = b; (2)
                    // release(b); (1)
                    // return ret; (1)
                    //
                    // Base b = caller(); // (2) retained
                    // 
                    // caller().check_release() // (1) still exists as a member
                    //
                    //
                    // Base b = new Base(); // (0 on new, retained and becomes 1)
                    // Base ret = b; // (1)
                    // release(b) // (0) (b == ret, decrement, but defer due to equality with return value)
                    //
                    // Base b = caller(); // (1)
                    // caller().check_release() // (0) (release)
                    // the receiver doesnt decrement if its not set; it merely checks if it should be released based on its value
                    // ANY set is incrementing the ref count; non-sets just check the ref count

                    if (cast)
                        sprintf(buf, "%s = (%s)(%s->retain(%s))", set_str->buffer, cast->buffer,
                            cd_returned->class_name->buffer, code->buffer);
                    else {
                        // let compiler warn or error
                        sprintf(buf, "%s = (%s->retain(%s))", set_str->buffer,
                            cd_returned->class_name->buffer, code->buffer);
                    }
                    call(output, concat_cstring, buf);
                } else if (!tracked) {
                    if (cast)
                        sprintf(buf, "%s = (%s)(%s)", set_str->buffer, cast->buffer,
                            code->buffer);
                    else {
                        // let compiler warn or error
                        sprintf(buf, "%s = (%s)", set_str->buffer,
                            code->buffer);
                    }
                    call(output, concat_cstring, buf);
                } else {
                    if (check_only) {
                        fprintf(stderr, "check_only == true\n");
                        exit(1);
                    }
                    if (cast)
                        sprintf(buf, "(%s)update_var((base_Base *)&(%s), (base_Base)(%s))", cast->buffer, set_str->buffer, code->buffer);
                    else {
                        fprintf(stderr, "Invalid object assignment\n");
                        exit(1);
                    }
                    call(output, concat_cstring, buf);
                }
            } else {
                if (setter_method) {
                    sprintf(buf, "%s %s)", set_str->buffer, code->buffer);
                    call(output, concat_cstring, buf);
                } else if (tracked) {
                    // usually this is going to be something like setting the object to null
                    sprintf(buf, "update_var((base_Base *)&(%s), (base_Base)(%s))", set_str->buffer, code->buffer);
                    call(output, concat_cstring, buf);
                } else {
                    call(output, concat_string, set_str);
                    call(self, token_out, t_assign, 0, output);
                    call(output, concat_string, code);
                }
            }
            release(set_str);
        } else if (md) {
            if (md->getter_start) {
                //if (md->type_cd)
                //    *flags |= CODE_FLAG_ALLOC; do not tell the caller to wrap this in some gen var to later release
                // call explicit getter (class or instance)
                sprintf(buf, "%s->get_", class_var);
                call(output, concat_cstring, buf);
                call(self, token_out, t_member, '(', output);
                if (closure_scoped)
                    call(output, concat_cstring, "__scope->");
                if (is_instance)
                    call(output, concat_string, target);
                sprintf(buf, ")");
                call(output, concat_cstring, buf);
            } else {
                // get object->var (class or instance)
                if (is_instance) {
                    if (closure_scoped)
                        call(output, concat_cstring, "__scope->");
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
            ClassDec cd_lookup = call(self, scope_lookup, scope, target, NULL, NULL, NULL);
            cd_last = cd_lookup;
            *type_last = cd_lookup->class_name; // todo: map to aliased name
            if (closure_scoped)
                call(output, concat_cstring, "__scope->");
            call(self, token_out, t_member, ' ', output);
            t = *t_after = t_member;
        }
    }
    if (cd_last && (t + 1)->punct == ".") {
        output = call(self, var_op_out, scope, t + 2, cd_last, output, true,
            t_after, type_last, flags, method, brace_depth, false, false); 
    }
    return output;
}

ClassDec CX_scope_lookup(CX self, List scope, String var, Pairs *found, String *name, bool *closure_scoped) {
    LList *list = &scope->list;
    Pairs p = (Pairs)list->last->data;
    for (LItem *_i = list->last; _i; _i = _i->prev,
            p = _i ? (typeof(p))_i->data : NULL) {
        String key = pairs_key(p, var, String);
        if (key) {
            ClassDec cd = pairs_value(p, key, ClassDec);
            if (cd) {
                if (closure_scoped)
                    *closure_scoped = ((key->flags & CLOSURE_FLAG_SCOPED) != 0);
                if (name)
                    *name = cd->class_name;
                if (found)
                    *found = p;
                return cd;
            } else {
                String type_name = pairs_value(p, key, String);
                if (type_name) {
                    if (closure_scoped)
                        *closure_scoped = ((key->flags & CLOSURE_FLAG_SCOPED) != 0);
                    if (name)
                        *name = type_name;
                    if (found)
                        *found = NULL;
                    return NULL;
                }
            }
        }
    }
    if (closure_scoped)
        *closure_scoped = false;
    if (name)
        *name = NULL;
    if (found)
        *found = NULL;
    return NULL;
}

String CX_scope_end(CX self, List scope, Token *end_marker) {
    String output = NULL;
    Pairs top;
    if ((top = (Pairs)call(scope, last))) {
        char buf[1024];
        output = new(String);
        KeyValue kv;
        each_pair(top, kv) {
            String name = (String)kv->key;
            ClassDec cd_var = inherits(kv->value, ClassDec);
            if (!cd_var)
                continue;
            bool check_only = false; // gen_var's are check_only
            if (call(self, is_tracking, top, name, &check_only)) {
                sprintf(buf, "\t%s->%s(%s);\n",
                    cd_var->class_name->buffer,
                    check_only ? "check_release" : "release", name->buffer);
                call(output, concat_cstring, buf);
            }
        }
        list_pop(scope, Pairs);
        call(self, line_directive, end_marker, output);
    }
    return output;
}

void CX_line_directive(CX self, Token *t, String output) {
    return;
    if (!t)
        return;
    if (self->directive_last_line == t->line && self->directive_last_file && 
            call(t->file, compare, self->directive_last_file) == 0)
        return;
    char buf[1024];
    int last_char = max(0, output->length - 1);
    sprintf(buf, "%s# %d \"%s\"\n", (output->buffer[last_char] == '\n' ? "" : "\n"), t->line, t->file->buffer);
    call(output, concat_cstring, buf);
    self->directive_last_line = t->line;
    self->directive_last_file = t->file;
}

String CX_code_block_out(CX self, List scope, ClassDec super_mode, Token *b_start, Token *b_end, Token **t_after, MemberDec method, int *brace_depth) {
    String output = new(String);
    if ((*brace_depth)++ == 0) {
        while (output->length > 0) {
            if (isspace(output->buffer[output->length - 1])) {
                output->length--;
                output->buffer[output->length] = 0;
            } else {
                break;
            }
        }
        if (super_mode) {
            String super_code = call(self, super_out, scope, super_mode, b_start, b_end);
            if (super_code) {
                call(output, concat_string, super_code);
                release(super_code);
            }
        }
    }
    list_push(scope, new(Pairs));
    Pairs top = (Pairs)call(scope, last);
    String type_last = NULL;
    int code_block_flags = 0;
    String code = call(self, code_out, scope, b_start, b_end, t_after,
        super_mode, false, &type_last, method, brace_depth, &code_block_flags, false);
    Pairs top2 = (Pairs)call(scope, last);

    KeyValue kv;
    Pairs types = new(Pairs);
    String types_str = new(String);
    each_pair(top, kv) {
        String var = (String)kv->key;
        ClassDec cd = inherits(kv->value, ClassDec);
        if (!cd)
            continue;
        bool check_only = false;
        if (call(self, is_tracking, top, var, &check_only) && check_only) {
            List types_for = pairs_value(types, cd, List);
            if (!types_for) {
                types_for = new(List);
                pairs_add(types, cd, types_for);
                release(types_for);
            }
            list_push(types_for, var);
        }
    }
    bool types_first = true;
    each_pair(types, kv) {
        if (types_first) {
            call(types_str, concat_cstring, "\n");
            types_first = false;
        }
        ClassDec cd = inherits(kv->key, ClassDec);
        if (!cd)
            continue;
        List types_for = (List)kv->value;
        String var;
        call(types_str, concat_string, cd->struct_object); // todo: Lookup alias name
        call(types_str, concat_char, ' ');
        bool f = true;
        each(types_for, var) {
            if (!f)
                call(types_str, concat_cstring, ", ");
            call(types_str, concat_string, var);
            f = false;
        }
        call(types_str, concat_cstring, ";\n");
    }
    
    call(output, concat_string, types_str);
    call(self, line_directive, b_start, output);
    call(output, concat_string, code);
    return output;
}

void CX_code_block_end(CX self, List scope, Token *t, int *brace_depth, String output) {
    // release last scope
    Pairs top = (Pairs)call(scope, last);
    if (top) {
        if ((top->user_flags & 1) == 0) {
            String scope_end = call(self, scope_end, scope, t);
            if (scope_end) {
                call(output, concat_string, scope_end);
                release(scope_end);
            }
        } else {
            list_pop(scope, Pairs);
        }
    }
}

void CX_code_return(CX self, List scope, Token *t, Token **t_out, Token **t_after, MemberDec method, String *type_last, int *brace_depth, String output) {
    char buf[1024];
    Token *t_ret = t;
    token_next(&t);
    int token_count = call(self, read_expression, t, NULL, NULL, ";", 0, false);
    String ret_code = NULL;
    if (token_count > 0) {
        int ret_code_flags = 0;
        ret_code = call(self, code_out, scope, t, &t[token_count - 1], t_after,
            NULL, false, type_last, method, brace_depth, &ret_code_flags, true);
        String ret_type = (method && method->type_cd) ? method->type_cd->struct_object : method->type_str;
        sprintf(buf, "%s __ret = %s;\n", ret_type->buffer, ret_code->buffer);
        call(output, concat_cstring, buf);
    }
    Pairs sc;
    int sc_count = list_count(scope);
    int sc_index = sc_count - 1;
    bool single = false;
    reverse(scope, sc) {
        if (sc_index == 0) // do NOT release the args
            break;
        KeyValue kv;
        each_pair(sc, kv) {
            String name = (String)kv->key;
            ClassDec cd_var = inherits(kv->value, ClassDec);
            if (!cd_var)
                continue;
            bool check_only = false; // gen_var's are check_only
            bool tracking = call(self, is_tracking, sc, name, &check_only);
            if (tracking) {
                single = true;
                if (!ret_code || call(ret_code, compare, name) != 0) {
                    sprintf(buf, "\t%s->%s(%s);\n",
                        cd_var->class_name->buffer,
                        check_only ? "check_release" : "release", name->buffer);
                } else {
                    sprintf(buf, "\t%s->defer_release(%s);\n",
                        cd_var->class_name->buffer, name->buffer);
                }
                call(output, concat_cstring, buf);
            }
        }
        sc_index--;
    }
    if (single)
        call(output, concat_cstring, "\t");
    if (token_count > 0)
        call(output, concat_cstring, "return __ret;\n");
    else
        call(output, concat_cstring, "return;\n");
    *t_out = &t[token_count];
    Pairs top = (Pairs)call(scope, last);
    top->user_flags = 1;
}

ClosureDec CX_gen_closure(CX self, List scope, Token *b_arg_start, int n_arg_tokens, Token *b_start, Token *b_end, MemberDec method) {
    Pairs ref_scope = new(Pairs);
    bool skip_ident = false;
    // cheap way to gather up scoped references; be sure to avoid sub-closures
    for (Token *t = b_start; t->value; t++) {
        if (!skip_ident) {
            if (t->type == TT_Identifier) {
                String type = NULL;
                ClassDec cd = call(self, scope_lookup, scope, t->str, NULL, &type, NULL);
                if (cd || type) {
                    String key = cp(t->str);
                    key->flags = CLOSURE_FLAG_SCOPED;
                    if (cd)
                        pairs_add(ref_scope, key, cd);
                    else
                        pairs_add(ref_scope, key, type);
                }
            }
        }
        skip_ident = false;
        if (t->punct == "." || t->punct == "->")
            skip_ident = true;
        if (t == b_end)
            break;
    }
    Token *t_after = NULL;
    String type_last = NULL;
    int brace_depth = 0;
    int closure_flags = 0;
    char buf[1024];
    sprintf(buf, "__closure_%d", self->closures->list.count);

    // needs a gen_var for the ref_scope, and that needs to be injected on all of the locals (including self, etc)
    ClosureDec closure_dec = new(ClosureDec);
    closure_dec->str_name = new_string(buf); 
    closure_dec->ref_scope = cp(ref_scope);
    closure_dec->block_start = b_start;
    closure_dec->block_end = b_end;
    closure_dec->parent = inherits(method, ClosureDec);
    List new_scope = new(List);
    list_push(new_scope, ref_scope);
    list_push(self->closures, closure_dec);

    // have a different type associated to virtually scoped strings
    closure_dec->code = call(self, code_out, new_scope, b_start, b_end, &t_after, false, false, &type_last,
        (MemberDec)closure_dec, &brace_depth, &closure_flags, false);
    call(closure_dec, read_args, b_arg_start, n_arg_tokens, true);
    
    release(new_scope);
    return closure_dec;
}

String CX_closure_out(CX self, List scope, ClosureDec closure_dec, bool assign) {
    /* implement init function that takes in struct literal and outputs allocated copy, as well as retains all object members */
    char *name = closure_dec->str_name->buffer;
    char buf[1024];
    String init_out = new(String);
    sprintf(buf, "static void *%s_init(struct %s lit) {\n", name, name);
    call(init_out, concat_cstring, buf);
    sprintf(buf, "\tstruct %s *value = (struct %s *)malloc(sizeof(lit));\n", name, name);
    call(init_out, concat_cstring, buf);
    sprintf(buf, "\tmemcpy(value, &lit, sizeof(lit));\n");
    call(init_out, concat_cstring, buf);
    KeyValue kv;
    each_pair(closure_dec->ref_scope, kv) {
        String var = (String)kv->key;
        ClassDec cd = inherits(kv->value, ClassDec);
        if (cd) {
            sprintf(buf, "\t%s->retain(value->%s);\n", cd->class_name->buffer, var->buffer);
            call(init_out, concat_cstring, buf);
        }
    }
    sprintf(buf, "\treturn value;\n");
    call(init_out, concat_cstring, buf);
    sprintf(buf, "}\n");
    call(init_out, concat_cstring, buf);
    list_push(self->misc_code, init_out);
    release(init_out);

    /* create dealloc function */
    String dealloc_out = new(String);
    sprintf(buf, "static void *%s_dealloc(struct %s *value) {\n", name, name);
    call(dealloc_out, concat_cstring, buf);
    each_pair(closure_dec->ref_scope, kv) {
        String var = (String)kv->key;
        ClassDec cd = inherits(kv->value, ClassDec);
        if (cd) {
            sprintf(buf, "\t%s->release(value->%s);\n", cd->class_name->buffer, var->buffer);
            call(dealloc_out, concat_cstring, buf);
        }
    }
    sprintf(buf, "}\n");
    call(dealloc_out, concat_cstring, buf);
    list_push(self->misc_code, dealloc_out);
    release(dealloc_out);

    /* create struct literal string */
    String literal = new(String);
    sprintf(buf, "(struct %s){", name);
    call(literal, concat_cstring, buf);
    bool first = true;
    each_pair(closure_dec->ref_scope, kv) {
        String var = (String)kv->key;
        // if the var is in the actual scope and flagged as a non-closure then
        bool closure_scoped = false;
        call(self, scope_lookup, scope, var, NULL, NULL, &closure_scoped);
        if (!first)
            call(literal, concat_char, ',');
        if (closure_scoped)
            call(literal, concat_cstring, "__scope->");
        call(literal, concat_string, var);
        first = false;
    }
    call(literal, concat_char, '}');

    String output = new(String);
    /* construct new closure with init call given struct literal, and dealloc function */
    sprintf(buf, "Closure->init_closure((base_Closure)Base->new_object(Base, (base_Class)Closure, 0), (ClosureFunc)%s, (void *)%s_init(%s), (ClosureDealloc)%s_dealloc)",
        name, name, literal->buffer, name);
    call(output, concat_cstring, buf);
    release(literal);

    String closure_class = new_string("Closure");
    ClassDec cd_closure = pairs_value(self->static_class_map, closure_class, ClassDec);
    if (!assign) {
        String gen_out = call(self, var_gen_out, scope, cd_closure, output);
        release(output);
        return gen_out;
    }
    return output;
}

String CX_code_out(CX self, List scope, Token *method_start, Token *method_end, Token **t_after,
        ClassDec super_mode, bool line_no, String *type_last, MemberDec method, int *brace_depth, int *flags, bool assign) {
    *type_last = NULL;
    String output = new(String);
    char buf[1024];
    bool first = true;
    for (Token *t = method_start; ; t++) {
        if (t->skip)
            continue;
        if (t->keyword == "new")
            token_next(&t);
        if (t->punct == "{" || t->keyword == "for") {
            call(self, token_out, t, ' ', output);
            if (t->punct == "{") {
                Token *b_start = NULL, *b_end = NULL;
                int token_count = call(self, read_block, t, &b_start, &b_end);
                t = &t[token_count];
                String code = call(self, code_block_out, scope, super_mode, b_start + 1, b_end, t_after, method, brace_depth);
                call(output, concat_string, code);
            } else {
                list_push(scope, new(Pairs)); // new scope in for expression
                Token *block_start = NULL, *block_end = NULL;
                int n = call(self, read_expression, t + 1, &block_start, &block_end, ")", -1, true);
                if (!n || !block_start) {
                    fprintf(stderr, "Invalid for statement\n");
                    exit(1);
                }
                token_next(&t);
                int for_code_flags = 0;
                String for_code = call(self, code_out, scope, t, &t[n], t_after,
                    NULL, false, type_last, method, brace_depth, &for_code_flags, false);
                call(output, concat_string, for_code);
                int for_block_flags = 0;
                String for_block = call(self, code_out, scope, block_start, block_end, t_after,
                    NULL, false, type_last, method, brace_depth, &for_block_flags, false);
                call(output, concat_string, for_block);
                list_pop(scope, Pairs);
                t = block_end;
            }
        } else if (t->keyword == "if" || t->keyword == "while") {
            // check for beginning block
            int n = call(self, read_expression, t, NULL, NULL, ")", -1, true);
            Token *t_if = &t[n + 1];
            call(self, token_out, t, ' ', output);
            if (t_if->punct != "{") {
                // code out with injected block {'s
                int token_count = call(self, read_expression, t_if, NULL, NULL, ";", 0, false);
                if (token_count > 0) {
                    int if_code_flags = 0;
                    String if_code = call(self, code_out, scope, t + 2, &t[n - 1], t_after,
                        NULL, false, type_last, method, brace_depth, &if_code_flags, false);

                    call(self, token_out, t + 1, ' ', output);
                    call(output, concat_string, if_code);
                    call(self, token_out, t_if - 1, ' ', output);
                    call(output, concat_cstring, "{\n");

                    Token *b_start = NULL, *b_end = NULL;
                    String block_code = call(self, code_block_out, scope, super_mode, t_if, &t_if[token_count], t_after, method, brace_depth);
                    call(output, concat_string, block_code);
                    call(self, code_block_end, scope, NULL, brace_depth, output);
                    call(output, concat_cstring, "\n}\n");
                    
                    t = &t_if[token_count];

                    call(self, line_directive, t + 1, output);
                }
            }
        } else if (t->punct == "}") {
            call(self, code_block_end, scope, t, brace_depth, output);
            call(self, token_out, t, ' ', output);
        } else if (t->keyword == "return") {
            call(self, code_return, scope, t, &t, t_after, method, type_last, brace_depth, output);
            if (t + 1 < method_end)
                call(self, line_directive, t + 1, output);
        } else if (t->punct == "(") {
            Token *t_prev = t - 1;
            Token *t_next = t + 1;
            bool token_out = true;
            if ((t_next->type == TT_Identifier || t_next->type_keyword) && t_prev->type != TT_Identifier && t_prev->type != TT_Keyword) {
                Token *t_from = NULL;
                String type_expected = new(String);
                bool first = true;
                int n_arg_tokens = 1;
                for (Token *t_search = t + 1; t_search; t_search++) {
                    if (t_search->punct == ")") {
                        t_from = t_search + 1;
                        break;
                    } else if (t_search == method_end)
                        break;
                    if (!first)
                        call(type_expected, concat_char, ' ');
                    call(type_expected, concat_string, t_search->str);
                    first = false;
                    n_arg_tokens++;
                }
                if (t_from) {
                    if (t_from->punct == "{") {
                        Token *b_start = NULL, *b_end = NULL;
                        call(self, read_block, t_from, &b_start, &b_end);
                        ClosureDec closure_dec = call(self, gen_closure, scope, t + 1, n_arg_tokens, b_start, b_end, method);
                        if (closure_dec) {
                            String closure_out = call(self, closure_out, scope, closure_dec, assign);
                            call(output, concat_string, closure_out);
                            *type_last = new_string("Closure");
                            *flags |= CODE_FLAG_ALLOC;
                        }
                        token_out = false;
                        t = b_end;
                        // output constructor for closure, setting function pointer to the static that will be declared above
                        // generate static function 
                    } else {
                        if (t_after)
                            *t_after = t_from;
                        int token_count = call(self, read_expression, t_from, NULL, NULL, "{;),", 0, false);
                        String code = call(self, code_out, scope, t_from, &t_from[token_count - 1], t_after,
                            NULL, false, type_last, method, brace_depth, flags, false);
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
                    }
                }
                release(type_expected);
            }
            if (token_out)
                call(self, token_out, t, 0, output);
        } else if (t->type == TT_Keyword || t->type == TT_Identifier || t->keyword == "class") {
            Token *t_start = t;
            String target = t->str;
            ClassDec cd = NULL;
            bool is_class = false;
            Pairs top = (Pairs)call(scope, last);

            // check for 'Class' in the token
            if (target->length > 5 && strcmp(&target->buffer[target->length - 5], "Class") == 0) {
                String sub = class_call(String, new_from_bytes, (uint8 *)target->buffer, t->length - 5); 
                cd = pairs_value(self->static_class_map, sub, ClassDec);
                is_class = cd != NULL;
            } else
                cd = t->cd;

            Token *tt = t;
            while (tt->type == TT_Identifier || tt->type == TT_Keyword)
                tt++;
            Base type = NULL;
            if (call((t + 1)->str, cmp, "testme") == 0) {
                int test = 0;
                test++;
            }
            if (call((t + 0)->str, cmp, "wchar_t") == 0) {
                int test = 0;
                test++;
            }
            if (call((t + 1)->str, cmp, "testme2") == 0) {
                int test = 0;
                test++;
            }
            bool declared = false;
            if (((tt != t + 1) || (((tt - 1)->cd || (tt - 1)->type_keyword) && tt->punct == "[")) && tt->type != TT_Keyword) {
                tt--;
                type = call(self, read_type_at, tt);
                if (type) {
                    pairs_add(top, tt->str, type);
                    target = tt->str;
                    t = tt;
                    declared = true;
                    cd = inherits(type, ClassDec);
                }
            }
            if (!inherits(type, ClassDec)) {
                for (Token *tt = t_start; tt < t; tt++) {
                    token_out(tt, 0, output);
                }
            }

            if (cd) {
                // check for new/auto keyword before
                Token *t_start = t;
                if (!declared)
                    token_next(&t);
                if (t->type == TT_Identifier) {
                    call(output, concat_string, is_class ? cd->struct_class : cd->struct_object);
                    call(output, concat_char, ' ');
                    // variable declared
                    String str_name = class_call(String, new_from_bytes, (uint8 *)t->value, t->length);
                    target = t->str;
                    //pairs_add(top, str_name, cd);
                    String out = call(self, var_op_out, scope, t, cd, target, false, &t,
                        type_last, flags, method, brace_depth, false, false);
                    call(output, concat_string, out);
                } else if (t->punct == ".") {
                    token_next(&t);
                    if (t->type == TT_Identifier) {
                        String out = call(self, var_op_out, scope, t, cd, target, false, &t,
                            type_last, flags, method, brace_depth, assign, false);
                        call(output, concat_string, out);
                    }
                } else if (t->punct == "(") {
                    if (is_class) {
                        fprintf(stderr, "constructor cannot be called directly on class type\n");
                        exit(1);
                    }
                    // construct (default)
                    String out = call(self, var_op_out, scope, t - 1, cd, target, false, &t,
                        type_last, flags, method, brace_depth, assign, false);
                    call(output, concat_string, out);
                } else {
                    call(output, concat_string, is_class ? cd->struct_class : cd->struct_object);
                    call(output, concat_char, ' ');
                    call(self, token_out, t, ' ', output);
                }
            } else {
                String type_name = NULL;
                bool closure_scoped = false;
                ClassDec cd = call(self, scope_lookup, scope, target, NULL, NULL, (type ? NULL : &closure_scoped));
                bool found = false;

                if (cd && !type) {
                    if ((t + 1)->punct == ".") {
                        if ((t + 2)->type == TT_Identifier) {
                            String out = call(self, var_op_out, scope, t + 2, cd, target, true, &t,
                                type_last, flags, method, brace_depth, assign, closure_scoped);
                            call(output, concat_string, out);
                            found = true;
                        }
                    } else {
                        String out = call(self, var_op_out, scope, t, cd, target, true, &t,
                            type_last, flags, method, brace_depth, assign, closure_scoped);
                        call(output, concat_string, out);
                        found = true;
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
    if (output->length > 0) {
        Pairs scope_at = NULL; // needs to check if var is within closure scope
        ClassDec cd_found = call(self, scope_lookup, scope, output, &scope_at, NULL, NULL);
        if (cd_found) {
            // if this scoped var was tracked, set CODE_FLAG_ALLOC
            bool check_only = false;
            if (call(self, is_tracking, scope_at, output, &check_only))
                *flags |= CODE_FLAG_ALLOC;
            *type_last = cd_found->class_name;
        }
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

Base CX_read_type_at(CX self, Token *t) {
    if (call(t->str, cmp, "testme2") == 0) {
        int test = 0;
        test++;
    }
    Token *ahead = t + 1;
    if (ahead->punct != "[" && ahead->punct != "(" && ahead->punct == ";" && !ahead->assign)
        return NULL;
    if (!t->type_keyword || (t->type != TT_Identifier && t->keyword != "class" && t->keyword != "this"))
        return NULL;
    Token *cur = t - 1;
    int type_keywords = 0;
    ClassDec cd = NULL;
    while (cur->type != TT_Punctuator && cur->punct != "*") {
        if (cur->cd)
            cd = cur->cd;
        if (cur->type_keyword)
            type_keywords++;
        cur--;
    }
    ClassDec class_type = NULL;
    cur++;
    if (cur >= t)
        return NULL;
    else if (type_keywords > 0 && ahead->punct == "(") {
        ClosureClass closure = new(ClosureClass);
        closure->class_name = new_string("Closure");
        closure->struct_object = new_string("base_Closure");
        closure->struct_class = new_string("base_ClosureClass");
        closure->class_var = new_string("base_Closure_var");
        
        ahead->skip = true; /* prevent parser getting thrown off from this from looking like a method invokation */
        int p = 0;
        Token *t_start = t + 2;
        for (Token *tt = t + 2; tt->value; tt++) {
            bool push = false, br = false;
            tt->skip = true;
            if (tt->punct == "(") {
                ++p;
            } else if (tt->punct == ")") {
                if (--p <= 0) {
                    push = true;
                    br = true;
                }
            } else if (tt->punct == "," && p == 1) {
                push = true;
            }
            if (push) {
                String arg = new(String);
                for (Token *z = t_start; z < tt; z++) {
                    token_out(z, 0, arg);
                }
                list_push(closure->args, arg);
                t_start = tt + 1;
                if (br)
                    break;
            }
        }
        class_type = (ClassDec)closure;
    } else if (type_keywords > 0 && ahead->punct == "[") {
        ArrayClass array = new(ArrayClass);
        array->class_name = new_string("Array");
        array->struct_object = new_string("base_Array");
        array->struct_class = new_string("base_ArrayClass");
        array->class_var = new_string("base_Array_var");
        array->delim_start = ahead;
        int d = 0;
        Token *t_start = t + 2;
        for (Token *tt = t + 2; tt->value; tt++) {
            bool push = false, br = false;
            tt->skip = true;
            if (tt->punct == "[") {
                ++d;
            } else if (tt->punct == "]") {
                if (--d == 0) {
                    array->delim_end = tt;
                    break;
                }
            }
        }
        if (!array->delim_end) {
            fprintf(stderr, "expected delimeter end ']'\n");
            exit(1);
        }
        class_type = (ClassDec)array;
    } else if (cd)
        return base(cd);
    else if (type_keywords == 0)
        return NULL;
    
    String type_name = new(String);
    bool first = true;
    do {
        if (!first)
            call(type_name, concat_char, ' ');
        first = false;
        call(type_name, concat_string, cur->str);
        cur++;
    } while (cur < t);
    if (class_type) {
        class_type->type_str = type_name;
    }
    return class_type ? base(class_type) : base(type_name);
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
    for (int i = 0; i < md->arg_types_count; i++) {
        if (aout > 0)
            call(output, concat_cstring, ", ");
        bool preserve = md->arg_preserve[i];
        for (int ii = 0; ii < md->at_token_count[i]; ii++) {
            Token *t = &(md->arg_types[i])[ii];
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
            if (top) {
                Base type = call(self, read_type_at, t_name);
                pairs_add(top, t_name->str, type);
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
        if (call(skey, cmp, "_init") != 0) {
            MemberDec md = pairs_value(*pairs, kv->key, MemberDec);
            if (!md)
                pairs_add(*pairs, kv->key, kv->value);
        }
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

String CX_forward_type(CX self, ClassDec cd, MemberDec md) {
    String output = new(String);
    char buf[1024];
    for (int i = 0; i < md->type_count; i++) {
        String ts = md->type[i].str;
        ClassDec forward = pairs_value(self->static_class_map, ts, ClassDec);
        // if this return type is st
        if (forward == md->cd) {
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
    return output;
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
        call(self, resolve_member_types, cd);
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
                String forward_type = call(self, forward_type, cd, md);
                // need to find actual symbol name
                switch (md->member_type) {
                    case MT_Method:
                        // need a way to gather forwards referenced in method returns, arguments, and property types
                        sprintf(buf, "\t%s", forward_type->buffer);
                        call(output, concat_cstring, buf);
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
                            sprintf(buf, "%s (*set_%s)(struct _%s *, ", forward_type->buffer, md->str_name->buffer,
                                cd->struct_class->buffer);
                            call(output, concat_cstring, buf);
                        } else {
                            sprintf(buf, "%s (*set_%s)(struct _%s *, ", forward_type->buffer, md->str_name->buffer,
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
    
    List classes = new(List);
    each_pair(self->classes, kv)
        list_push(classes, kv->value);
    call(classes, sort, true, (SortMethod)sort_classes);

    ClassDec cd;
    each(classes, cd) {
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
    each(classes, cd) {
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

void CX_resolve_member_types(CX self, ClassDec cd) {
    KeyValue mkv;
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
    }
}

void CX_pretty_token(CX self, int brace_depth, Token *t, String output) {
    char buf[1024];
    int n = 0;
    buf[0] = 0;

    for (int i = 0; i < t->length; i++) {
        n += sprintf(buf + n, "%c", t->value[i]);
    }
    int n_lines = 0;
    int n_spaces = 0;
    const char *after = &t->value[t->length];
    while (*after && isspace(*after)) {
        if (*after == 10)
            n_lines++;
        else if (*after == 32)
            n_spaces++;
        after++;
    }
    if (t->punct == ",")
        n_spaces = 1;
    for (int i = 0; i < n_spaces; i++) {
        n += sprintf(buf + n, " ");
    }
    if ((t + 1)->punct == "}")
        brace_depth--;
    for (int i = 0; i < n_lines; i++) {
        n += sprintf(buf + n, "\n");
        for (int ii = 0; ii < brace_depth; ii++) {
            n += sprintf(buf + n, "\t");
        }
    }

    call(output, concat_cstring, buf);
}

String CX_pretty_print(CX self, String input) {
    int n_tokens = 0;
    List contents = new(List);
    list_push(contents, input);
    Token *tokens = call(self, read_tokens, contents, NULL, &n_tokens);
    int brace_depth = 0;
    String output = new(String);

    for (int i = 0; i < n_tokens; i++) {
        Token *t = &tokens[i];
        if (t->skip)
            continue;
        if (t->punct == "{")
            brace_depth += 1;
        else if (t->punct == "}")
            brace_depth -= 1;
        call(self, pretty_token, brace_depth, t, output);
    }
    return output;
}

// verify the code executes as planned
bool CX_emit_implementation(CX self, FILE *file_output) {
    String output = new(String);
    char buf[1024];
    List scope = new(List);
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
        each_pair(m, mkv) {
            list_clear(scope);
            list_push(scope, new(Pairs));
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
                int init_code_flags = 0;
                int brace_depth = 0;
                String code = call(self, code_out, scope, md->assign, &md->assign[md->assign_count - 1],
                    NULL, NULL, false, &type_last, md, &brace_depth, &init_code_flags, false);
                call(output, concat_string, code);

                if (md->setter_start) {
                    sprintf(buf, ")");
                    call(output, concat_cstring, buf);
                }
                sprintf(buf, ";\n");
                call(output, concat_cstring, buf);
            }
        }
        sprintf(buf, "}\n");
        call(output, concat_cstring, buf);

        each_pair(cd->members, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            ClassDec super_mode = !md->is_static ? cd : NULL;

            if (!md->block_start) {
                // for normal var declarations, output stub for getter / setter
                if (md->member_type == MT_Prop && !md->is_private) {
                    const char *tn = md->type_str->buffer;
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
            list_clear(scope);
            list_push(scope, top);
            switch (md->member_type) {
                case MT_Method: {
                    call(output, concat_string, md->type_str);
                    sprintf(buf, " %s_%s(", class_name, md->str_name->buffer);
                    call(output, concat_cstring, buf);

                    String code = call(self, args_out, top, cd, md, !md->is_static, true, 0, false);
                    call(output, concat_string, code);

                    sprintf(buf, ") ");
                    call(output, concat_cstring, buf);
                    String type_last = NULL;
                    int method_code_flags;
                    int brace_depth = 0;
                    code = call(self, code_out, scope, md->block_start, md->block_end, NULL, super_mode,
                        true, &type_last, md, &brace_depth, &method_code_flags, false);
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
                        sprintf(buf, "\tbase_Array args = (base_Array)Base->new_object(Base, (base_Class)Array, 0);\n");
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
                                pairs_add(top, s_type, cd_type);
                            } else {
                                pairs_add(top, s_type, md->type_str);
                            }
                        }
                        sprintf(buf, ") {\n");
                        call(output, concat_cstring, buf);
                        if (block_start->punct == "{")
                            block_start++;
                        if (block_end->punct == "}")
                            block_end--;
                        String type_last = NULL;
                        int prop_code_flags = 0;
                        int brace_depth = 0;
                        if (i == 0) {
                            String code = call(self, code_out, scope, block_start, block_end, NULL, super_mode, true,
                                &type_last, md, &brace_depth, &prop_code_flags, false);
                            call(output, concat_string, code);
                        } else {
                            String code = call(self, code_out, scope, block_start, block_end, NULL, super_mode, true,
                                &type_last, md, &brace_depth, &prop_code_flags, false);
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
    if (list_count(self->closures) > 0) {
        fprintf(file_output, "// ---------- closures ----------\n");
        ClosureDec closure_dec;
        each(self->closures, closure_dec) {
            fprintf(file_output, "struct %s;\n", closure_dec->str_name->buffer);
        }
        String misc_code;
        each(self->misc_code, misc_code) {
            String pretty = call(self, pretty_print, misc_code);
            fprintf(file_output, "%s", pretty->buffer);
        }
        each(self->closures, closure_dec) {
            KeyValue kv;
            fprintf(file_output, "struct %s {\n", closure_dec->str_name->buffer);
            each_pair(closure_dec->ref_scope, kv) {
                String var = (String)kv->key;
                ClassDec cd = inherits(kv->value, ClassDec);
                if (cd) {
                    fprintf(file_output, "\t%s %s;\n", cd->struct_object->buffer, var->buffer);
                } else {
                    String type = inherits(kv->value, String);
                    fprintf(file_output, "\t%s %s;\n", type->buffer, var->buffer);
                }
            }
            String pretty = call(self, pretty_print, closure_dec->code);
            fprintf(file_output, "};\n");
            fprintf(file_output, "void %s(struct %s *__scope, %s) %s\n",
                closure_dec->str_name->buffer,
                closure_dec->str_name->buffer,
                closure_dec->str_args->buffer,
                pretty->buffer);
        }
    }
    String pretty = call(self, pretty_print, output);
    fprintf(file_output, "%s", pretty->buffer);
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

void ArrayClass_init(ArrayClass self) {
}

void ClosureClass_init(ClosureClass self) {
    self->args = new(List);
}

void CX_init(CX self) {
    self->modules = new(List);
    self->classes = new(Pairs);
    self->aliases = new(Pairs); // key = module, value = alias that this module is referred to as; each alias will create a static struct with all of the classes of that module contained inside
    self->includes = new(List);
    self->private_includes = new(List);
    self->forward_structs = new(List);
    self->closures = new(List);
    self->misc_code = new(List);
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

    if (call(self->name, cmp, "base") == 0) {
        fprintf(module_header, "\nstatic inline base_Base update_var(base_Base *var, base_Base value) {\n");
        fprintf(module_header, "\tbase_Base before = *var;\n");
        fprintf(module_header, "\t*var = value;\n");
        fprintf(module_header, "\tif (value)\n");
        fprintf(module_header, "\t\tvalue->cl->retain(value);\n");
        fprintf(module_header, "\tif (before)\n");
        fprintf(module_header, "\t\tbefore->cl->release(before);\n");
        fprintf(module_header, "\treturn value;\n");
        fprintf(module_header, "}\n");
    }
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
