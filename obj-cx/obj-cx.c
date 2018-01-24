#include <obj/obj.h>
#include <obj-cx/obj-cx.h>

#define CX_VERSION  "0.4.0"

implement(CX)
implement(ClassDec)
implement(MemberDec)

MemberDec ClassDec_member_lookup(ClassDec self, String name) {
    for (ClassDec cd = self; cd; cd = cd->parent) {
        MemberDec md = pairs_value(cd->members, name, MemberDec);
        if (md)
            return md;
    }
    return NULL;
}

Token *CX_read_tokens(CX self, String str, int *n_tokens) {
    Token *tokens = array_struct(Token, str->length + 1);
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
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        if (cd->super_class)
            cd->parent = pairs_value(self->classes, cd->super_class, ClassDec);
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

/* read class information, including method and property blocks */
/* method expressions in the class must include a code block; this is optional for properties */
bool CX_read_classes(CX self) {
    self->classes = new(Pairs);
    for (Token *t = self->tokens; t->value; t++) {
        if (strncmp(t->value, "class", t->length) == 0) {
            Token *t_start = t;
            Token *t_name = ++t;
            String class_str = class_call(String, new_from_bytes, t_name->value, t_name->length);
            ClassDec cd = pairs_value(self->classes, class_str, ClassDec);
            if (!cd) {
                cd = new(ClassDec);
                pairs_add(self->classes, class_str, cd);
                cd->name = t_name; // validate
                cd->class_name = class_str;
                cd->members = new(Pairs);
                cd->start = t_start;
            }
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
                            equals = &t[e];
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
                if (t_last->punct == "]") {
                    bool found_meta = false;
                    for (Token *tt = t_last; tt != t; tt--) {
                        if (tt->punct == "^[") {
                            found_meta = true;
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
                    }
                }
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
                if (!construct_default) {
                    if (!((t_last->type == TT_Keyword && !t_last->type_keyword) || t_last->type == TT_Identifier)) {
                        printf("expected member name\n");
                        exit(0);
                    }
                    if (t_last->type != TT_Identifier) {
                        printf("expected identifier (member name)\n");
                        exit(0);
                    }
                }
                String str_name = class_call(String, new_from_bytes, t_name->value, t_name->length);
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
    return class_call(String, new_from_bytes, t->value, t->length);
}

// create stack space at the root of the method; simplest form of line # mapping
Pairs CX_stack_map(CX self, Token *from, Token *to) {
    // look for instances of Class( without new or auto before it
    // for each create an index of 0 (or increment) for each unique type
    // set association on Token * (stack_var)

    /*

    change of plans on stack-based classes:
        there will be none, but there will be structs with member functions, operator overloading, and reflection/serialization
    */
}

bool CX_class_op_out(CX self, List scope, Token *t_start, Token *t,
        ClassDec cd, Token *t_ident, bool is_instance, Token **t_after, const char *prev_keyword) {
    String s_token = call(self, token_string, t);
    String s_ident = t_ident ? call(self, token_string, t_ident) : NULL;
    Token *t_member = t;
    MemberDec md = call(cd, member_lookup, s_token);
    if (!md) {
        printf("member: %s not found on class: %s\n", s_token->buffer, cd->class_name->buffer);
        exit(0);
    }
    if ((++t)->punct == "(") {
        fprintf(stdout, "%s_cl.", cd->class_name->buffer);
        call(self, token_out, t_member, ' ');
        if (is_instance) {
            call(self, token_out, t_ident, t->punct == ")" ? ',' : ' ');
        }
        bool prepend_arg = false;
        // method call
        // expect md->type == TT_Method
        if (md->member_type == MT_Prop) {
            printf("invalid call to property\n");
            exit(0);
        }
        fprintf(stdout, "(");
        if (md->member_type == MT_Constructor) {
            prepend_arg = true;
            fprintf(stdout, "&%s_cl, %s", cd->class_name->buffer,
                prev_keyword == "auto" ? "true" : "false");
        }
        // issue with stack spaced vars is you'll need to crawl ahead and find all instances of stack vars, and create associative space for each */
        // output stack * (if stack mode), and auto (true/false)
        int n = call(self, read_expression, t, NULL, NULL, ")");
        for (int i = 1; i < n; i++) {
            if (i == 1 && prepend_arg)
                fprintf(stdout, ",");
            call(self, token_out, &t[i], ' ');
        }
        t += n;
        if (t->punct != ")") {
            printf("expected ')'\n");
            exit(0);
        }
        call(self, token_out, t, ' ');
        *t_after = t;
        // perform call replacement here
    } else if (t->assign) {
        Token *t_assign = t++;
        int n = call(self, read_expression, t, NULL, NULL, "{;),");
        if (n <= 0) {
            fprintf(stderr, "expected token after %s\n", t->punct);
            exit(0);
        }
        if (md->setter_start) {
            // call explicit setter
            fprintf(stdout, "%s_cl.set_", cd->class_name->buffer);
            call(self, token_out, t_member, '(');
            if (is_instance)
                call(self, token_out, t_ident, ',');
            call(self, code_out, scope, t, &t[n - 1]);
            fprintf(stdout, ")");
            call(self, token_out, &t[n], 0);
            *t_after = t + n;
        } else {
            // set object var
            if (is_instance) {
                call(self, token_out, t_ident, 0);
                fprintf(stdout, "->");
            } else {
                fprintf(stdout, "%s_cl.", cd->class_name->buffer);
            }
            call(self, token_out, t_member, 0);
            call(self, token_out, t_assign, 0);
            call(self, code_out, scope, t, &t[n]);
            *t_after = t + n;
        }
        // call setter if it exists (note: allow setters to be sub-classable, calling super.prop = value; and such)
    } else {
        if (md->getter_start) {
            // call explicit getter (class or instance)
            fprintf(stdout, "%s_cl.get_", cd->class_name->buffer);
            call(self, token_out, t_member, '(');
            if (is_instance)
                call(self, token_out, t_ident, 0);
            fprintf(stdout, ")");
        } else {
            // get object->var (class or instance)
            if (is_instance) {
                call(self, token_out, t_ident, 0);
                fprintf(stdout, "->");
            } else {
                fprintf(stdout, "%s_cl.", cd->class_name->buffer);
            }
            call(self, token_out, t_member, ' ');
        }
        *t_after = t_member;
    }
    return true;
}

void CX_code_out(CX self, List scope, Token *method_start, Token *method_end) {
    int brace_depth = 0;
    for (Token *t = method_start; ; t++) {
        // find 
        const char *prev_keyword = NULL;
        if (t->keyword == "new" || t->keyword == "auto") {
            prev_keyword = t->keyword;
            t++;
        }
        if (t->punct == "{" || t->keyword == "for") {
            call(self, token_out, t, ' ');
            if (brace_depth++ != 0)
                list_push(scope, new(Pairs));
        } else if (t->punct == "}") {
            call(self, token_out, t, ' ');
            brace_depth--;
            if (brace_depth != 0)
                list_pop(scope, Pairs);
        } else if (t->type == TT_Identifier) {
            Token *t_ident = t;
            // check if identifier exists as class
            String str_ident = class_call(String, new_from_bytes, t->value, t->length);
            ClassDec cd = pairs_value(self->classes, str_ident, ClassDec);
            if (cd) {
                // check for new/auto keyword before
                Token *t_start = t;
                if ((++t)->type == TT_Identifier) {
                    call(self, token_out, t - 1, ' ');
                    call(self, token_out, t, ' ');
                    // variable declared
                    String str_name = class_call(String, new_from_bytes, t->value, t->length);
                    Pairs top = (Pairs)call(scope, last);
                    pairs_add(top, str_name, cd);
                } else if (t->punct == ".") {
                    if ((++t)->type == TT_Identifier)
                        call(self, class_op_out, scope, t_start, t, cd, t_ident, false, &t, prev_keyword);
                } else if (t->punct == "(") {
                    // construct (default)
                    call(self, class_op_out, scope, t_start, t - 1, cd, t_ident, false, &t, prev_keyword);
                } else {
                    call(self, token_out, t - 1, ' ');
                    call(self, token_out, t, ' ');
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
                        Token *t_start = t;
                        if ((++t)->punct == ".") {
                            if ((++t)->type == TT_Identifier) {
                                call(self, class_op_out, scope, t_start, t, cd, t_ident, true, &t, NULL);
                                found = true;
                            }
                        }
                        break;
                    }
                }
                if (!found)
                    call(self, token_out, t, ' ');
            }
        } else {
            call(self, token_out, t, 0);
        }
        if (t == method_end)
            break;
    }
}

void CX_token_out(CX self, Token *t, int sep) {
    for (int i = 0; i < t->length; i++) {
        fprintf(stdout, "%c", t->value[i]);
    }
    const char *after = &t->value[t->length];
    while (*after && isspace(*after)) {
        fprintf(stdout, "%c", *after);
        after++;
    }
    if (sep != ' ' && sep > 0)
        fprintf(stdout, "%c", sep);
}

void CX_args_out(CX self, Pairs top, ClassDec cd, MemberDec md, bool inst, bool names, int aout) {
    if (inst) {
        if (aout > 0)
            fprintf(stdout, ", ");
        fprintf(stdout, "struct _object_");
        call(self, token_out, cd->name, ' ');
        fprintf(stdout, " *");
        if (names)
            fprintf(stdout, "self");
        if (top)
            pairs_add(top, new_string("self"), cd);
        aout++;
    }
    for (int i = 0; i < md->arg_types_count; i++) {
        if (aout > 0)
            fprintf(stdout, ", ");
        
        for (int ii = 0; ii < md->at_token_count[i]; ii++) {
            call(self, token_out, &(md->arg_types[i])[ii], ' ');
        }
        if (names) {
            Token *t_name = md->arg_names[i];
            call(self, token_out, t_name, ' ');
            if (top && md->at_token_count[i] == 1) {
                String s_type = call(self, token_string, md->arg_types[0]);
                ClassDec cd_type = pairs_value(self->classes, s_type, ClassDec);
                if (cd_type) {
                    String s_var = call(self, token_string, t_name);
                    pairs_add(top, s_var, cd_type);
                }
            }
        }
        aout++;
    }
}

void CX_effective_methods(CX self, ClassDec cd, Pairs *pairs) {
    if (cd->parent)
        call(self, effective_methods, cd->parent, pairs);
    KeyValue kv;
    each_pair(cd->members, kv) {
        pairs_add(*pairs, kv->key, kv->value);
    }
}



void CX_declare_classes(CX self) {
    KeyValue kv;
    each_pair(self->classes, kv) {
        String class_name = (String)kv->key;
        ClassDec cd = (ClassDec)kv->value;
        String super_name = cd->parent ? cd->parent->class_name : new_string("Base");
        fprintf(stdout,
            "\ntypedef struct _class_%s {\n"    \
            "\tstruct _class_%s *parent;\n"   \
            "\tconst char *class_name;\n"       \
            "\tunsigned int flags;\n"           \
            "\tint obj_size;\n"                 \
            "\tvoid (*_init)(struct _object_%s *);" \
            "\tint *mcount;\n"                  \
            "\tunsigned char *mtypes;\n"        \
            "\tconst char **mnames;\n"          \
            "\tMethod **m[1];\n", class_name->buffer, class_name->buffer, super_name->buffer);

        Pairs m = new(Pairs);
        call(self, effective_methods, cd, &m);
        cd->effective = m;
        KeyValue mkv;
        each_pair(m, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            ClassDec origin = md->cd;
            // need to find actual symbol name
            switch (md->member_type) {
                case MT_Constructor:
                case MT_Method:
                    fprintf(stdout, "\t");
                    if (md->member_type == MT_Constructor) {
                        fprintf(stdout, "%s", cd->class_name->buffer);
                    } else {
                        for (int i = 0; i < md->type_count; i++)
                            call(self, token_out, &md->type[i], ' ');
                    }
                    fprintf(stdout, " (*%s)(", md->str_name->buffer);
                    call(self, args_out, NULL, cd, md, md->member_type == MT_Method && !md->is_static, false, 0);
                    fprintf(stdout, ");\n");
                    break;
                case MT_Prop:
                    fprintf(stdout, "\t");
                    for (int i = 0; i < md->type_count; i++)
                        call(self, token_out, &md->type[i], ' ');
                    fprintf(stdout, " (*get_%s)();\n", md->str_name->buffer);
                    fprintf(stdout, "\t");
                    fprintf(stdout, "void (*set_%s)(", md->str_name->buffer);
                    for (int i = 0; i < md->type_count; i++)
                        call(self, token_out, &md->type[i], ' ');
                    fprintf(stdout, ");\n");
                    break;
            }
        }
        fprintf(stdout, "} class_%s %s_cl;\n\n", cd->class_name->buffer, cd->class_name->buffer);

        fprintf(stdout,
            "\ntypedef struct _object_%s {\n"   \
            "\tstruct _class_%s *cl;\n"         \
            "\tLItem *ar_node;\n"               \
            "\tint refs;\n",
            class_name->buffer, class_name->buffer);
        {
            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                if (md->member_type == MT_Prop && !md->getter_start) {
                    for (int i = 0; i < md->type_count; i++)
                        call(self, token_out, &md->type[i], ' ');
                    fprintf(stdout, "%s;\n", md->str_name->buffer);
                }
            }
        }
        fprintf(stdout, "} object_%s;\n\n", cd->class_name->buffer);
    }
}

void CX_define_module_constructor(CX self) {
    fprintf(stdout, "static void module_constructor(void) __attribute__(constructor) {\n");
    KeyValue kv;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        int method_count = 0;
        KeyValue mkv;
        // function pointers
        if (cd->parent) {
            fprintf(stdout, "\t%s_cl.parent = &%s_cl;\n",
                cd->class_name->buffer, cd->parent->class_name->buffer);
        }
        fprintf(stdout, "\t%s_cl._init = %s__init;\n",
            cd->class_name->buffer, cd->class_name->buffer);
        each_pair(cd->effective, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            ClassDec origin = md->cd;
            switch (md->member_type) {
                case MT_Constructor:
                case MT_Method:
                    if (md->member_type == MT_Constructor) {
                        fprintf(stdout, "\t%s_cl.%s = construct_%s_%s;\n",
                            cd->class_name->buffer, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                    } else {
                        fprintf(stdout, "\t%s_cl.%s = %s_%s;\n",
                            cd->class_name->buffer, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                    }
                    method_count++;
                    break;
                case MT_Prop:
                    fprintf(stdout, "\t%s_cl.get_%s = %s_get_%s;\n",
                        cd->class_name->buffer, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                    fprintf(stdout, "\t%s_cl.set_%s = %s_set_%s;\n",
                        cd->class_name->buffer, md->str_name->buffer, origin->class_name->buffer, md->str_name->buffer);
                    method_count += 2;
                    break;
            }
        }
        // member types
        {
            fprintf(stdout, "\t%s_cl.mtypes = (char *)malloc(%d);\n", cd->class_name->buffer, method_count);
            int mc = 0;
            each_pair(cd->effective, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                switch (md->member_type) {
                    case MT_Constructor:
                    case MT_Method:
                        fprintf(stdout, "\t%s_cl.mtypes[%d] = %d;\n", cd->class_name->buffer, mc, md->member_type); mc++;
                        break;
                    case MT_Prop:
                        fprintf(stdout, "\t%s_cl.mtypes[%d] = %d;\n", cd->class_name->buffer, mc, md->member_type); mc++;
                        fprintf(stdout, "\t%s_cl.mtypes[%d] = %d;\n", cd->class_name->buffer, mc, md->member_type); mc++;
                        break;
                }
            }
        }
        // member names
        {
            fprintf(stdout, "\t%s_cl.mnames = (const char **)malloc(%d * sizeof(const char *));\n", cd->class_name->buffer, method_count);
            int mc = 0;
            each_pair(cd->effective, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                switch (md->member_type) {
                    case MT_Constructor:
                    case MT_Method:
                        fprintf(stdout, "\t%s_cl.mnames[%d] = \"%s\";\n", cd->class_name->buffer, mc, md->str_name->buffer); mc++;
                        break;
                    case MT_Prop:
                        fprintf(stdout, "\t%s_cl.mnames[%d] = \"%s\";\n", cd->class_name->buffer, mc, md->str_name->buffer); mc++;
                        fprintf(stdout, "\t%s_cl.mnames[%d] = \"%s\";\n", cd->class_name->buffer, mc, md->str_name->buffer); mc++;
                        break;
                }
            }
        }
        fprintf(stdout, "\t%s_cl.mcount = %d;\n", cd->class_name->buffer, method_count);
    }
    fprintf(stdout, "}\n");
}

// verify the code executes as planned
bool CX_replace_classes(CX self) {
    List scope = new(List);
    list_push(scope, new(Pairs));
    KeyValue kv;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        KeyValue mkv;
        // output init
        fprintf(stdout, "void %s__init(%s self) {\n",
            cd->class_name->buffer, cd->class_name->buffer);
        Pairs m = new(Pairs);
        call(self, effective_methods, cd, &m);
        {
            each_pair(m, mkv) {
                MemberDec md = (MemberDec)mkv->value;
                if (md->member_type == MT_Prop && md->assign) {
                    if (md->setter_start)
                        fprintf(stdout, "self->cl->set_%s(self, ", md->str_name->buffer);
                    else
                        fprintf(stdout, "self->%s = ", md->str_name->buffer);
                    call(self, code_out, scope, md->assign, &md->assign[md->assign_count - 1]);
                    if (md->setter_start)
                        fprintf(stdout, ")");
                    fprintf(stdout, ";\n");
                }
            }
        }
        fprintf(stdout, "}\n");

        each_pair(cd->members, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            if (!md->block_start)
                continue;
            Pairs top = new(Pairs);
            list_push(scope, top);
            switch (md->member_type) {
                case MT_Constructor:
                    call(self, token_out, cd->name, ' ');
                    fprintf(stdout, "%s_construct_%s(", cd->class_name->buffer, md->str_name->buffer);
                    fprintf(stdout, "struct _class_%s *cl, bool ar", cd->class_name->buffer);
                    call(self, args_out, top, cd, md, false, true, 1);
                    fprintf(stdout, ") {\n");
                    fprintf(stdout, "%s self = object_alloc(cl, 0);\n",
                        cd->class_name->buffer);
                    fprintf(stdout, "self->cl->_init(self);\n");
                    pairs_add(top, new_string("self"), cd);
                    if (md->block_start != md->block_end)
                        call(self, code_out, scope, md->block_start + 1, md->block_end - 1);
                    fprintf(stdout, "return ar ? object_auto(self) : self;\n}\n");
                    fprintf(stdout, "\n");
                    break;
                case MT_Method:
                    for (int i = 0; i < md->type_count; i++)
                        call(self, token_out, &md->type[i], ' ');
                    fprintf(stdout, "%s_%s(", cd->class_name->buffer, md->str_name->buffer);
                    call(self, args_out, top, cd, md, !md->is_static, true, 0);
                    fprintf(stdout, ")");
                    call(self, code_out, scope, md->block_start, md->block_end);
                    fprintf(stdout, "\n");
                    break;
                case MT_Prop:
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
                                call(self, token_out, &md->type[i], ' ');
                            fprintf(stdout, "%s_get_", cd->class_name->buffer);
                            call(self, token_out, md->name, '(');
                        } else {
                            fprintf(stdout, "void ");
                            fprintf(stdout, "%s_set_", cd->class_name->buffer);
                            call(self, token_out, md->name, '(');
                        }
                        if (!md->is_static) {
                            call(self, token_out, cd->name, ' ');
                            fprintf(stdout, "self");
                            pairs_add(top, new_string("self"), cd);
                            if (i == 1) {
                                fprintf(stdout, ", ");
                            }
                        }
                        if (i == 1) {
                            for (int i = 0; i < md->type_count; i++)
                                call(self, token_out, &md->type[i], ' ');
                            call(self, token_out, md->setter_var, ' ');
                            String s_type = call(self, token_string, md->setter_var);
                            ClassDec cd_type = pairs_value(self->classes, s_type, ClassDec);
                            if (cd_type) {
                                String s_var = call(self, token_string, md->setter_var);
                                pairs_add(top, s_var, cd_type);
                            }
                        }
                        fprintf(stdout, ")");
                        call(self, code_out, scope, block_start, block_end);
                        fprintf(stdout, "\n");
                        call(top, clear);
                    }
                    break;
            }
        }
    }
}

bool CX_process(CX self, const char *file) {
    String str = class_call(String, from_file, file);
    int n_tokens = 0;
    self->tokens = call(self, read_tokens, str, &n_tokens);

    call(self, read_classes);
    call(self, resolve_supers);
    call(self, declare_classes);
    call(self, replace_classes);
    call(self, define_module_constructor);
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
