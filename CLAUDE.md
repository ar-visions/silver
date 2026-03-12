# Silver

Silver is a systems programming language with an LLVM backend. It compiles `.ag` source files into native binaries via LLVM IR. The compiler itself is written in C, built on the **Au** object system.

## Build & Run

```bash
# Build the silver compiler (from /src/silver)
make                    # builds release
make debug              # builds debug (with -g -O0)
make clean              # cleans generated headers

# Compile a .ag program
./sdk/native/bin/silver foundry/ai-test/ai-test.ag

# Run the output binary
./sdk/native/release/ai-test
```

- `make` runs `bootstrap.sh` then `ninja`. Bootstrap generates `target.cmake` and a `.ninja` build file.
- The compiler binary is at `sdk/native/bin/silver`.
- Compiled outputs (`.ll`, `.bc`, `.o`, final binary) go to `sdk/native/release/`.
- The bootstrap uses the SDK's own clang (`sdk/native/bin/clang`) built from a pinned LLVM commit.

## Project Structure

```
src/
  Au              # Au object system header (types, macros, memory, declare_class)
  Au.c            # Au runtime implementation (object lifecycle, type registration, collections)
  Au.g            # Au build descriptor (shared lib, links libffi)
  aether          # Aether header (enode, etype, aether schemas — the IR/AST layer)
  aether.c        # Aether implementation (LLVM codegen, type building, expression nodes)
  aether.g        # Aether build descriptor (shared lib, links LLVM/clang/lldb)
  silver          # Silver compiler header (silver_schema, import_schema, codegen classes)
  silver.c        # Silver compiler parser (tokenizer, expression parser, statement parser)
  silver.g        # Silver build descriptor (app, modules: Au net aether)
  macros.h        # C macros for declare_class, schema definitions
  object.h        # Low-level object header/vtable layout
foundry/          # Silver application projects (each has a .ag file)
  ai/ai.ag        # Neural network library (tensors, ops, keras model, training)
  ai-test/        # AI test application
  test/test.ag    # General language test
  random/         # Random number generation module
  orbiter/        # Orbiter project
  ...
sdk/native/       # Built SDK (bin/, lib/, include/)
checkout/         # Vendored dependencies (llvm-project, mbedtls, etc.)
```

## .g Build Descriptors

Each module has a `.g` file defining its build:
```
type:       app | shared
modules:    <dependency modules>
link:       <linker flags>
import:     <external dependency with git ref>
install:    <headers to install>
```

## Au Object System

Au is the C-based object/type system underlying everything. Key concepts:

- **Au_t** (`struct _Au_t*`): Type descriptor. Holds members, methods, vtable, size, traits.
- **Au** (`struct _Au**`): Object reference (double-pointer, header before data).
- **`typeid(T)`**: Gets the Au_t for type T (e.g., `typeid(i32)`, `typeid(string)`).
- **`declare_class(Name)`**: Declares a class with schema. Variants: `declare_class_2(Name, Base)`, `declare_class_3`, `declare_class_4` for inheritance depth.
- **Schema macros**: `#define foo_schema(X, Y, ...) M(X, Y, i, prop, public, type, name) ...` — defines members, methods, overrides, constructors.
- **Member types**: `prop` (field), `method`, `override`, `ctr` (constructor), `vargs`, `guard`.
- **Access**: `public`, `intern`, `iobject`.
- **Traits**: `AU_TRAIT_CONST`, `AU_TRAIT_INLAY`, `AU_TRAIT_STRUCT`, `AU_TRAIT_PRIMITIVE`, `AU_TRAIT_ENUM`.

### Key Au Types
- Primitives: `i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bf16 bool num sz`
- `string` — managed string object
- `symbol` — `const char*` (use `cstring(s)` to convert string → symbol for functions like `lexical()`)
- `array` — heap-allocated class-based collection (inherits `collective`)
- `map` — hash map collection
- `path` — file path object
- `token` — lexer token (has `chars`, `line`, `indent`, `literal`)
- `enode` — expression/AST node
- `etype` — type reference in the compiler (wraps Au_t with LLVM metadata)
- `shape` — dimensional shape (e.g., `32x32x1`, `4x4`)

### Key Au Functions
- `hold(x)` / `drop(x)` — reference counting
- `len(collection)` — element count
- `push(array, element)` — append
- `get(map, key)` / `set(map, key, value)` — map access
- `eq(a, b)` — equality check (works on strings, tokens)
- `instanceof(obj, type)` — type check, returns cast or null
- `inherits(au_t, typeid)` — inheritance check
- `find_member(au_t, name, member_type, flags, search_inherited)` — member lookup

## Silver Language (.ag Syntax)

### Module Structure
```
export 0.8.8              # version export

import <stdio.h>          # C header import
import ai                 # silver module import
import random             # silver module import

alias astrings: array string   # type alias
```

### Types & Variables
```
x: i32 [ 42 ]             # typed variable with initializer in [ ]
name: string [ 'hello' ]  # string with single quotes
v: f32 [ 0.0 ]            # float
flag: bool [ true ]        # boolean
p: ref i32                 # pointer (reference)
data: new f32 [ 1024 ]    # heap allocation (new Type [ shape ])
```

**Important**: `new Type [ expr ]` parses `expr` as a shape. `*` inside `[]` is shape literal. Pre-compute products as i32 before passing to new:
```
sz: i32 [ a * b ]          # compute product first
buf: new f32 [ sz ]        # then allocate
```

### Enums
```
enum Optimizer
    sgd:  0
    adam: 1
```
Access: `Optimizer.sgd` or bare `sgd` when type is inferred (e.g., in switch cases).

### Classes & Structs
```
class op
    public name: string
    public quantized: bool

    func forward [] -> none
        puts 'forward'
```
- `class` = heap-allocated, reference-counted
- `struct` = inlay/value type
- Members: `public`, `intern` (private), `context` (read-only after init)
- Inheritance: `class dense [ op ]` — dense inherits from op

### Functions
```
func name [ arg1: i32, arg2: string ] -> return_type
    body
```
- Indentation-based blocks (tabs)
- No `[ ]` needed for zero-arg calls at expression level 0
- `[ ]` required for expressions and when args are present

### Control Flow
```
if [ condition ]
    body
else
    body

for [ i: i32 0, i < n, i += 1 ]
    body

switch [ expr ]
    case value1
        body
    case value2, value3
        body
    default
        body
```
- Switch cases infer enum type from the switch expression (bare enum member names work)

### Inline Assembly
```
asm x86_64                  # conditional: only compiles on x86_64
    mov rax, 1
    ...

result: i32 asm [ args ]    # expression-level asm with return value
    mov eax, ...

asm [ args ]                # void/statement-level asm
    ...
```
- Intel syntax
- `asm <define>` on same line = conditional compilation (skips block if define is false)
- Auto-gather: when no `[args]` given, asm scans body for in-scope variables
- Platform defines: `x86_64`, `arm64`, `mac`, `linux`, `windows`

### String Interpolation
```
puts 'hello {name}, value is {x}'
```

### Collections
```
items: array i32 [ 1, 2, 3 ]
data: map string [ i32 ]
```

### Iteration
```
each(items, type, var)
    use var
```

## Compiler Architecture (silver.c)

### Key Parse Functions
- `silver_parse()` — entry point, sets up platform defines, loops `parse_statement()`
- `parse_statement()` — dispatches keywords (if/for/switch/return/class/func/etc.)
- `parse_expression()` → `reverse_descent()` → `read_enode()` — expression parsing with precedence climbing
- `read_enode()` — reads a single expression node (literals, variables, new, sizeof, etc.)
- `parse_member()` — resolves member access chains (`a.b.c`), handles scope_mdl hint for enum inference
- `read_etype()` — reads a type name, handles generics, refs, primitives, C types
- `read_expression()` — tokenizing wrapper around `parse_expression`, used in switch/case for deferred build
- `parse_switch()` — switch statement parser, passes enum hint via `canonical(e_expr)` to case value parsing
- `parse_asm()` — inline assembly parser, supports conditional `asm <define>` and auto-gather

### Key Aether Functions (aether.c)
- `e_operand()` — create literal/constant enode
- `e_create()` — type conversion/construction
- `e_op()` — binary operation node
- `e_load()` — load value from memory
- `e_null()` — null value of type
- `e_vector()` — heap array allocation
- `canonical(enode)` — resolve enode to its underlying etype (follows vars → source type)
- `is_enum(Au)` — checks if type is enum (uses `au_arg_type` to resolve through vars)

### Token Navigation
- `peek(a)` — look at next token without consuming
- `consume(a)` — consume next token
- `read_if(a, "keyword")` — consume if matches, return token or null
- `next_is(a, "token")` — check without consuming
- `element(a, -1)` — get previously consumed token
- `read_alpha(a)` — read an alphanumeric identifier
- `peek_alpha(a)` — peek at next alpha without consuming
- `push_current(a)` / `pop_tokens(a, keep)` — save/restore cursor for speculative parsing
- `read_body(a)` — read an indented block as token array

### Lexical Scoping
- `lexical(a->lexical, symbol)` — look up identifier in scope stack (takes `symbol`/`char*`, NOT `string`)
- `top_scope(a)` — current scope Au_t
- `context_func(a)` — enclosing function
- `context_class(a)` / `context_record(a)` — enclosing class/record
- `elookup(chars)` — look up identifier globally

## Common Patterns

### Adding a platform define
In `silver_parse()`:
```c
Au_t m = def_member(a->au, "name", typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
set(a->registry, (Au)m, (Au)hold(e_operand(a, _bool(condition), etypeid(bool))));
```

### Working with enode types
```c
etype t = canonical(some_enode);    // get resolved type
bool  e = is_enum((Au)some_enode);  // check if type is enum (resolves through vars)
bool  p = is_prim((Au)some_enode);  // check if primitive
```

## Testing

No formal test runner. Test by compiling and running foundry projects:
```bash
make && ./sdk/native/bin/silver foundry/ai-test/ai-test.ag && ./sdk/native/release/ai-test
```
