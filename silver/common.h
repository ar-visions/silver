#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

typedef   int8_t i8;
typedef  uint8_t u8;
typedef  int16_t i16;
typedef uint16_t u16;
typedef  int32_t i32;
typedef uint32_t u32;
typedef  int64_t i64;
typedef uint64_t u64;

typedef  float r32;
typedef double r64;
typedef double real;

// makes sense to write silver in C99 -- the most basic and direct approach
#define null ((void*)0)

typedef struct _field_t {
    struct _field_t* next;
    struct _field_t* prev;
    u64   hash;
    char* key;
    void* value;
} field_t;

typedef struct {
    int count;
    int element_size;
    field_t* first;
    field_t* last;
} list_t;

typedef struct {
    int count;
    int size;
    int element_size;
    unsigned char* elements;
} array_t;

typedef struct {
    char*    name;
    int      len;
} token_t; 

typedef struct {
    array_t*  tokens;
    int       line_num;
} ident_t;

// crossing the streams a bit here.  this is module-bound membership types, irrespective if they are inside of a mod or outside.
// technically its better since it allows us to implement more to this degree
enum member_type {
    undefined,
    mod,
    enumerable,
    ctr,
    method,
    prop
};

enum model_type {
    model_ref,
    model_i8,
    model_u8,
    model_i16,
    model_u16,
    model_i32,
    model_u32,
    model_i64,
    model_u64,
    model_bool,
    model_r32,
    model_r64,
};

typedef struct {
    enum member_type type;
    bool            intern;
    bool            inlay;
    char*           name;
    list_t          members;
} module_member_t;

typedef struct _type_t {
    struct _type_t* map_key; 
    struct _type_t* map_value; 
    struct _type_t* array_of; 
    struct _module_mod* mod;
} type_t;

typedef struct {
    module_member_t info;
    ident_t*        type; /// when parsing these, we should be able to resolve the types
    array_t*        initializer;
} module_data_t;

typedef struct _module_t {
    module_member_t info;
    char*           abs_path;
    array_t*        tokens;
    array_t*        imports;
    array_t*        mods;
    array_t*        enums;
    array_t*        structs;
    array_t*        data;
} module_t;

typedef struct {
    list_t*         fields;
    array_t*        hash_list;
    int             hash_size;
} map_t;

typedef struct {
    module_member_t info;
    char*           source; // url
    char*           shell;  // CMakeLists.txt <-
    array_t*        links;    // char*
    array_t*        includes; // char*
    map_t*          defines;  // char* -> object_t*
    char*           alias;
    module_t*       instance;
} import_t;

typedef struct {
    module_member_t info;
} proto_t;

typedef struct _enum_t {
    module_member_t info;
    int             default_value;
} enum_t;

typedef struct _struct_t {
    module_member_t info;
} struct_t;

typedef struct _mod_t {
    module_member_t info;
    array_t*        imports;
    array_t*        conforms;
    char*           inherits;
    struct _mod_t*  parent;
    enum model_type model; /// only a root mod can have a model set
} mod_t;

typedef struct {
    char*           name;
    type_t*         type;
    array_t*        initializer;
} arg_t;

typedef struct {
    module_member_t info;
    type_t*         rtype;
    array_t*        args;
} method_t;

typedef struct {
    module_member_t info;
    type_t*         prop_type;
} prop_t;

typedef struct {
    array_t*        tokens;
    int             current;
} parser_t;

#define FNV_PRIME    0x100000001b3
#define OFFSET_BASIS 0xcbf29ce484222325

void*     list_push(list_t* list);
field_t*  list_push_element(list_t* list, void* value);

char**    split(char *string, char *seperators, int *count);
array_t*  array_with_sizes(int size, int element_size);
void*     array_at(array_t* a, int index);
void*     array_push(array_t* a);
void      array_push_element(array_t* a, void* e);
int       array_index_of(array_t* a, void* e);
map_t*    map_with_size(int size);
field_t*  map_lookup(map_t* map, char* key);
field_t*  map_fetch(map_t* map, char* key);
void*     map_get(map_t* map, char* key);
void*     map_set(map_t* map, char* key, void* value);
char*     copy_string(char *start, int len);
void      token_set(token_t* ident, char* name, int line_num);
void      parse_token(token_t* result, char *start, int len, int line_num);
void      ws(char **cur);
char*     contents(char* file);
array_t*  tokenize(char* file);
bool      is_abs(char *path);
char*     resolve_path(char* path);
parser_t* parser_with_tokens(array_t* tokens);
token_t*  parser_expect_next(parser_t* parser, char* name, char* assertion);
token_t*  parser_next(parser_t* parser);
token_t*  parser_pop(parser_t* parser);
int       token_compare(token_t* ident, char* b);