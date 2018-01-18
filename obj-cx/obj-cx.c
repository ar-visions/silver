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

int CX_read_expression(CX self, Token *t, Token **b_start, Token **b_end) {
    int brace_depth = 0;
    int count = 0;
    int paren_depth = 0;
    bool mark_block = b_start != NULL;

    while (t->value) {
        if (t->punct == "(") {
            paren_depth++;
        } else if (t->punct == ")") {
            if (--paren_depth < 0) {
                printf("unexpected ')' in expression\n");
                exit(0);
            }
        }
        if (brace_depth == 0 && (t->punct == "{" || t->punct == ";" || t->punct == ")" || t->punct == ",")) {
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
                Token *block_start = NULL, *block_end = NULL;
                int token_count = call(self, read_expression, t, &block_start, &block_end);
                if (token_count == 0) {
                    if (md->is_static || md->is_private) {
                        printf("expected expression\n");
                        exit(0);
                    }
                    t++;
                    continue;
                }
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
                    // method
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
                md->block_start = block_start;
                md->block_end = block_end;
                for (Token *tt = t; tt != t_last; tt++) {
                    md->type_count++;
                }
                if (md->type_count == 0) {
                    printf("expected type (member)\n");
                    exit(0);
                }
                pairs_add(cd->members, str_name, md);
                if (block_end) {
                    t = block_end + 1;
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

bool CX_class_op_out(CX self, List scope, Token *t_start, Token *t,
        ClassDec cd, Token *t_ident, bool is_instance, Token **t_after) {
    String s_token = call(self, token_string, t);
    String s_ident = call(self, token_string, t_ident);
    Token *t_member = t;
    MemberDec md = call(cd, member_lookup, s_token);
    if (!md) {
        printf("member: %s not found on class: %s\n", s_token->buffer, cd->class_name->buffer);
        exit(0);
    }
    if ((++t)->punct == "(") {
        fprintf(stdout, "%s_cl.", cd->class_name->buffer);
        call(self, token_out, t, ' ');
        if (is_instance) {
            call(self, token_out, t_ident, t->punct == ")" ? ',' : ' ');
        }
        // method call
        // expect md->type == TT_Method
        if (md->member_type != MT_Method) {
            printf("invalid call to property\n");
            exit(0);
        }
        t++;
        int n = call(self, read_expression, t, NULL, NULL);
        for (int i = 0; i < n; i++) {
            call(self, token_out, &t[i], ' ');
        }
        if ((++t)->punct != ")") {
            printf("expected ')'\n");
            exit(0);
        }
        call(self, token_out, t, ' ');
        *t_after = t;
        // perform call replacement here
    } else if (t->assign) {
        Token *t_assign = t++;
        int n = call(self, read_expression, t, NULL, NULL);
        if (md->setter_start) {
            // call explicit setter
            fprintf(stdout, "%s_cl.set_", cd->class_name->buffer);
            call(self, token_out, t_member, '(');
            if (is_instance)
                call(self, token_out, t_ident, ',');
            fprintf(stdout, "(");
            call(self, code_out, scope, t, &t[n]);
            fprintf(stdout, ")");
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
                call(self, token_out, t_ident, ',');
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
        if (t->punct == "{" || t->keyword == "for") {
            call(self, token_out, t, ' ');
            if (brace_depth++ != 0)
                list_push(scope, new(Pairs));
        } else if (t->punct == "}") {
            call(self, token_out, t, ' ');
            brace_depth--;
            list_pop(scope, Pairs);
        } else if (t->type == TT_Identifier) {
            Token *t_ident = t;
            // check if identifier exists as class
            String str_ident = class_call(String, new_from_bytes, t->value, t->length);
            ClassDec cd = pairs_value(self->classes, str_ident, ClassDec);
            if (cd) {
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
                        call(self, class_op_out, scope, t_start, t, cd, t_ident, false, &t);
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
                                call(self, class_op_out, scope, t_start, t, cd, t_ident, true, &t);
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

static Token *t_token_last;

void CX_token_out(CX self, Token *t, int sep) {
    if (t_token_last == t) {
        int test = 0;
        test++;
    }
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

// verify the code executes as planned
bool CX_replace_classes(CX self) {
    List scope = new(List);
    list_push(scope, new(Pairs));
    KeyValue kv;
    each_pair(self->classes, kv) {
        ClassDec cd = (ClassDec)kv->value;
        KeyValue mkv;
        each_pair(cd->members, mkv) {
            MemberDec md = (MemberDec)mkv->value;
            if (!md->block_start)
                continue;
            Pairs top = new(Pairs);
            list_push(scope, top);
            Token *out = NULL;
            int count = 0;
            call(self, token_out, md->type, ' '); // <-- must output all of the type info; such as const Type *
            call(self, token_out, md->name, '(');
            if (!md->is_static) {
                pairs_add(top, new_string("self"), cd);
                call(self, token_out, cd->name, ' ');
                fprintf(stdout, "self");
                if (md->args_count)
                    fprintf(stdout, ", ");
            }
            for (int a = 0; a < md->args_count; a++)
                call(self, token_out, &md->args[a], ' ');
            fprintf(stdout, ")");
            call(self, code_out, scope, md->block_start, md->block_end);
            fprintf(stdout, "\n");
        }
    }
}

bool CX_process(CX self, const char *file) {
    String str = class_call(String, from_file, file);
    int n_tokens = 0;
    self->tokens = call(self, read_tokens, str, &n_tokens);

    call(self, read_classes);
    call(self, resolve_supers);
    call(self, replace_classes);

    /*

    if there is anything assigned at all, an init is created

    declare Vector <double, float> : Base {
        int test ^[key:value];
    }

    struct _class_Vector {
        struct _class_Base *parent;
        const char *name;
        const char *super_name;
        unsigned int flags;
        struct _object_Pairs *meta;
        int obj_size;
        int mcount;
        char **mnames;
        Method m[1];
        int (*get_test)(struct _object_Vector *);
        void (*set_test)(struct _object_Vector *, int);
    }

    new Object  -> new_object(Object_cl, 0)
    auto Object -> obj->cl->autorelease(new_object(Object_cl, 0))
    retain      -> obj->cl->retain(obj)
    release     -> obj->cl->release(obj)
    autorelease -> obj->cl->autorelease(obj)
    object.method_call(arguments) -> object->cl->method_call(arguments)
    .prop = value -> self->cl->set_prop(self, value)

    // Output one global constructor per module; gather up code as the module is built, and the last module will be the ctor.c
    // Use the framework implementation to allow for different parsing features, effectively using the framework to add to the language

    class var identification within scope:

    look for tokens that equal one of the names of the classes
    in each case, look to the right of that and find a possible identifier
        if identifier found, associate identifier to this class within this scope
    
    upon exiting scope, pop the pairs out of the scope

    for (type_possible; x; x) {
        // for statements can have TWO scopes
    }
    
    once you read a for keyword, expect ( ), then the scope will be defined as the expression after


    should you have a unique syntax for explicit getter and setter?

    property defines:

        Object::property {
            get {
                return self->property;
            }
            set (value) {
                self->property = value;
                .property = value;
            }
        }

    method defines:

        Type Object::method(arg, arg2) {
            
        }

    */
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