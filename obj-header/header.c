#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define HEADER_VERSION "0.9.0"

int char_count(char *str, char c) {
    int len = strlen(str);
    int count = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] == c)
            count++;
    }
    return count;
}

int parens_depth(char *str, int *n_open, int *n_close, char **last_parens) {
    int len = strlen(str);
    int count = 0;
    int depth = 0;
    *last_parens = NULL;
    for (int i = 0; i < len; i++) {
        if (str[i] == '(') {
            depth++;
            (*n_open)++;
        } else if (str[i] == ')') {
            *last_parens = &str[i];
            depth--;
            (*n_close)++;
            if (depth < 0)
                return -1;
        }
    }
    if (depth != 0)
        return -1;
    return 0;
}

char *copy_string(char *str) {
    int len = strlen(str);
    char *ret = (char *)malloc(len + 1);
    memcpy(ret, str, len);
    ret[len] = 0;
    return ret;
}

char *copy_to(char *str, char *up_to, bool *found_up_to, char **scan) {
    int len = strlen(str);
    char *ret;
    int index = -1;
    if (up_to) {
        char *s = strstr(str, up_to);
        if (s)
            index = (int)((size_t)(s - str));
    }
    if (index == -1) {
        ret = (char *)malloc(len + 1);
        memcpy(ret, str, len + 1);
        if (found_up_to)
            *found_up_to = false;
        if (scan)
            *scan += len;
    } else {
        ret = (char *)malloc(index + 1);
        memcpy(ret, str, index);
        ret[index] = 0;
        if (found_up_to)
            *found_up_to = true;
        if (scan)
            *scan += index;
    }
    return ret;
}

#define exit_code(c) code = c; goto exit_with;

char *read_cblock(char *in, int *p_len, char **expr_list, int *expr_count, char expr_c, bool trailing_expr) {
    *expr_count = 0;
    if (*in != '{') {
        *p_len = 0;
        return NULL;
    }
    int slen = strlen(in);
    int brace_depth = 0;
    bool comment = false;
    bool comment_single = false;
    bool preprocessor = false;
    bool first_char_line = false;
    bool finding_first = true;
    bool first_char = false;
    bool quote = false;
    int expr_start = -1;
    int ignore_chars = 0;

    for (int i = 0; i < slen; i++) {
        char c = in[i];
        bool new_line = c == '\n';
        if (new_line) {
            comment_single = false;
            finding_first = true;
        } else if (finding_first && !isspace(c)) {
            first_char = true;
            finding_first = false;
        }
        if ((!comment && !comment_single) && c == '"' && (i == 0 || in[i - 1] != '\\'))
            quote = !quote;
        else if (!quote) {
            if (!comment && !comment_single) {
                if (c == '/' && in[i + 1] == '/')
                    comment_single = true;
                else if (c == '/' && in[i + 1] == '*')
                    comment = true;
                else if (new_line && preprocessor)
                    preprocessor = in[i - 1] == '\\';
                else if (first_char && c == '#')
                    preprocessor = true;
            } else if (comment) {
                if (c == '*' && in[i + 1] == '/') {
                    comment = false;
                    ignore_chars = 2;
                }
            }
        }
        if (!quote && !comment && !comment_single && !preprocessor) {
            if (c == '{')
                brace_depth++;
            else if (c == '}') {
                if (trailing_expr) {
                    if (ignore_chars == 0 && expr_start != -1) {
                        int expr_len = i - expr_start;
                        char *expr = (char *)malloc(expr_len + 1);
                        memcpy(expr, &in[expr_start], expr_len);
                        expr[expr_len] = 0;
                        expr_list[(*expr_count)++] = expr;
                    }
                    expr_start = -1;
                }
                if (--brace_depth == 0) {
                    *p_len = i + 1;
                    char *block = (char *)malloc((*p_len) + 1);
                    memcpy(block, in, *p_len);
                    block[*p_len] = 0;
                    return block;
                }
            } else if (c == expr_c || i == (slen - 1)) {
                if (expr_start != -1) {
                    int expr_len = i - expr_start + 1;
                    char *expr = (char *)malloc(expr_len + 1);
                    memcpy(expr, &in[expr_start], expr_len);
                    expr[expr_len] = 0;
                    expr_list[(*expr_count)++] = expr;
                }
                expr_start = -1;
            } else if (ignore_chars == 0 && !isspace(c) && expr_start == -1) {
                expr_start = i;
            }
        }
        if (ignore_chars > 0)
            --ignore_chars;
        first_char = false;
    }
    return NULL;
}

char *file_def(char *file) {
    int len = strlen(file);
    int target = 2;
    int current = 0;
    char *start = NULL;
    for (int i = len - 1; i > 0; i--) {
        if (file[i] == '/' || file[i] == '\\') {
            current++;
            if (current == target) {
                start = &file[i + 1];
                break;
            }
        }
    }
    if (!start)
        start = file;
    char *cp = copy_string(start);
    for (char *p = cp; *p; p++) {
        *p = !isalnum(*p) ? '_' : toupper(*p);
    }
    int final_len = strlen(cp) + 2;
    char *f = (char *)malloc(final_len + 1);
    sprintf(f, "_%s_", cp);
    return f;
}

char *delimit_quotes(char *in) {
    int len = strlen(in);
    char *out = (char *)malloc(len * 2 + 1);
    int cursor = 0;
    for (int i = 0; i < len; i++) {
        if (in[i] == '"') {
            out[cursor++] = '\\';
            out[cursor++] = '"';
        } else {
            out[cursor++] = in[i];
        }
    }
    out[cursor++] = 0;
    return out;
}

void main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("libobj header preprocessor -- version %s\n", HEADER_VERSION);
        printf("usage: obj-header input.hh output.h\n");
        exit(1);
    }
    int code = 0;
    char *input = argv[1];
    char *output = argv[2];
    FILE *fout = NULL;
    FILE *fin = fopen(input, "r");
    char *file_def_str = file_def(input);
    if (!fin) {
        fprintf(stderr, "%s: not readable\n", input);
        exit_code(1);
    }
    fout = fopen(output, "w");
    if (!fout) {
        fprintf(stderr, "%s: not writable\n", output);
        exit_code(1);
    }
    // read file line by line
    // check for comments
    fseek(fin, 0, SEEK_END);
    long file_len = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (file_len < 0) {
        exit_code(1);
    }
    char *in = (char *)malloc(file_len + 1);
    if (fread(in, file_len, 1, fin) != 1) {
        exit_code(1);
    }
    in[file_len] = 0;
    char *buf = (char *)malloc(file_len + 1);
    buf[0] = 0;
    // file input line by line
    char *line_start = in;
    bool rn = false;
    int block_len = 1;

    fprintf(fout, "#ifndef %s\n#define %s\n\n", file_def_str, file_def_str);

    for (int i = 0; i < (int)file_len; i += block_len) {
        block_len = 1;
        char c = in[i];
        bool new_line = c == '\n';
        bool end = i == file_len - 1;
        rn = (c == '\r' && in[i + 1] == '\n');
        char *start = &in[i];
        int stop_at = 0;
        bool token_read = sscanf(start, "%s%n", buf, &stop_at) == 1;
        if (!new_line && token_read && strcmp(buf, "class") == 0) {
            int len = strlen(start);
            int index = 0;
            char *cname = (char *)malloc(len + 1);
            char *sname = (char *)malloc(len + 1);
            sname[0] = 0;
            cname[0] = 0;
            char *cur = &start[stop_at - 5];
            if (sscanf(cur, "class %s : %s%n", cname, sname, &index) != 2) {
                cname[0] = 0;
                sname[0] = 0;
                if (sscanf(cur, "class %s%n", cname, &index) != 1)
                    cname[0] = 0;
            }
            if (*cname == 0) {
                fprintf(stderr, "expected class name\n");
                exit_code(1);
            }
            char *token = (char *)malloc(len + 1);
            cur = &cur[index];
            for (cur; isspace(*cur); ++cur) { }
            if (*cur != '{') {
                fprintf(stderr, "expected { char to start class declaration block; found '%c'\n", *cur);
                exit_code(1);
            }

            // read entire block
            int expr_alloc = len + 1;
            int expr_count = 0;
            char **expr = (char **)malloc(expr_alloc * sizeof(char *));
            memset(expr, 0, expr_alloc * sizeof(char *));
            char *block = read_cblock(cur, &block_len, expr, &expr_count, ';', false);
            block_len += (int)(size_t)(cur - start);
            if (!block) {
                fprintf(stderr, "invalid class declaration block\n");
                exit_code(1);
            }
            fprintf(fout, "#define _%s(D,T,C) _%s(spr,T,C) \\\n",
                        cname, sname != 0 ? sname : (char *)"Base");

            for (int e = 0; e < expr_count; e++) {
                cur = expr[e];
                for (cur; isspace(*cur); ++cur) { }
                char *meta = NULL;
                if (*cur == '[') {
                    bool bracket = false;
                    char *hash = copy_to(&cur[1], "]", &bracket, NULL);
                    if (!bracket) {
                        fprintf(stderr, "expected end bracket start\n");
                        exit_code(1);
                    }
                    char *hash_delimit = delimit_quotes(hash);
                    cur += strlen(hash) + 2;
                    for (cur; isspace(*cur); ++cur) { }
                    meta = (char *)malloc(strlen(hash_delimit) + 32);
                    sprintf(meta, ",\"%s\"", hash_delimit);
                    free(hash_delimit);
                    free(hash);
                }
                if (sscanf(cur, "%s%n", token, &index) == 1) {
                    cur = &cur[index];
                    bool override = false;
                    bool delegate = strcmp(token, "delegate") == 0;
                    if (strcmp(token, "override") == 0 || delegate) {
                        override = !delegate;
                        if (sscanf(cur, "%s%n", token, &index) != 1) {
                            fprintf(stderr, "expected type after %s\n", token);
                            exit_code(1);
                        }
                        cur += index;
                    }
                    for (cur; isspace(*cur); ++cur) { }
                    bool parens = false;
                    char *type = copy_to(token, NULL, NULL, NULL);
                    char *name = copy_to(cur, "(", &parens, NULL);
                    if (!parens) {
                        free(name);
                        name = copy_to(cur, ";", NULL, NULL);
                    }
                    int name_len = name ? strlen(name) : 0;
                    if (delegate && parens) {
                        fprintf(stderr, "unexpected parens in delegate declaration\n");
                        exit_code(1);
                    }
                    if (override && !parens) {
                        fprintf(stderr, "expected parens in override declaration\n");
                        exit_code(1);
                    }
                    cur += name_len;
                    bool has_semi = false;
                    char *trailing = copy_to(cur, ";", &has_semi, NULL);
                    if (!has_semi) {
                        fprintf(stderr, "expected semi-colon\n");
                        exit_code(1);
                    }
                    bool is_method = parens;
                    char *args = NULL;
                    if (is_method) {
                        int n_open = 0, n_close = 0;
                        args = trailing;
                        parens_depth(trailing, &n_open, &n_close, &trailing);
                        if (n_close == 0) {
                            fprintf(stderr, "expected parenthesis close )\n");
                            exit_code(1);
                        } else if (n_open != n_close) {
                            fprintf(stderr, "parenthesis mismatch\n");
                            exit_code(1);
                        }
                        trailing++;
                    }
                    for (trailing; isspace(*trailing); ++trailing) { }
                    if (*trailing != 0) {
                        fprintf(stderr, "unexpected character '%c'\n", *trailing);
                        exit_code(1);
                    }
                    if (args) {
                        fprintf(fout, "\t%s(D,T,C,%s,%s,%s%s) %s\n",
                            (override ? "override" : "method"), type, name, args, meta ? meta : "", e < (expr_count - 1) ? "\\" : "");
                    } else {
                        fprintf(fout, "\t%s(D,T,C,%s,%s%s) %s\n",
                            (delegate ? "object" : "var"), type, name, meta ? meta : "", e < (expr_count - 1) ? "\\" : "");
                    }
                    free(type);
                    free(name);
                }
                free(meta);
            }
            free(block);
            free(expr);
            free(token);
            fprintf(fout, "declare(%s,%s)", cname, *sname != 0 ? sname : (char *)"Base");
            free(cname);
            free(sname);
        } else if (!new_line && token_read && strcmp(buf, "enum") == 0) {
            int len = strlen(start);
            int index = 0;
            char *ename = (char *)malloc(len + 1);
            ename[0] = 0;
            char *cur = &start[stop_at - 4];
            if (sscanf(cur, "enum %s%n", ename, &index) != 1) {
                fprintf(stderr, "expected enum name\n");
                exit_code(1);
            }
            for (cur = &cur[index]; isspace(*cur); ++cur) { }
            if (*cur != '{') {
                fprintf(stderr, "expected { char to start enum block; found '%c'\n", *cur);
                exit_code(1);
            }
            // read entire block
            int expr_alloc = len + 1;
            int expr_count = 0;
            char **expr = (char **)malloc(expr_alloc * sizeof(char *));
            memset(expr, 0, expr_alloc * sizeof(char *));
            char *block = read_cblock(cur, &block_len, expr, &expr_count, ',', true);
            block_len += (int)(size_t)(cur - start);
            if (!block) {
                fprintf(stderr, "invalid enum declaration block\n");
                exit_code(1);
            }
            fprintf(fout, "#define _%s(D,T,C) _Enum(spr,T,C) \\\n", ename);
            char *iname = (char *)malloc(len + 1);
            iname[0] = 0;
            int val = 0;
            bool first = false;
            bool allow_number = true;
            int enum_cursor = 0;
            int enum_inc = 1;
            bool comma_given = true;
            int enum_first = 0;
            int enum_second = 1;
            int num_specified_count = 0;

            for (int e = 0; e < expr_count; e++) {
                bool num_specified = false;
                cur = expr[e];
                for (cur; isspace(*cur); ++cur) { }
                if (!comma_given) {
                    fprintf(stderr, "comma expected in enum\n");
                    exit_code(1);
                }
                iname[0] = 0;
                val = 0;
                if (sscanf(cur, "%[^=]=%d,%n", iname, &val, &index) != 2) {
                    if (sscanf(cur, "%[^=]=%d%n", iname, &val, &index) != 2) {
                        if (sscanf(cur, "%[^,],%n", iname, &index) != 1) {
                            if (sscanf(cur, "%s%n", iname, &index) != 1) {
                                fprintf(stderr, "expected enum definition\n");
                                exit_code(1);
                            } else {
                                comma_given = false;
                                if (first)
                                    allow_number = false;
                            }
                        } else {
                            comma_given = true;
                            if (first)
                                allow_number = false;
                        }
                    } else {
                        num_specified = true;
                    }
                } else {
                    num_specified = true;
                    comma_given = true;
                }
                sscanf(iname,"%s",iname);
                if (num_specified && !allow_number) {
                    fprintf(stderr, "number must be specified on first enum item\n");
                    exit_code(1);
                } else if (num_specified) {
                    ++num_specified_count;
                    if (num_specified_count == 1) {
                        enum_first = val;
                    } else if (num_specified_count == 2) {
                        // infer auto-step
                        enum_second = val;
                        enum_inc = enum_second - enum_first;
                        if (enum_inc == 0) {
                            fprintf(stderr, "duplicate enum\n");
                            exit_code(1);
                        }
                    }
                    enum_cursor = val;
                }
                fprintf(fout, "\tenum_object(D,T,C,%s,%d) %s\n",
                    iname, enum_cursor, e < (expr_count - 1) ? "\\" : "");
                enum_cursor += enum_inc;
            }
            fprintf(fout, "enum_declare(%s, Enum)", ename);
        } else {
            fprintf(fout, "%c", c);
        }
    }
    free(in);
    free(buf);

    fprintf(fout, "\n\n#endif\n");

exit_with:

    fclose(fout);
    fclose(fin);
    exit(code);
}