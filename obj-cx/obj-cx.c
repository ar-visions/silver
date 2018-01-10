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
        "[", "]", "(", ")", "{", "}", ".", "->",
        "++", "--", "&", "*", "+", "-", "~", "!",
        "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "^", "|", "&&", "||",
        "?", ":", ";", "...",
        "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|=",
        ",", "#", "##",
        "<:", ":>", "<%", "%>", "%:", "%:%:"
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
                        if (length == keyword_lens[k] && memcmp(t->value, keyword, length) == 0)
                            t->type = TT_Keyword;
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
                            t->type = TT_Punctuator;
                            t->length = punct_len;
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

bool CX_read_template_types(CX self, ClassDec cd, int *cursor) {
    Token *tokens = self->tokens;
    if (tokens[*cursor].punct != "<")
        return true;
    // read template types
    bool expect_identifier = true;
    for (int t = tokens[*cursor]; t < n_tokens; t++) {
        Token *tname = &tokens[t];
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
        (*cursor)++;
    }
    return true;
}

inline void expect_type(Token token, enum TokenType type) {
    if (token.type != type) {
        printf("wrong token type\n");
        exit(0);
    }
}

bool CX_process(CX self, const char *file) {
    String str = class_call(String, from_file, file);
    int n_tokens = 0;
    self->tokens = call(self, read_tokens, str, &n_tokens);
    Token *tokens = self->tokens;
    // gather up all class declarations
    
    printf("tokens:\n");

    // pass 1: collect classes
    for (int i = 0; i < n_tokens; i++) {
        Token *t = &tokens[i];
        if (t->length == 7 && strncmp(t->value, "declare", t->length) == 0) {
            ClassDec cd = new(ClassDec);
            cd->name = &tokens[i + 1]; // validate
            cd->methods = new(Pairs);
            int cursor = i + 2;
            call(self, read_template_types, cd, &cursor);
            if (tokens[cursor++].punct != "{") {
                printf("expected '{' character\n");
                exit(0);
            }

            // read optional keywords such as static and private

            // type
            expect_type(token[cursor], TT_Identifier);
            String str_type = class_call(String, new_from_bytes, token[cursor].value, token[cursor].length);
            MemberDec member = new(MemberDec);
            member->type = &token[cursor++];

            // name
            expect_type(token[cursor], TT_Identifier);
            String str_name = class_call(String, new_from_bytes, token[cursor].value, token[cursor].length);
            member->name = &token[cursor++];
            pairs_add(cd->methods, str_name, member);

            // expect meta ('[' or ';') for property, or ('[' or '(') for method
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