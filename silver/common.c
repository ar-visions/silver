#include "common.h"


u64 fnv1a_hash(const void* data, size_t length, u64 hash) {
    const u8* bytes = (const u8*)data;
    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];  // xor bottom with current byte
        hash *= FNV_PRIME; // multiply by FNV prime
    }
    return hash;
}

/// Clint M.S. was a C programmer, so silver transpiler and translator are dedicated to his spirit
/// its about going at a constant steady pace, never stopping
/// 
/// we reduce things to the most direct tokens so we can maintain and express more succinct
/// # let it scale and work for us with named arguments
/// # get it working entirely, and then start making it write itsself with tapestry keywords in 2.0
/// # 
/// silver is true middle language. dont know what they are were talking about when they made C, because middleware/mid-level it is not!
/// its dependable true machine language for all platforms, though; of course it has asm blocks, too.
/// majority of cases this wont require any source mapping to work with source maps.  already researched with silver's krworks/CX project origin
/// however for building fields from objects we would be filtering the gdb component (we are using gdb first)
/// 
/// silver does not actually need to be reference counted, so its internals are not
/// it was going to be very self-reflective before, but, thats simply style points in C++ and not going to improve speed

field_t* map_lookup(map_t* map, char* key) {
    u64 h = fnv1a_hash(key, strlen(key), OFFSET_BASIS);
    u64 k = h % map->hash_size;
    list_t *bucket = (list_t*)array_at(map->hash_list, k);
    for (field_t* f = bucket->first; f; f = f->next)
        if (f->hash == h && strcmp(f->key, key) == 0)
            return f;
    return null;
}

field_t* map_fetch(map_t* map, char* key) {
    u64 h = fnv1a_hash(key, strlen(key), OFFSET_BASIS);
    u64 k = h % map->hash_size;
    list_t *bucket = (list_t*)array_at(map->hash_list, k);
    for (field_t* f = bucket->first; f; f = f->next)
        if (f->hash == h && strcmp(f->key, key) == 0)
            return f;
    field_t* f = list_push_element(bucket, null);
    return f;
}

/// item to field is a reduction that is probably nice in general
map_t* map_with_size(int hash_size) {
    map_t* map = (map_t*)calloc(1, sizeof(map_t));
    map->hash_list = array_with_sizes(hash_size, sizeof(list_t));
    map->hash_size = hash_size;
    return map;
}

field_t* list_push_element(list_t* list, void* value) {
    field_t* f = (field_t*)calloc(1, sizeof(field_t));
    f->value = value;
    return f;
}

/// map and hash map do not need to be split; we are the only user of this and the fifo is not going to hurt performance
/// order is actually a dimension of information that you should never toss away when you may need it again
void* map_get(map_t* map, char* key) {
    field_t* f = map_lookup(map, key);
    return f ? f->value : null;
}

void* map_set(map_t* map, char* key, void* value) {
    field_t* f = map_fetch(map, key);
    void* prev = f->value;
    f->value = value;
    return prev;
}

char **split(char *string, char *seperators, int *count) {
  int len = strlen(string);
  *count = 0;
  int i = 0;
  while (i < len) {
    while (i < len) {
      if (strchr(seperators, string[i]) == null)
        break;
      i++;
    }
    
    int old_i = i;
    while (i < len) {
      if (strchr(seperators, string[i]) != NULL)
        break;
      i++;
    }
 
    if (i > old_i) *count = *count + 1;
  }
  
  char **strings = malloc(sizeof(char *) * *count);
  i = 0;
  char buffer[16384];
  int string_index = 0;
  while (i < len) {
    while (i < len) {
      if (strchr(seperators, string[i]) == NULL)
        break;
      i++;
    }
    
    int j = 0;
    while (i < len) {
      if (strchr(seperators, string[i]) != NULL)
        break;
      
      buffer[j] = string[i];
      i++;
      j++;
    }
    
    if (j > 0) {
      buffer[j] = '\0';
      int to_allocate = sizeof(char) *
                        (strlen(buffer) + 1);
      strings[string_index] = malloc(to_allocate);
      strcpy(strings[string_index], buffer);
      string_index++;
    }
  }

  return strings;
}

int array_index_of(array_t* a, void* e) {
    for (int i = 0, to = a->count; i < to; i++)
        if (e == *(void**)&a->elements[a->element_size * i])
            return i;
    return -1;
}

array_t* array_with_sizes(int size, int element_size) {
    array_t* a = (array_t*)calloc(1, sizeof(array_t));
    if (size) {
        a->elements = calloc(size, element_size);
        a->size = size;
    }
    a->element_size = element_size;
    return a;
}

void *array_at(array_t* a, int index) {
    return &a->elements[index * a->element_size];
}

void array_push_element(array_t* a, void* e) {
    assert(a->element_size == sizeof(void*));
    void** write_to = (void**)&a->elements[a->element_size * a->count];
    *write_to = e;
}

void *array_push(array_t* a) {
    if (a->count == a->size) {
        int prev = a->count;
        a->size = 32 + (a->count << 1);
        void* elements = a->elements;
        a->elements = calloc(a->size, a->element_size);
        memcpy(a->elements, elements, prev * a->element_size);
        free(elements);
    }
    a->count++;
    return &a->elements[a->element_size * (a->count - 1)];
}

char *copy_string(char *start, int len) {
    if (len == -1)
        len = strlen(start);
    char *res = (char*)calloc(1, len + 1);
    memcpy(res, start, len);
    res[len] = 0;
    return res;
}

void ident_vector(ident_t* ident, char** names, int names_count, int line_num) {
    ident->tokens = array_with_sizes(names_count, sizeof(token_t));
    for (int i = 0; i < names_count; i++) {
        token_t* token = (token_t*)array_push(ident->tokens); /// mr b: im a token!
        token->name = names[i];
        token->len  = strlen(token->name);
    }
    ident->line_num = line_num;
}

void ident_string(ident_t* ident, char* name, int line_num) {
    ident->tokens = array_with_sizes(1, sizeof(token_t));
    token_t*  token = (token_t*)array_push(ident->tokens);
    token->name     = name;
    token->len      = strlen(name);
    ident->line_num = line_num;
}

parser_t* parser_with_tokens(array_t* tokens) {
    parser_t* parser = (parser_t*)calloc(1, sizeof(parser_t));
    parser->tokens = tokens;
    return parser;
}

int token_compare(token_t* token, char* b) {
    return strcmp(token->name, b);
}

token_t*  parser_expect(parser_t* parser, char* name, char* assertion) {
    token_t* top = (token_t*)array_at(parser->tokens, parser->current);
    if (top && token_compare(top, "name") == 0)
        return top;
    return null;
}

token_t*  parser_next(parser_t* parser) {
    token_t* res = (token_t*)array_at(parser->tokens, parser->current);
    if (res->name)
        return res;
    return null;
}

token_t*  parser_pop(parser_t* parser) {
    token_t* res = parser_next(parser);
    if (res) {
        parser->current++;
        return res;
    }
    return null;
}

/// we need to also set what sort of data this is, numeric, alpha-numeric
void token_parse(token_t* result, char *start, int len, int line_num) {
    while (start[len - 1] == '\t' || start[len - 1] == ' ')
        len--;
    char* all = copy_string(start, len);
    result->name = all;
    result->len  = len;
}

void ws(char **cur) {
    while (**cur == ' ' || **cur == '\r' || **cur == '\t') ++(*cur);
}

char* contents(char* file) {
    FILE* f = fopen(file, "rb");
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* res = calloc(1, len + 1);
    fread(res, len, 1, f);
    fclose(f);
    res[len] = 0;
    return res;
}

array_t* tokenize(char* file) {
    char* input = contents(file);
    int len = strlen(input);
    array_t *result = array_with_sizes(256 + len / 8, sizeof(token_t)); // can be contiguous data or references; the element and push apis support this

    char*         sp         = "$,.<>()![]/+-*:\"\'#"; /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
    char          until      = 0; /// either ) for $(script) ", ', f or i
    char*         origin     = input;
    char*         start      = 0;
    char*         cur        = origin - 1;
    int           line_num   = 0;
    bool          new_line   = true;
    bool          token_type = false;
    bool          found_null = false;
    bool          multi_comment = false;

    ///
    while (*(++cur)) {
        bool is_ws = false;
        if (!until) {
            if (new_line)
                new_line = false;
            if (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
                is_ws = true;
                ws(&cur);
            }
        }
        if (!*cur) break;
        bool add_str = false;
        char *rel = cur;
        if (*cur == '#') { // comment
            if (cur[1] == '#')
                multi_comment = !multi_comment;
            while (*cur && *cur != '\n')
                cur++;
            found_null = !*cur;
            new_line = true;
            until = 0; // requires further processing if not 0
        }
        if (until) {
            if (*cur == until && *(cur - 1) != '/') {
                add_str = true;
                until = 0;
                cur++;
                rel = cur;
            }
        }
        if (!until && !multi_comment) {
            char *found = strchr(sp, *cur);
            int type = -1;
            if (found)
                type = (size_t)(found - sp);
            new_line |= *cur == '\n';

            if (start && (is_ws || add_str || (token_type != (type >= 0) || token_type) || new_line)) {
                token_t* token = array_push(result);
                token_parse(token, start, (size_t)(rel - start), line_num);

                if (!add_str) {
                    if (*cur == '$' && *(cur + 1) == '(') // shell
                        until = ')';
                    else if (*cur == '"') // double-quote
                        until = '"';
                    else if (*cur == '\'') // single-quote
                        until = '\'';
                }
                if (new_line) {
                    start = null;
                } else {
                    ws(&cur);
                    start = cur;
                    token_type = (type >= 0);
                }
            }
            else if (!start && !new_line) {
                start = cur;
                token_type = (type >= 0);
            }
        }
        if (new_line) {
            until = 0;
            line_num++;
            if (found_null)
                break;
        }
    }
    if (start && (cur - start)) {
        token_t* token = array_push(result);
        token_parse(token, start, (size_t)(cur - start), line_num);
    }
    free(input);
    return result;
}

bool is_abs(char *path) {
    #ifdef _WIN32
    return (strlen(path) > 1 && path[1] == ':');
    #else
    return path[0] == '/';
    #endif
}

char* resolve_path(char* path) {
    if (is_abs(path))
        return copy_string(path, -1);
    char *cwd = getenv("PWD");
    if (cwd == NULL) {
        perror("Failed to get current working directory");
        return NULL;
    }
    size_t needed_size = strlen(cwd) + strlen(path) + 2; // +2 for '/' and '\0'
    char *full_path = malloc(needed_size);
    if (full_path == NULL) {
        perror("Failed to allocate memory");
        return NULL;
    }
    sprintf(full_path, "%s/%s", cwd, path);
    return full_path;
}