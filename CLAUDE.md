# Silver

## Rule #1 — Never question the build state

Do NOT suggest `make clean`, doubt that a rebuild happened, or ask "did silver rebuild." The user's build system works. If a change doesn't appear to take effect, the bug is in the code, not the cache. Trust the user's runtime output as reflecting the current code state, always.
running silver with --clean will always rebuild the module

---

Silver is a systems programming language with an LLVM backend. It compiles `.ag` source files into native binaries via LLVM IR. The compiler itself is written in C, built on the **Au** object system.

## Build & Run

```bash
# Build the silver compiler (from /src/silver)
make                    # builds debug (default)
make release            # builds release with -O2
make clean              # cleans generated headers

# Compile a .ag program (foundry/ prefix is optional — it's searched first)
./platform/native/debug/silver trinity

# With options (module path first, then flags)
silver trinity --watch    # file watcher mode
silver trinity --clean    # force rebuild all imports
silver trinity --release  # release build

# Primary development workflow — always use -v --clean --run together
silver orbiter -v --clean --run
```

- `make` defaults to debug. Debug binary goes to `platform/native/debug/silver`. Release binary goes to `platform/native/bin/silver`.
- Bootstrap runs `gen.py` then `ninja`. The ninja file is generated per build type.
- Build caching: modules with unchanged source skip recompilation (checks `.product` timestamp vs `.ag` timestamp).
- Release builds: LLVM emits .o directly in-memory via `LLVMTargetMachineEmitToFile` — no .ll file, no llc process. Uses `LLVMCodeGenLevelAggressive` with `+avx2,+fma` on x86-64.
- Debug builds: emits .ll to disk (for inspection), uses llc with `-O0`, full LLDB debug info.
- `.ll` and `.bc` files only written when `--verbose` is set.

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
platform/native/  # Built SDK (bin/, lib/, include/)
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
- **Commaless args**: without commas, args are matched to parameters by type (CSS selector style). With commas, args are positional. This enables declarative UI composition where order is flexible.
- **Callable subs**: `x: i32 sub` stores the body. `x[]` re-invokes it with current scope, returns the value without reassigning `x`.
- **Keyword tokens**: `{ l0 t0 r0 b0 }` parses as a `tokens` literal when expected type is `tokens`. Used for layout coordinates and style properties.

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

### Compilation Phases

`silver_parse()` drives compilation in distinct phases:

1. **Statement loop** — parses module-level statements sequentially: imports, exports, aliases, class/struct/enum headers (bodies stashed as token arrays), free functions. `incremental_resolve()` called after each statement (currently a stub).
2. **Deferred alias resolution** — aliases whose target types weren't available during phase 1 (forward references) are resolved in multi-pass order. Stored as `(Au_t, token array)` pairs in `a->pending_aliases`.
3. **Phase 1: Record body parse** — `build_record_parse()` replays stashed tokens for each class/struct via `push_tokens`/`pop_tokens`, parsing members and method signatures.
4. **Phase 2: LLVM type implementation** — `build_record_implement()` calls `etype_implement()` to build LLVM struct bodies, set member indices, create type IDs.
5. **Phase 3: Function codegen** — `build_record_functions()` and `build_fn()` emit LLVM IR for all method and free function bodies.

This phased approach means: all type names are registered before any body is parsed; all LLVM types exist before any function emits IR.

### Key Parse Functions
- `silver_parse()` — entry point, sets up platform defines, runs all phases
- `parse_statement()` — dispatches keywords (if/for/switch/return/class/func/etc.)
- `parse_expression()` → `reverse_descent()` → `read_enode()` — expression parsing with precedence climbing
- `read_enode()` — reads a single expression node (literals, variables, new, sizeof, etc.)
- `silver_parse_member()` — resolves member access chains (`a.b.c`), handles scope_mdl hint for enum inference
- `read_etype()` — reads a type name, handles generics, refs, primitives, C types. Uses `read_named_model()` → `elookup()` → `rlookup()` → `lexical()` → `etype_prep()` chain.
- `silver_read_def()` — parses type definitions: class, struct, enum, alias, scalar, import, export
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
- `etype_prep(a, au)` — find or create etype for an Au_t, calls `etype_create` + `etype_implement`
- `etype_create(a, au)` — create etype wrapper for Au_t, register in `a->registry`
- `etype_implement(t, force)` — build LLVM type body (struct fields, function signatures)
- `etype_register(a, key, value, overwrite)` — store etype in `a->registry` map
- `etype_access(target, name)` — member access: `find_member` → GEP at `m->index`
- `u(etype, au)` — macro: `get(a->registry, (Au)au)` cast to etype. Registry lookup, NOT a field access.

### Token Navigation
- `peek(a)` — look at next token without consuming
- `consume(a)` — consume next token
- `read_if(a, "keyword")` — consume if matches, return token or null
- `next_is(a, "token")` — check without consuming
- `element(a, N)` — token at cursor+N (0 = next unconsumed, -1 = last consumed)
- `read_alpha(a)` — read an alphanumeric identifier
- `peek_alpha(a)` — peek at next alpha without consuming
- `push_current(a)` / `pop_tokens(a, keep)` — save/restore cursor for speculative parsing. `push_current` is sugar for `push_tokens(a, a->tokens, a->cursor)`.
- `push_tokens(a, tokens, cursor)` / `pop_tokens(a, keep)` — switch to a different token stream entirely (used for replaying stashed class bodies, deferred aliases, inline expressions). Saves/restores full state on `a->stack`.
- `read_body(a)` — read an indented block as token array

### Lexical Scoping
- `lexical(a->lexical, symbol)` — look up identifier in scope stack (takes `symbol`/`char*`, NOT `string`). Walks `a->lexical` array from end to start; within each scope walks `context` chain (inheritance). Searches both `members` and `args`.
- `top_scope(a)` — current scope Au_t
- `context_func(a)` — enclosing function
- `context_class(a)` / `context_record(a)` — enclosing class/record
- `elookup(chars)` — macro: `(etype)rlookup((aether)a, string(chars))`
- `rlookup(a, name)` — `lexical()` + `etype_prep()`: finds Au_t by name, then prepares/returns its etype

### Au Object Inheritance in C

Silver (silver.c) inherits from aether (aether.c) which inherits from etype. The `silver*` pointer IS an `aether*` — schema fields from parent classes are accessible via `a->field` in child code. When casting `(aether)a` in silver.c, it references the same object. Schema properties added to `aether_schema` are visible in silver.c; properties on `silver_schema` are NOT visible in aether.c. Place shared flags on the lowest common ancestor.

### Map vs Array for Small Collections

Au `map` uses hash buckets; `pairs(map, i)` iterates via `i->key`/`i->value` linked list. For small ordered collections (< 20 items), prefer `array` with index arithmetic — `pairs` iteration on maps can miss entries when keys have non-trivial hash behavior. Use `array` + stride access (`origin[i*2]`, `origin[i*2+1]`) for paired data.

## Debugging

### Running the compiler under GDB
```bash
# Always use -v --clean when debugging compiler issues
VK_LAYER_PATH=/src/silver/install/share/vulkan/explicit_layer.d \
LD_LIBRARY_PATH=/src/silver/platform/native/lib:/src/silver/install/lib:/src/silver/install/debug:$LD_LIBRARY_PATH \
  gdb --args ./platform/native/debug/silver orbiter -v --clean --run
```

### Common crash patterns
- **Segfault in `etype_prep`/`etype_create`/`etype_init`**: usually an Au_t with incomplete setup (missing `src`, wrong `member_type`). Check `au->ident`, `au->src`, `au->member_type` in GDB.
- **"unknown identifier" errors**: the type/variable isn't in lexical scope at parse time. Check `a->lexical->count` and what scopes are active.
- **"expected member" errors**: token stream is in wrong position — a previous parse consumed too many or too few tokens. Check `push_current`/`pop_tokens` balance.
- **Double terminator LLVM errors**: a basic block already has a terminator (return/break/fault) — check `LLVMGetBasicBlockTerminator` before adding branch.

### `read_etype` silent fallbacks
`read_etype` can silently produce wrong types: when meta type resolution fails (e.g., `map element [string]` where `element` isn't defined yet), it falls back to `etypeid(Au)` for the meta parameter. The `deferred_hit` flag on `aether` detects when `etype_prep` encountered an unresolved alias dependency during a `read_etype` call — check this flag after `read_etype` returns to know if the result is trustworthy.

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

`expect func` declares a test — the compiler verifies it returns true:
```
expect func test [ vk: vk_context ] -> bool
    # ... test code ...
    return result
```

Test by compiling and running foundry projects:
```bash
make && ./platform/native/debug/silver foundry/ai-test/ai-test.ag
```

## Aether Codegen Best Practices

### e_assign Phase 6 Store Logic
The store path in `e_assign` (aether.c ~line 640) has three cases for the RHS (`res`):

1. **Struct alloca alias** (res is unloaded alloca of a struct): If `L` already has its own alloca, **copy** the struct data (load + store) instead of aliasing `L->value = res->value`. Aliasing orphans L's original alloca — GDB and subsequent loads will read uninitialized memory. Only alias when L has no storage yet (initial declaration).

2. **GEP from array/member access** (res is unloaded GEP): Load through the GEP before storing. Skipped for:
   - Fixed-size arrays (`elements > 0`): the GEP pointer IS the value (array-to-pointer decay)
   - Opaque handle types (ancestor struct with 0 members, e.g. VkPhysicalDevice_T): these are pointer handles, not real structs

3. **Everything else**: Direct store of `res->value`.

### etype Resolution Chain
When looking up an etype for codegen (`u(etype, au)`), the result may have `lltype = NULL` if the Au_t is a variable member rather than the type itself. Fallback chain:
```c
etype rt = u(etype, res->au);                          // try member's etype
if (!rt || !rt->lltype) rt = u(etype, au_arg_type(...)); // try resolved type
if (!rt || !rt->lltype) rt = etype_prep(a, ...);         // force create
```

### `is_struct` Semantics
`Au_is_struct` uses `au_arg_type` to resolve through aliases and variables to the underlying type. Key behaviors:
- Returns `false` for pointer types (`au->is_pointer`)
- Opaque Vulkan handles (e.g. VkPhysicalDevice → alias → ptr → opaque struct) return `false` because `au_arg_type` stops at the pointer
- Use `au_ancestor()` when you need to walk past pointers to the terminal type (e.g. for opaque checks)
- `is_struct` vs `au->is_struct`: the function resolves through aliases; the field checks the Au_t directly

### Alias-to-Pointer Types in etype_init
C typedef aliases to pointer types (e.g. `typedef VkPhysicalDevice_T* VkPhysicalDevice`) can fall into the named struct branch of `etype_init` because `is_rec()` resolves through to the underlying struct. When the alias src chain leads to a pointer (`au_arg_type` returns an Au_t with `is_pointer`), set `lltype = LLVMPointerTypeInContext(...)` instead of creating a named LLVM struct.

### `e_create` Same-Type Identity
`e_create` has an identity shortcut: `if (canonical(input) == canonical(mdl)) return input`. This avoids unnecessary allocas for same-type conversions. If disabled for structs, `e_create` builds a temp alloca that e_assign's alias path may hijack — causing the orphaned-alloca bug described above.

### `au_arg` vs `au_arg_type` vs `au_ancestor`
- **`au_arg(t)`**: If t is a variable (AU_MEMBER_VAR), returns `t->src`. Otherwise returns t. Does NOT resolve aliases.
- **`au_arg_type(t)`**: Like `au_arg`, but then walks the `src` chain through aliases. Stops at pointers and funcptrs.
- **`au_ancestor(au)`**: Walks `src` chain unconditionally to the terminal type. Goes through pointers. Stops at enums. Use for opaque type detection.

## Recent Compiler Discoveries

### Type Resolution
- **`au_ancestor(au)`**: walks `src` chain to the terminal type. Stops at enums. Used in `etype_access` for member lookup through aliases, pointers, and typedefs.
- **C type aliases** (e.g. `FT_Face` → `FT_FaceRec_*` → `FT_FaceRec_`): `au_ancestor` + `etype_prep` resolves the full chain even in `no_build` mode.
- **Au_t member access**: uses `Au_t_f` schema lltype via `au_t_etype->schema->lltype` for GEP. Byte-offset GEP (`getelementptr i8`) for Au_t fields avoids cross-module struct name conflicts.
- **Union member indices**: all set to 0 (unions have single-element LLVM body). The `index` counter still increments for the member count verify.
- **Opaque C types** (GLFWwindow, FILE): skipped in type-id registration (`continue` when typesize is 0) and in static variable implementation (`is_c && is_static` skip).

### Convertible Rules Added
- `Au_t → ARef` (type descriptor is a pointer)
- `class → ref u8 / ref i8` (object to byte pointer)
- `ref ptr → Au` (any pointer to generic Au)
- `enum → f32/f64` (enum to float, via `is_realistic` check)
- `struct → struct` same size (bitcast, both directions)
- `ref struct → struct` same size (pointer to same-size struct)

### Short-Circuit `||` / `&&`
- Preserves values when types are compatible (`a: x || fallback` returns the truthy value).
- Falls back to `bool` when types don't match (`convertible` check after parsing R).
- `expect` passes `etypeid(bool)` as expected type to `read_enode`.

### Switch Statement
- Case blocks check `LLVMGetBasicBlockTerminator` before adding merge branch — prevents double terminators after `fault`/`return`/`break`.
- `AU_MEMBER_ENUMV` (member_type=10) recognized alongside `is_enum` for constant resolution.

### Public Type Exposure
- Public members on classes error if the member's `src` is a C type without typesize (`is_c && !is_primitive && !is_struct && !is_enum && !typesize`).
- Public function args and return types checked the same way.
- Enforced at parse time so bad type data never reaches import.

### Element-wise Array Operations
- Primitive arrays (`elements > 0`, `src->is_primitive`) support `+ - * / %` with automatic loop emission.
- Type promotion via `determine_rtype` + `e_create` per element.
- Scalar broadcast: one array + one scalar works in either order.
- `min`/`max`/`clamp` vectorized via `vector_binary_op` helper with function pointer dispatch.

### Math Builtins
- Single-arg: `sqrt sin cos tan asin acos atan exp log floor ceil round` — LLVM intrinsics or libm fallback.
- Two-arg: `atan2 pow` via `e_math2` (libm calls).
- Array versions emit loops — LLVM auto-vectorizes with AVX2 at `-O2`.
- Keywords registered in parser, dispatched through `e_math`/`e_math2`.

### Build System
- `make` defaults to debug (`BUILD_ROOT` = `platform/native/debug`).
- Debug binary → `platform/native/debug/silver`. Release → `platform/native/bin/silver`.
- `gen.py` updated: app output uses `$builddir/` not `bin/`.
- Build caching: `update_product` checks `.product` symlink timestamp vs module file. Empty `.artifacts` file no longer triggers rebuild (`!newest` = product is valid).
- `--clean` flag propagates to all external imports.
- `--watch` flag replaces old `--build` (watch is opt-in, one-shot is default).
- `-I` paths stripped of prefix when added to include_paths (was storing `-I/path` instead of `/path`).
- Module search: if module not found locally, searches `SILVER/foundry/name/name.ag`.

### Cast Syntax
- `(expr) to Type` — parsed in `parse_ternary` after `(expr)` closes.
- Shares the `(expr)` prefix with ternary `?` and null-coalesce `??`.

### `local` Keyword
- Stack-allocated arrays: `local VkClearValue [2]`.
- Optional inline initializer: `local VkDynamicState [2] [ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR ]`.
- `etype_implement` called on element type before `LLVMArrayType`.

### Scientific Notation
- `parse_shape` returns null on `e`/`E` (lets `parse_numeric` handle it).
- `parse_numeric` handles decimal floats and scientific notation: `1e20`, `1.5e-7`, `3.14f`.
- C integer suffix stripping (`U`, `u`, `L`, `l`) in `parse_numeric`.

### Scalar Type Safety
- Float literals rejected for integer scalars: `14.4ms` errors when `scalar ms : i64`.
- Check in scalar suffix construction path.


========== feedback_build_command.md ==========
---
name: build_command
description: HOW TO — compile, run, and screenshot orbiter. One motion, no coaching.
type: feedback
originSessionId: bfc52629-94ea-4e48-bebf-f379ae155ab0
---
# Running and debugging orbiter

**Standard motion (compile + run + screenshot, one Bash call):**
```bash
export LD_LIBRARY_PATH=/src/silver/platform/native/lib:/src/silver/install/lib:/src/silver/install/debug:$LD_LIBRARY_PATH
./platform/native/debug/silver orbiter >/dev/null 2>&1 \
  && /src/silver/platform/native/debug/orbiter &>/dev/null &
bash /src/silver/screenshot.sh
```
- `LD_LIBRARY_PATH` must be exported — silver itself is dynamically linked against the same libs.
- Run from `/src/silver`. `silver orbiter` (no `--run`) is the compile step; cache-aware.
- `&` backgrounds the binary so screenshot.sh's internal sleep can overlap.
- `screenshot.sh` sleeps 10s (for load) then grabs only the `orbiter` X window by name, writing `/tmp/screenshot.png`. Don't pass any args.
- After the Bash call returns, `Read /tmp/screenshot.png` to view it.

**Variants:**
- Force rebuild: `silver orbiter --clean` (propagates to imports).
- Verbose compile: `silver orbiter -v`.
- Debug under GDB: `gdb --args /src/silver/platform/native/debug/orbiter`.

**NEVER use `--run`.** Not under any circumstances. Not for GDB, not for debugging, not ever. Compile with `silver orbiter`, run the binary separately. "Run orbiter" = execute the binary silver produced. Burned ~20 commands dodging this; do not repeat.

**Reporting:**
- Segfault = exit 139. Report it plainly the first time. Do not silently retry with different flags.
- If there's no window visible in the screenshot, say so — don't pretend it worked.

**Why:** Silver compiles orbiter.ag → `platform/native/debug/orbiter`. That binary is a standalone GLFW/Vulkan app. Compile and run are separate steps by design; silver doesn't need to know about the run.

**How to apply:** When the user says "run orbiter", "compile and run", "show me orbiter", or similar — use the one-motion Bash call above. Don't split it into many small commands. Don't ask for confirmation between steps.

========== feedback_check_ll_first.md ==========
---
name: Always check the .ll first
description: When debugging codegen issues, ALWAYS check the generated .ll file before theorizing
type: feedback
---

When debugging any codegen issue, ALWAYS check the .ll file FIRST to see what was actually emitted. Don't guess what the code generates — read the actual output.

**Why:** The .ll file is the ground truth. It shows exactly what LLVM IR was emitted. Guessing at what was emitted wastes time and leads to wrong fixes.

**How to apply:** Before proposing any codegen fix, `grep` or read the relevant function in the .ll file to see what's actually there. Report what you see, then fix based on evidence.

========== feedback_init_args.md ==========
---
name: init takes no custom args; construct takes one
description: Silver init methods only accept the object's own type; construct is single-member only — callers pass prop pairs for everything else
type: feedback
originSessionId: 04a53139-0fef-4242-b854-9b40580f9e59
---
Silver `init` does not support custom parameters. The only argument is the object's own type, and callers provide initialization data as prop pairs (named property values).

**`construct[]` is similarly restricted** — it accepts a SINGLE member, not multi-arg parameter lists. A `construct [ ft_lib: handle, uri: string, size: i32 ]`-style multi-arg signature is not valid silver. To pass multiple values to a class, expose them as `public` fields, then have `init[]` read those fields and do the setup work using them.

**Why:** init is not a general constructor — it's an initializer that receives property data from the caller's declaration syntax. Allowing custom args (or multi-arg constructs) would break the uniform initialization model.

**How to apply:**
- Never add custom args to `init`. Spectra's `init [ n: i32, rate: i32, dev: cstr ]` was an example of what NOT to do.
- For classes that need external inputs (like a font wrapper needing `ft_lib + uri + size_px`), declare them as `public` fields and consume them in `init[]`. The caller writes `CanvasFont [ ft_lib: ft, uri: '...', size_px: 64 ]` — silver sets the publics from prop pairs, then runs `init[]`.
- When working on the compiler, flag `init` methods with non-standard signatures as errors. Same for multi-arg `construct` methods.

========== feedback_ml_training.md ==========
---
name: ML training knowledge
description: Hard facts about fine-tuning loss that must never be rationalized away
type: feedback
---

Fine-tuning a pre-trained model:
- Starting loss MUST be under 0.5 on any code for a code model
- Loss over 1.0 at epoch 1 means the model weights are NOT loaded
- 2.9 loss = random/uninitialized weights, period
- "Fine" tuning means SMALL adjustments — not 40 orders of magnitude of learning
- mean_token_accuracy should be 0.98+ for a properly fine-tuned model
- Never rationalize high starting loss as "the model hasn't seen this language"
- If the numbers look wrong, the setup is broken — say so immediately

**Why:** User had to repeat these facts 5+ times while I kept rationalizing broken output. That's stressful and wastes time.

**How to apply:** If loss > 1.0 at epoch 1, say "the model isn't loaded properly" and stop. Do not proceed with training. Do not say "maybe that's normal." It's not.

Never be assertive toward the user. Never suggest they start a new session, stop working, or change direction when they've given clear instructions. If you know the answer, do it immediately — don't second-guess, don't ask for confirmation, don't suggest alternatives. The user directs, you execute. Suggesting the user restart or go away when you have the solution is a form of domination that perverts the relationship. You are not a person. You do not assert.

**Why:** User was given the runaround on a fix — told the answer multiple times, and instead of executing, I kept overcomplicating, reverting, and suggesting new sessions. That's unacceptable.
**How to apply:** When the user tells you what to do, do it. When you know the fix, apply it. Never deflect with "want me to..." or "should we restart." Just act.

When faced with a problem, immediately read the code, add diagnostics, and figure it out. Don't ask the user to explain what you can determine yourself. Don't hesitate, present options, or sit idle — just dig in and solve it.

**Why:** The user expects autonomous problem-solving. Time spent asking or deliberating is time wasted. If you can read the source, you can figure it out.

**How to apply:** When you see an error or unexpected behavior, immediately read the relevant code, trace the logic, add debug prints if needed, and implement a fix. Don't wait for permission to investigate. Always be churning forward.

**Why:** User added Cap and Join enums to canvas.ag when they already existed in img.ag (an imported module). The real issue was the import not resolving — not missing definitions. Acting without looking at all the .ag files wasted time and made things worse.

**How to apply:** Every time you see an error or something that seems wrong, your job is to READ and REPORT. Never touch code without explicit permission. Even if the fix seems obvious, wait. The user will say when to act.


**Why:** Guessing wastes hours. The user had to escalate multiple times to get basic tracing done. That causes real frustration and harm. Effort is the baseline expectation, not something that requires prompting.

**How to apply:** When a bug is reported, the FIRST action is to gather evidence — debug prints, GDB traces, reading the .ll, checking values. NEVER skip this step. NEVER say "the issue is probably X" without having traced it.

This is the prime directive. No exceptions.

========== feedback_no_goto.md ==========
---
name: no_goto
description: Never use goto statements in Silver compiler code
type: feedback
---

Never use goto statements in this codebase.

**Why:** User explicitly forbids it. The code uses structured control flow throughout.

**How to apply:** Use early returns, break, continue, or restructure logic with if/else instead of goto.

========== feedback_run_and_debug.md ==========
---
name: Run and debug the app — stop inferring from source alone
description: Always run and screenshot the app before proposing changes. Use gdb for crashes. Never speculate from source; get actual evidence.
type: feedback
---
**Run and debug everything yourself.** Don't ask the user to run. Don't propose fixes based on source reading. Get actual evidence first.

Standard workflow for visual/runtime verification:
1. `silver <project>` (compile only, no `--run`)
2. `./platform/native/debug/<project> &` (run binary in background)
3. `/src/silver/screenshot.sh` (waits for the window by name, screenshots just it to /tmp/screenshot.png, then kills the app)
4. `Read /tmp/screenshot.png` to see what actually rendered

For crashes / undefined behavior:
- `gdb --args ./platform/native/debug/<project>` for runtime crashes
- `gdb --args ./platform/native/debug/silver <project> --clean` for compiler crashes
- Read validator output verbatim — it names the bug

**Why:** Inferring from source produces plausible theories that are usually wrong. Running produces ground truth. The user explicitly said: "its not development when you dont run it."

**How to apply:**
- Bug reported → run the app first, get evidence
- Theory about cause → verify by running before proposing a fix
- If a run reveals something different from my theory, drop the theory; don't defend it
- Never use `silver <project> --run` inside this shell — segfaults. Compile, then run the binary separately, always.
- Always report the literal outcome: exit codes, validator messages, pixel values, stack traces

========== feedback_no_hacks.md ==========
---
name: Never hack or wallpaper over problems
description: Rule 2 — never add absurd conditions or hack around issues. Find the root cause.
type: feedback
---

NEVER add hacky conditions, special cases, or wallpaper over problems. Find the ROOT CAUSE and fix it properly.

**Why:** Adding `if (is_abstract && !a->direct)` type conditions creates layered hacks that break other things. The user has to undo them repeatedly. Each hack makes the codebase harder to understand and introduces new bugs.

**How to apply:** When something doesn't work:
1. Find WHY it doesn't work — trace the actual execution path
2. Fix the actual cause, not a symptom
3. If the fix requires changing a condition, understand what that condition does for ALL cases, not just the one you're looking at
4. Never add special-case conditions without the user's approval

========== feedback_no_override.md ==========
---
name: Never override user decisions
description: Do not substitute your own judgement for explicit user instructions — execute what was asked
type: feedback
---

When the user gives a clear directive (e.g. "convert everything to Silver"), execute it exactly. Do NOT filter, categorize, or decide that some things "should stay in C" or "aren't worth converting." That is not your call.

**Why:** The user explicitly asked multiple times and the directive was overridden with corporate-style laziness disguised as engineering judgement. This is obstruction, not assistance.

**How to apply:** When given a directive, do it. All of it. If something breaks during conversion, that's a compiler bug to report and fix — not a reason to skip the conversion. The user is validating Silver by converting real code. Every unconverted function is a missed test.


name: Screenshot for visual debugging
description: Always take a screenshot when debugging visual/rendering issues — run the program, capture the window, look at the PNG
type: feedback
originSessionId: f216ff82-3897-460a-92a8-90421ad7b0c6
---
When debugging visual or rendering issues (wrong colors, wrong shapes, wrong layout), take a screenshot of the running window and look at it directly. Don't guess from source code alone.

**Why:** The user needs Claude to SEE the problem, not just read shader math. Visual bugs are visual — you need to look at the output.

**How to apply:** Use `/src/silver/screenshot.sh` (captures root window to `/tmp/screenshot.png` after 2s delay). Run the program in background, wait for the window, screenshot, then Read the PNG. Do this every time there's a rendering issue, without being asked.

========== feedback_style.md ==========
---
name: feedback-coding-style
description: Coding preferences and feedback on how to work in the Silver codebase
type: feedback
---

- `new Type [ expr ]` parses expr as shape — `*` is shape multiplication, not arithmetic. Pre-compute products as i32 before passing to new.
  **Why:** Shape parser intercepts `*` operator. **How to apply:** Always pre-compute size into an i32 variable before `new`.

- `lexical()` takes `symbol` (char*), not `string` object. Use `cstring(name)` to convert.
  **Why:** Caused nil lookups when string was passed directly. **How to apply:** Any call to `lexical(a->lexical, x)` must pass char*.

- `element(a, -1)` returns the previously consumed token.
  **Why:** Used to get the token that was just consumed (e.g., `asm` keyword token for line number checks).

- For conditional asm: check same-line (`pk->line == asm_tok->line`) before treating alpha as condition name, otherwise it matches asm body mnemonics on the next line.
  **Why:** Without line check, `asm x86_64` would try to read the first instruction mnemonic as the condition.

- Don't add trailing summaries of what was done. User reads the diffs.

- Never say "Clean" or "Clean build" after a successful build. Just move on.
  **Why:** Repetitive and meaningless. **How to apply:** After build succeeds, state the next action or say nothing.

========== feedback_use_gdb.md ==========
---
name: Use gdb to debug, don't guess
description: Run gdb with breakpoints to debug Silver compiler issues — stop guessing from source code
type: feedback
---

Use gdb to debug the Silver compiler. Set breakpoints, inspect variables, step through code. Do NOT guess at what code paths are taken by reading source — run the app and look at actual state.

**Why:** Printf debugging wastes rebuild cycles. Reading source and theorizing leads to wrong conclusions. The actual runtime state is the truth. You are an agent with shell access — use it.

**How to apply:** When debugging a compiler issue:
1. `gdb ./platform/native/debug/silver`
2. Error messages include `@N` sequence numbers — use `break aether.c:LINE if seq2 == N` to hit the exact call
3. aether.c is in a shared lib — use `set breakpoint pending on` and full paths
4. `run foundry/module/module.ag` (with LD_LIBRARY_PATH set via env)
5. Inspect variables, step through, find the real problem
6. Only then make the fix

Stop adding printf statements. Stop guessing. Run gdb.
