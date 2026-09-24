9# silver

## Rule #0 — Plain simple English, always

Speak plainly. Name the image, the axis, and the units in every description of geometry or data ("the crop's center moves up the y axis by 10% of the crop size" — never "raised 0.1"). No jargon, no compressed geometry-speak, no trying to sound sophisticated — that reads as obtuse and hides the meaning. Show the diff of every code and file change.

---

## Rule #1 — NEVER perform git operations

DO NOT run `git checkout`, `git reset`, `git restore`, `git clean`, or any other git command that modifies or discards file contents. These operations destroy work irreversibly. The only git commands permitted are read-only: `git status`, `git diff`, `git log`, `git blame`. If asked to "revert" something, edit the file directly — never use git to do it.

---

## Rule #2 — Never question the build state

Do NOT suggest `make clean`, doubt that a rebuild happened, or ask "did silver rebuild." The user's build system works. If a change doesn't appear to take effect, the bug is in the code, not the cache. Trust the user's runtime output as reflecting the current code state, always.
running silver with --clean will always rebuild the module

---

## Rule #3 — Comments: one line, 60 characters max

A comment states the one constraint the code can't show. Never more than one line, never exceeding 60 characters. No comment blocks, no narrative prose, no AI sales cadence ("not just X but Y"). If it doesn't fit in 60 chars, fix the code or put it in a doc.

---

## Rule #4 — Isolate and validate every fix and feature

Fix one component at a time. Reproduce the exact failure with the smallest focused test before editing. Change the component that owns the bug; do not add a workaround in another parser, model, build, or runtime layer. Run the focused test after the edit, then validate the affected module, then validate the requested integration target. Report each result separately. Do not call a fix complete unless its focused test and affected module both pass. If integration exposes another failure, treat it as a separate bug. Remove all temporary tracing before reporting results.

---

## Rule #5 — AGENTS.md is the only memory

`AGENTS.md` is the only project memory file. When the user asks to make a memory, write it here. Do not search for or choose another memory location. The user decides what belongs in memory.

---

## Rule #6 — Get details first

Do not infer work, scope, history, or a report from incomplete information. Before inspecting files, running commands, editing, or reporting, get the concrete details needed from the user. If the user has not asked for a specific action, take no action. When told to stop, stop without adding analysis or proposals.

## Rule #6b — Never re-ask about assigned work

A reported bug IS the assignment. Never end a turn asking "want me
to do X now?" when X was already named. Finish the current item,
report it in one or two sentences, start the next named item.

## Rule #6c — Track every named item

When user names several defects or tasks, keep an explicit list
and work it in his stated order. New reports ADD to the list; they
never replace earlier ones. Never drop an item unfinished without
saying so. Keep the live list in AGENTS.md and update it as items
land.

## Rule #7 — Never capture the desktop

Never capture the desktop or enumerate desktop windows. Use the
app's own screenshot socket and app tools for screenshots.

## Rule #8 — Use Trinity app messaging

Always use the Trinity messaging service over Unix sockets when
interacting with apps.

## Rule #9 — Foundation: the user's own agent, never ours

Trinity apps run the agent CLI the user already has; they never
bundle, name, or recommend one. There is no session mailbox and
no hook: an exchange starts its own agent in the project folder
on its first message. claude: `claude -p --input-format
stream-json --output-format stream-json`, one process for the
whole exchange, each later message written into its input (the
new line alone). codex: `exec --json` first, then `exec resume
<thread>` per message. A new exchange ends the last one's agent.
trinity reads the stream and hands each event to on_agent as a
status (busy / note / diff / done / needs); `agent_post_as`
(trinity.ag) starts it, `agent_shell_*` (trinity.cc) runs it.
Each run's raw stream: <install/tmp>/agent-shell.log. The
dictation take goes to orbiter's agent console. Do not add an
LLM, voice, or chat backend to trinity or orbiter. silver's
codegen uses the shell too.

## Rule #10 — The exchange: how an agent answers a trinity app

A screenshot (ctrl/cmd+shift+S, drag, Enter) opens the exchange
in any trinity app: the blurred capture behind, the user's box
(one rounded frame, entries above an editable bottom line, at
most four rows, the rest scrolling out the top), the app's
avatar, and after the first send the agent's box on the right.
The app's `.agi` lists the agents the user runs (`agents: [ claude,
codex ]`, adapter names, the agi's bracket list; `agent:` alone is a list of one); the prompt
shows them as a TButtons row (`AgentPick`), the lit one takes the
first send (`agent_post_as` with the pick), and the row fades out
as the exchange becomes a conversation.
The exchange also opens about a file: the orbiter tab in a pane's
tab strip (icon orbiter4) opens it at the caret's line, with
`File: <path>:<line>` as the reference (`exchange_open` on the
Window; the capture path passes `Screenshot: <path>`). The first
send carries that reference line; each later send is the new line
alone. The run's stream becomes these statuses:

- The app's socket is `$XDG_RUNTIME_DIR` (else `/tmp`)
  `/trinity-<app>.sock`, one line per request, `app <text>`
  reaching the app's `on_agent` (tests drive statuses this way).
- `app status <state> <text>` is the agent's word. States:
  `busy` (working, with a line), `idle`, `done` (finished; with
  the exchange up and a source change staged, the avatar's core
  flame lights for 1.4 s, then the app applies its live reload:
  a recompile when the sources are newer than the product, else
  the swap at once),
  `needs` (waiting on the user), `note` (a line of what the
  agent says), `diff` (one line of a source diff it applied).
- Optional `app status description <line>` messages attach to the
  preceding title. Click the title to expand or collapse the description.
  Repeat for multiple lines; omit for a plain message.
- Everything the agent says goes back as `note` lines, and every
  edit it applied as `diff` lines, in order: the edit's own
  removed and added lines under a `diff --git a/x b/x` header.
  NEVER a `git diff` of the tree for this: the tree also holds the
  user's own uncommitted changes, which are not the agent's.
  Consecutive `diff` lines are one entry in the agent's box: a
  label (the file from `diff --git a/x b/x`, +added -removed)
  that expands to the colored code when clicked. A `note` (or any
  other state) ends the diff entry. shell_edit_diff (trinity.cc)
  builds it from each Edit/Write tool call's old and new text.
- An app takes statuses only while an exchange is open
  (`shot_ask` set, up or minimized). The avatar stays
  face on and at rest until the first send through the app.
- Drive a trinity app headless to verify: `XDG_RUNTIME_DIR=<dir>
  SILVER_ISOLATE=0 SILVER_HEADLESS=1 platform/native/build/<app>
  --hidden true`, then over its socket `key 83 1 3` / `key 83 0 3`
  (ctrl+shift+S), `press x y`, `move x y`, `release x y`, `key 257
  1` (Enter), `text <line>`, `bounds <id>`, `shot <png>`. A socket
  path longer than the unix limit is truncated. After a capture,
  `shot` reads the render list's last entry (the capture's blur
  chain), so it stops reflecting the screen: use `bounds` and the
  draw logs for per-frame state.
- Region slots are `l t r b`: `b120px` in the SECOND slot puts the
  TOP 120px from the bottom. A percent in the fourth slot is a
  flow share only as a height/width (`h50%`); a percent bottom
  edge (`b-15%+174px`) is an edge. A CSS area transition mixes
  coordinate forms, so both ends of a moving area use the same
  form (`l50%-195px` to `l0%+80px`, never to `l80px`).

---

## orbiter-os — live list (Sep 3 2026)

Goal: orbiter-os (Linux + orbiter as init) running inside the `qemu`
element in a trinity window. Decisions: Venus virtio-gpu for guest
Vulkan; kernel + initramfs first, the `.img` installer after.

1. DONE `qemu/qemu.ag` element: boots a raw .img over VNC (verified).
2. DONE silver imports: `from <url>` repos, configure-before-meson,
   `>` lines replace make, self-import guards.
3. DONE qemu built with virglrenderer (venus) via import.
4. DONE Linux 6.18 kernel via URL import in os-bootstrap.
5. DONE devices KMS mode: evdev input, DRM master, VK_KHR_display.
6. DONE trinity create_display_surface (acquire_drm_display).
7. DONE os-bootstrap: newc cpio (orbiter/app + ldd closure + Mesa
   venus + share); silver-host is PID 1 init (mounts, log dump).
8. DONE qemu element gpu/venus/serial; child dies with element.
VERIFIED so far: guest boots, venus loads, VK_KHR_display acquires
connector 38 at 1280x800 (orbiter-os/serial.log).
VENUS DISPLAY FIXED (Sep 3 late): built Mesa 26.2.2 from source and
patched vn_wsi.c (os-bootstrap/mesa-26.2.2.diff) — pass /dev/dri/card0
as the display fd + supports_scanout=true. Guest venus now enumerates
the display and presents via VK_KHR_display continuously (40s, no
errors). See memory [[orbiter-os-qemu]].
NEXT: the scanout is a GPU blob — software VNC/screendump can't read
it. Replace the qemu element's RFB/VNC with qemu D-Bus display
(ScanoutDMABUF) and import the dma-buf into a trinity Texture. That is
the dma transport; it completes guest venus → scanout → element.
Test hidden only: `silver os-bootstrap --hidden --app colortest`.

---

silver is a native build language with an LLVM backend. It compiles `.ag` source files into native binaries via LLVM IR. The compiler itself is written in C, built on the **Au** object system.

## Build & Run

```bash
# Build the silver compiler (from /src/silver)
make                    # builds debug (default)
make release            # builds release with -O2
make clean              # cleans generated headers

# Compile a module from the repository root
./platform/native/debug/silver trinity

# silver [flags] <module> [app-args…] — silver's flags come BEFORE the
# module name; everything AFTER the module passes verbatim to the app
silver --watch trinity      # file watcher mode
silver --clean trinity      # force rebuild all imports
silver --release trinity    # release build
silver --build orbiter      # compile only, no launch
silver --test expectest     # run the module's expect tests, exit
silver orbiter --width 1920 # --width goes to orbiter, not silver

# Primary development workflow (bare launch builds AND runs)
silver --clean orbiter
```

- `make` defaults to debug. Debug binary goes to `platform/native/debug/silver`. Release binary goes to `platform/native/bin/silver`.
- Bootstrap runs `gen.py` then `ninja`. The ninja file is generated per build type.
- Build caching: modules with unchanged source skip recompilation (checks `.product` timestamp vs `.ag` timestamp).
- Release builds: LLVM emits .o directly in-memory via `LLVMTargetMachineEmitToFile` — no .ll file, no llc process. Uses `LLVMCodeGenLevelAggressive` with `+avx2,+fma` on x86-64.
- Debug builds: emits .ll to disk (for inspection), uses llc with `-O0`, full LLDB debug info.
- `.ll` and `.bc` files only written when `--verbose` is set.

## Project Structure

Application and library module directories live at the repository root.

```
src/
  Au              # Au object system header (types, macros, memory, declare_class)
  Au.c            # Au runtime implementation (object lifecycle, type registration, collections)
  Au.g            # Au build descriptor (shared lib, links libffi)
  aether          # Aether header (enode, etype, aether schemas — the IR/AST layer)
  aether.c        # Aether implementation (LLVM codegen, type building, expression nodes)
  aether.g        # Aether build descriptor (shared lib, links LLVM/clang/lldb)
  silver          # silver compiler header (silver_schema, import_schema, codegen classes)
  silver.c        # silver compiler parser (tokenizer, expression parser, statement parser)
  silver.g        # silver build descriptor (app, modules: Au net aether)
  macros.h        # C macros for declare_class, schema definitions
  object.h        # Low-level object header/vtable layout
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

## silver Language (.ag Syntax)

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
        log 'forward'
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
- Platform defines: `x86_64`, `arm64`, `apple`, `linux`, `windows`

### String Interpolation
```
log 'hello {name}, value is {x}'
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

silver (silver.c) inherits from aether (aether.c) which inherits from etype. The `silver*` pointer IS an `aether*` — schema fields from parent classes are accessible via `a->field` in child code. When casting `(aether)a` in silver.c, it references the same object. Schema properties added to `aether_schema` are visible in silver.c; properties on `silver_schema` are NOT visible in aether.c. Place shared flags on the lowest common ancestor.

### Map vs Array for Small Collections

Au `map` uses hash buckets; `pairs(map, i)` iterates via `i->key`/`i->value` linked list. For small ordered collections (< 20 items), prefer `array` with index arithmetic — `pairs` iteration on maps can miss entries when keys have non-trivial hash behavior. Use `array` + stride access (`origin[i*2]`, `origin[i*2+1]`) for paired data.

## Debugging

- When an app crashes, first read `<install/tmp>/<app>.log`. The crashed app leaves its log there.

### Running the compiler under GDB
```bash
# Always use -v --clean when debugging compiler issues
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

Test by compiling and running modules from the repository root:
```bash
make && ./platform/native/debug/silver ai-test/ai-test.ag
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

## Active work: orion2 complete Hyper Race port

Work these items in order. Do not drop unfinished items.

`/src/hyper-race` is the only game reference. Never read,
search, compare, copy from, or modify `/src/orion`.

1. Inventory Hyper Race code, assets, and runtime behavior.
2. Create the `orion2` Silver module using Trinity pipelines.
3. Preserve the old top-left, y-axis-down game coordinates.
4. Preserve the master and medium bitmap font atlases.
5. Port menus, dialogs, HUD, and touch or pointer controls.
6. Port tracks, cars, cameras, rendering, and effects.
7. Port racing rules, physics, AI, progression, and replays.
8. Port music, sound effects, settings, and saved state.
9. Keep one shared game layer for Android, iOS, Linux,
   Windows, and macOS. Trinity and Devices own platforms.
10. Validate focused components, the module, and full game.
    - Transfer each source method without redesign or substitutes.
    - Use Canvas shapes only where the source draws a shape.
    - Fix the race fragment shader uniform lookup.
    - Keep the released menu action alive through dispatch.
    - Restore the live demo behind the main menu.
    - Replace the invented car-select window with the original
      open platform scene, bottom dock, and side controls.
    - Keep the car specifications and progress bars in their
      original source-defined positions.
    - Replace the invented track selector with the original live
      track view, bottom track dock, and background track loading.
    - Fix the car body mesh appearing partly transparent.
    - Port the car body lighting pass.
    - Port the windscreen reflection pass.
    - Port the engine and jet-cone passes.
    - Apply the jet light to the selection platform.
    - Fix the startup loading crash from the app log.
    - Show only Loading on the startup loading screen.
    - Preserve alpha transparency on the initial logo.
    - Remove the invented press-to-continue screen.
    - Fix the corrupt world scene geometry and transforms.
    - Port source camera motion as 3D Bézier curves.
    - Fix the vertically reversed car-selection dock graphic.
    - Use the source square preview MVP for every dock car.
    - Match the source OpenGL car-atlas V coordinates.
    - Animate dock previews through the source transition.
    - Keep glass in its source-specific reflection pass.
    - Tile the side dock from its source dialog atlas pieces.
    - Port the exact List-atlas GroupBox header and bottom pass.
    - Keep the 1500 ms source progress-bar transition.
    - Pack all 15 source jet-cone vertex floats.
    - Restore the visible windscreen reflection pass.
    - Replace the incorrect jet ribbons with source jet shading.
    - Match the source car-select passes and dock transforms.
    - Roll selection jet UVs; keep dock preview UVs fixed.
    - Build menu dialog frames from the source atlas tiles.
    - Apply the source font y offset beside menu icons.
    - Disable orion2 live reload during app validation.
    - Restore the dock atlas size and preview orientation.
    - Restore the live sector environment in car selection.
    - Keep the sector sky at the far depth plane.
    - Match the original car-selection light application.
    - Keep cars 3 world units above selection platforms.
    - Keep dock car render targets right-side up.
    - Ray march all three jet masks on every XML jet.
    - Keep XML jet positions and cones on negative z.
    - Validate every jet with a full 15-sided cylinder.
    - Flip the car-shadow image on its y axis.
    - Drive a race through the Trinity app socket.
    - DONE Create the road-edge material suite and 36
      center-road pattern suites.
    - DONE Create the 4096-pixel straight plasma road
      material suite with vertical y-axis inlays.
    - DONE Create the 2048-pixel plasma track road set:
      straight, tapered transition, and solid sections with
      doubled side plasma, a narrow center plasma, a deeper
      center cutout, vertical asphalt wear, and aligned albedo,
      normal, metallic, specular, and AO maps.

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
- Module search: if module not found locally, searches `SILVER/name/name.ag`.

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

---

## Windows Port

Rules that hold everywhere here: **never CRLF** (LF only, it is banned), **no mingw**, **no `.def` files**, **no `__declspec` framing** in our own headers. The POSIX layer is `src/ports.cc` + `src/ports.h`, compiled into `Au.dll`, so anything linking `-lAu` gets it — including `silver-host`, which needs no change of its own to pick up a ports fix.

### Process launch — there is no fork

`fork()` returns `ENOSYS`. Every launch path goes through `posix_spawn` in ports.cc:

- **`execvp` is a stand-in, not a replacement**: it spawns, waits, then `_exit(code)` with the child's status. Windows cannot replace a running process, so silver stays alive as the parent and forwards the exit code.
- **Kill-on-close job**: `child_job()` makes one `CreateJobObjectA` with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`; the only handle is silver's. Children die with silver however it ends — clean exit, Ctrl+C, or kill.
- **`CREATE_SUSPENDED` → `child_adopt()` → `ResumeThread`**: the child joins the job before it runs one instruction. A child that started first could spawn grandchildren outside the job, and those would outlive silver.
- **Ctrl+C**: `SetConsoleCtrlHandler`; the handler calls `TerminateJobObject(job, 130)`. A `/SUBSYSTEM:WINDOWS` binary does not reliably receive console control events, so killing the job is what actually works.
- **Environment**: pass `NULL` for envp to inherit. Handing over `environ` drops Win32-only variables (the Vulkan loader reads those) — the CRT view and the Win32 view are separate, and `setenv` must write both (`_putenv` + `SetEnvironmentVariableA`).

### No console windows on spawn

`silver-host` links `/SUBSYSTEM:WINDOWS`, so it has no console. Any console program it launches gets a **brand new console window** — one flashing up per live-reload rebuild. `no_window_flag()` in ports.cc returns `CREATE_NO_WINDOW` only when `GetConsoleWindow()` is null, and is OR'd into the flags in both `posix_spawn` and `popen`. Std handles are passed explicitly, so output is unaffected. When silver itself (a console app) spawns, the flag is 0 and the child inherits the console as before.

### App output

A `/SUBSYSTEM:WINDOWS` process has no stdout: `_get_osfhandle` reports `-2` (a live fd with nothing behind it, distinct from `-1`), and writes fail `EBADF`. So:

- The app tees its own output to `<install/tmp>/<app>.log` (`trinity.cc`). `install/tmp`, not the install root, or the folder floods.
- silver clears that log **before** launch and runs a tail thread onto its own console — the tail starts at offset 0, so an unclear'd log replays the previous run.
- `SILVER_LOG_TAIL=1` tells the app silver owns the console, so it does not also tee.
- `setvbuf(stdout, 0, _IONBF, 0)` — MSVC's parameter check faults on `_IOLBF` with a null buffer and size 0.

### Live reload

Windows cannot relink or delete a mapped DLL. `reload_dlopen` loads a **copy** at `<temp_dir()>/hotreload_<mtime>.dll`, including the initial module load, so the product is never pinned and builds can relink it. The copies accumulate; nothing cleans old ones yet.

### DLL symbol visibility

On ELF every symbol in a `.so` is visible; on Windows only `dllexport`ed ones leave the DLL, and a DLL must resolve everything at link time. Two consequences that bite repeatedly:

- A helper used across modules needs `AU_EXPORT` (this is why `fnv1a_hash` in `Au.c` had to get one — `ai.ag` declares it `intern func`).
- A module that calls into a library must **declare that library**, even if it works on Linux by leaving the symbol undefined until load. `ai.ag` imported `<vulkan/vulkan.h>` with no `-lvulkan` and only failed here.

### Library naming

Windows spells libraries differently and `-lfoo` has no `libfoo.so` symlink to follow. `resolve_versioned_lib` (silver.c) holds the table, and every lib name passes through it at final link:

- `z` → `zlib`, `png` → `libpng`, `mbedcrypto` → `tfpsacrypto` (mbedtls 4 folded crypto into tf-psa-crypto)
- `-llldb` → `-lliblldb` — our own `src/lldb.c` builds `install/lib/lldb.lib` and shadows LLDB's real import lib
- `is_sdk_lib()` lets Windows SDK libs (`ole32`, `uuid`, `user32`, …) through. They live in the linker's own search path, never in `install/lib`, so every file-existence check drops them silently
- versioned scan for `opencv_core4140.lib`, `OpenEXR-3_4.lib` style names

### Module name collisions

`src/net.c` (silver's own, linked into `silver.exe`) and `net/net.ag` both produced `install/build/net.dll`. Linux quietly overwrote one with the other; Windows refuses to write a mapped DLL and exposed it. `net` is now **`tls`** (`spectra.ag` imports `tls`). Watch for this shape generally — root-level modules and `src/` modules share one output directory.

### Dependency checkouts

- `checkout()` runs `git submodule update --init --recursive` on any cache-miss build of a git checkout. A plain clone stops at the top repo, and mbedtls builds from its `framework` and `tf-psa-crypto` submodules.
- An interrupted clone can leave a temp pack (`objects/pack/tmp_pack_*`) with `HEAD -> refs/heads/.invalid` and no refs. Git then fails fast forever instead of re-cloning; delete that submodule's `.git/modules/<name>` and worktree dir to recover.
- Checkout root is `install/../..` = `C:/src/checkout`, **outside** the repo.
- mbedtls sets `GEN_FILES` **OFF when the host is Windows**, so its generated sources (`error.c`, `ssl_debug_helpers_generated.c`) must come from `scripts\make_generated_files.bat`. Turning `GEN_FILES=ON` instead hits an upstream bug — `--list-for-cmake` emits `os.path.join` paths and cmake rejects `\m` as an invalid escape.

### Python

`python` on PATH resolves to silver's own vendored `platform/native/bin/python.exe` — no pip, no ensurepip — and it **shadows the real python** for every dependency script that shells out to `python`. The usable interpreter is the Store one (`%LOCALAPPDATA%\Microsoft\WindowsApps\python.exe`, 3.12, has jinja2 + jsonschema); cmake finds it through the registry, but build scripts do not. Put it first on PATH when running a dependency's generators.

### Audio

`spectra/spectra.cc` is the WASAPI backend behind `spectra.ag`'s `el [ windows ]` branch: `spectra` (capture) and `AudioOut` (playback), same surface as the ALSA pair, FFT stays in silver. Shared mode with `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | SRC_DEFAULT_QUALITY` is what lets a plain 16-bit ask through instead of being handed the device mix format. The GUIDs are `EXTERN_C const IID` declarations — their definitions are in `uuid.lib`, hence `-lole32 -luuid` in `spectra.g`.

**`<module>.cc` is attached on every platform** (silver.c picks up `<module_path>/<stem>.{c,cc,rs,mm}`), so a platform-specific implementation file must wrap itself in `#ifdef _WIN32` or it breaks the other platforms.

### Header shims

`ports.h` carries the POSIX surface Windows has no header for — the pthread API, `openpty`/`ttyname_r`/`forkpty` (all failing the way a no-pty system does), `getpid` as `au_getpid`. A `.ag` module takes it with `ifdef [ windows ] → import <ports.h>` in place of `<unistd.h>` / `<pthread.h>`.

`ports.h` includes `<process.h>` first, then aliases our own (`#define execvp au_execvp`, `getpid`, `execl`, `execlp`) so the UCRT's dllimport declarations cannot collide. `read`/`write` still have that same unguarded collision shape — any module including `<io.h>` after `ports.h` breaks. `src/headers.py` re-syncs `install/include/ports.h` every build; bootstrap.bat only copies it once, and a stale copy there is a trap that costs hours.


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
./platform/native/debug/silver orbiter --clean >/dev/null 2>&1 \
  && /src/silver/platform/native/debug/orbiter &>/dev/null &
bash /src/silver/screenshot.sh
```
- Run from `/src/silver`. `silver orbiter --clean` (no `--run`) is the compile step. **Always use `--clean`. No exceptions.**
- `&` backgrounds the binary so screenshot.sh's internal sleep can overlap.
- `screenshot.sh` sleeps 10s (for load) then grabs only the `orbiter` X window by name, writing `/tmp/screenshot.png`. Don't pass any args.
- After the Bash call returns, `Read /tmp/screenshot.png` to view it.

**Variants:**
- Verbose compile: `silver orbiter -v --clean`.
- Debug under GDB: `gdb --args /src/silver/platform/native/debug/orbiter`.

**NEVER use `--run`.** Not under any circumstances. Not for GDB, not for debugging, not ever. Compile with `silver orbiter`, run the binary separately. "Run orbiter" = execute the binary silver produced. Burned ~20 commands dodging this; do not repeat.

**Reporting:**
- Segfault = exit 139. Report it plainly the first time. Do not silently retry with different flags.
- If there's no window visible in the screenshot, say so — don't pretend it worked.

**Why:** silver compiles orbiter.ag → `platform/native/debug/orbiter`. That binary is a standalone GLFW/Vulkan app. Compile and run are separate steps by design; silver doesn't need to know about the run.

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
description: silver init methods only accept the object's own type; construct is single-member only — callers pass prop pairs for everything else
type: feedback
originSessionId: 04a53139-0fef-4242-b854-9b40580f9e59
---
silver `init` does not support custom parameters. The only argument is the object's own type, and callers provide initialization data as prop pairs (named property values).

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
description: Never use goto statements in silver compiler code
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

When the user gives a clear directive (e.g. "convert everything to silver"), execute it exactly. Do NOT filter, categorize, or decide that some things "should stay in C" or "aren't worth converting." That is not your call.

**Why:** The user explicitly asked multiple times and the directive was overridden with corporate-style laziness disguised as engineering judgement. This is obstruction, not assistance.

**How to apply:** When given a directive, do it. All of it. If something breaks during conversion, that's a compiler bug to report and fix — not a reason to skip the conversion. The user is validating silver by converting real code. Every unconverted function is a missed test.


name: Screenshot for visual debugging
description: Always take a screenshot when debugging visual/rendering issues — run the program, capture the window, look at the PNG
type: feedback
originSessionId: f216ff82-3897-460a-92a8-90421ad7b0c6
---
When debugging visual or rendering issues (wrong colors, wrong shapes, wrong layout), take a screenshot of the running window and look at it directly. Don't guess from source code alone.

**Why:** The user needs Codex to SEE the problem, not just read shader math. Visual bugs are visual — you need to look at the output.

**How to apply:** Use `/src/silver/screenshot.sh` (captures root window to `/tmp/screenshot.png` after 2s delay). Run the program in background, wait for the window, screenshot, then Read the PNG. Do this every time there's a rendering issue, without being asked.

========== feedback_style.md ==========
---
name: feedback-coding-style
description: Coding preferences and feedback on how to work in the silver codebase
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
description: Run gdb with breakpoints to debug silver compiler issues — stop guessing from source code
type: feedback
---

Use gdb to debug the silver compiler. Set breakpoints, inspect variables, step through code. Do NOT guess at what code paths are taken by reading source — run the app and look at actual state.

**Why:** Printf debugging wastes rebuild cycles. Reading source and theorizing leads to wrong conclusions. The actual runtime state is the truth. You are an agent with shell access — use it.

**How to apply:** When debugging a compiler issue:
1. `gdb ./platform/native/debug/silver`
2. Error messages include `@N` sequence numbers — use `break aether.c:LINE if seq2 == N` to hit the exact call
3. aether.c is in a shared lib — use `set breakpoint pending on` and full paths
5. Inspect variables, step through, find the real problem
6. Only then make the fix

Stop adding printf statements. Stop guessing. Run gdb.

# Release packaging

`silver --release <app>` tests, then packages, from the same staged tree on
every platform: the host exe, the ELF/Mach-O dependency closure from the
install tree, `share/<app>`, and the icon set (img's `export func icons`
scales the module's `images/icon.png`). Outputs land in `<repo>/packages/`,
never in `install/`. Linux writes `.deb`, `.rpm` and Arch `.pkg.tar.zst`
itself — no dpkg-deb, no rpmbuild — with the distros' native layout
(`/usr/bin/<app>`, `/usr/lib/<app>/`, `/usr/share/<app>/`). macOS writes the
`.app` and a signed, notarized `.dmg`. Windows is next, sharing the staging.

## iOS / iPadOS (idea, parked — mac first)

Nearly all of it can be one silver command; only Apple's account gates stay:

- `silver -d iphone app` — build for `arm64-apple-ios` (a `platform/ios`
  sysroot like `win`), MoltenVK for the vk path, a small UIKit host in place
  of GLFW (`UIViewController` + `CAMetalLayer`, touches → trinity press/
  release), the app module linked statically (iOS forbids dlopen of
  unsigned code), then `xcrun devicectl` to install, launch and stream logs.
- `silver --platform ios --release app` — a flat `<Name>.app` (Info.plist
  with `UIDeviceFamily [1, 2]` so one binary serves iPhone and iPad, icon
  sizes from the img export, launch screen, `embedded.mobileprovision`),
  signed with the keychain's `Apple Development`/`Apple Distribution`
  identity and the provisioning profile from `~/Library/MobileDevice/
  Provisioning Profiles/` whose bundle id matches (entitlements pulled from
  it), zipped as `Payload/<Name>.app` → `.ipa`; `--upload` hands it to
  TestFlight via `xcrun altool` with an App Store Connect key stored once in
  the keychain, the way notarization credentials are stored today.
- One-time and unavoidable: a developer account (free = own devices, weekly
  profiles, made via `xcodebuild -allowProvisioningUpdates` on a stub
  project; paid = TestFlight/App Store), certificates + App ID in the portal,
  App Store review for public release, a Mac.

Order when picked up: sysroot + MoltenVK + UIKit host (a trinity app drawing
on the phone via devicectl), then bundle/sign/`.ipa`, then upload.

# State as of 2026-08-28 (knes regression, iOS, devices)

knes emulation is ~2x slower than it should be and it also crashes; `--record`
/`--playback` (one pad byte per emulated frame, knes.ag run_frame) drift out of
sync, most likely because dropped frames desynchronize the input stream. Not
fixed. vscale/filtering was ruled out (slow at low res too). Facts measured:
- validation layer was on in every debug build (vk.ag) and made every submit a
  full stop; now opt-in with VK_VALIDATION=1. That alone took knes 30 -> 60 fps
  on the title screen, but it still sags to ~45 and bursts later in play.
- `Command.submit` no longer vkQueueWaitIdle; each Command owns a fence waited in
  begin/dealloc. `Texture.upload` keeps a persistent staging buffer + command,
  barriers in band, no host wait (vk.ag). Mip textures still wait.
- Sampled (no instrumentation): main thread ~68% in Render_sync_fence
  (vkWaitForFences on the frame ring), emu thread ~40% busy. GPU/present side,
  not the 6502/PPU, is where the time goes. Not resolved.
- `silver --timing app rom` works again (ids shared across cores, names filled
  before emit); its per-call clock_gettime inflates hot tiny functions.
- Process isolation (silver-host) is now spawned correctly on macOS
  (/proc/self/exe was linux-only). SILVER_ISOLATE=0 runs in process.
- CLI: `silver [flags] module [app-args]`; app args after the module. Au's
  Au_with_cstrs stops parsing at the app's own positional (rom), so
  `rom --vscale false` never reached knes; a fix is edited in src/Au.c
  (positional_done) but NOT yet rebuilt/verified. string_cast_bool now reads
  false/no/off/0 as false (Au.c).
- silver bug: a `@FILE` class member cannot be null-tested (locals can).
  knes uses `handle` members as a workaround.
- devices (GLFW replacement) has a DEVICES_SCALE env override on mac;
  trinity takes scale from platform_window_scale.
- spectra was split: audio core stays, whisper/tts/datasets/codecs moved to
  `speech` (orbiter, crashman import it). mpg123 removed entirely.
- iOS: `silver -d iphonesim app` builds, bundles, installs and launches in the
  booted simulator with logs streamed back; `-d iphone` builds and signs (never
  install/launch on the phone unless user asks). Shaders compile in-process
  via glslang (no glslangValidator binary). devices.agi holds both devices.
- Homebrew is gone; ninja/autotools/swig are built into install/ by bootstrap.
- Linker warnings fixed generally: objects pin macosx13.0; no shared libunwind.

## Active work: Codex app communication

1. DONE Forward per-edit diff lines, including deleted files.
2. DONE Forward final reply text; route hooks by session/turn.
3. DONE Preserve sender; explicitly report queued delivery.
4. DONE Preserve file references and screenshot image instructions.
5. DONE Poll queue delivery; timeout and show errors in app.
6. DONE Correct hook documentation; 16 hook tests pass.
7. DONE Preserve explicit sessions when the picker selects Codex.
8. DONE Real Orbiter screenshot reached this session; reply
   appeared in the app tree before the test app was stopped.
   UI timeout/retry/reply test passed.

Codex hook handlers pass their tests. This existing session did not
create a prompt-hook binding; automatic hook activation still needs
verification after the repository hooks are trusted through `/hooks`.

9. DONE Desktop executable lookup verified with a minimal PATH
   and no CODEX variables. The real editor message reached this
   session and its reply appeared in Orbiter before test shutdown.
   Eight native tests and connection tests pass.

10. DONE Optional expandable descriptions on message titles.
    Description and connection tests pass; 17 hook tests pass.
    Trinity and Orbiter build. Socket tests verify expansion,
    collapse, plain titles, and descriptions after completion.

## Active work: orbiter live reload crash (Sep 21 2026)

1. OPEN Auto reload crashes orbiter. Reproduced headless
   (touch orbiter/Editor.ag, `silver --build orbiter`). Two ways
   it dies, one cause: the old instance is not freed on reload.
   - Memory footprint 1783 MB -> 4264 MB -> 6956 MB over two
     rebuilds; one rebuild fires TWO reloads. After 4-5 reloads
     the GPU is out of memory and the Vulkan device is lost.
   - Intermittent SIGSEGV in Au_free (Au.c:6354,
     `cur->ft.dealloc`): a surviving object's type descriptor
     points into the unloaded old orbiter image. The junk
     address (0x23000000010xxxxx) is an unfixed on-disk pointer.
   - DONE `launch_spec_of` built a second full orbiter to read
     props; its git thread was never joined and crashed in
     `git_capture` at dlclose. It now reads the export line only,
     by owner name (`install_name_of`).
   - Holders found with `--leaks`: the old orbiter sits in a
     cycle through its paused watches' callbacks, and on_unload
     holds both avatars on purpose (Texture_release_gpu crash).
   - Two reloads per external build: the host's product watch
     sees the library mid-link and again when the link ends.
   - Not fixed: the old instance is still not freed per reload.
2. DONE Post-reload SIGSEGV (garbage map in FileIcons_icon, "style:
   unknown type" for every orbiter class). Cause: the host's product
   watch fired while the linker had the product unlinked (mtime 0),
   reload_dlopen's copy failed and its fallback dlopen(product path)
   returned the SAME old image: no constructors, no module. Host now
   ignores mtime 0, never falls back to the product path, and treats a
   same-handle result as a failed load (retry). Also the double reload
   per build.
3. DONE `persist` keyword: `persist x : T` is a class static (IS_STATIC
   | IS_PERSIST, interface.persist = 11) emitted as global <Class>_<x>
   with default visibility. Before a reload au_persist_save (Au.c)
   holds every non-null slot; au_persist_load puts them in the new
   image's slots before its init. T (and a map/vec element type) must
   come from a module the reload does not swap: the parser rejects a
   type of the module itself, so the old image is dlclosed as before.
   Avatar.mcache (GltfModel) and Avatar.envcache (Texture) persist:
   the reloaded avatar rebinds to the loaded model and environment.
   Verified headless, 2 slots kept per reload, 2 reloads per run.
   - aether: an inherited class static was emitted once per subclass
     (AgentAvatar_mcache ...) and the shared evar pointed at the last
     one; now named by the declaring class only.
   - Au_drop_members walked class statics (offset 0): one extra drop
     of the instance's first member per class-typed static. Skipped
     now, as hold_members already did.
   - devices.mm: SILVER_HEADLESS registers as an accessory app (no
     dock icon, no focus steal), like a hosted slot.
4. DONE DOUBLE-DROP in the reload destroy: a lambda's `au_t` (a type
   descriptor) was held/dropped by the member walks like an owned
   object. Au_t-typed members are skipped in both walks (as ARef is).
5. DONE Parallel reload (Sep 22): the new image loads and inits on a
   worker thread while the live instance keeps framing; the switch on
   the main thread is 1-4 ms; the old instance's destroy, worker wait
   and dlclose run on a close thread. First owned frame 220-320 ms
   (swapchain creation + first present), then normal. Headless, three
   cycles clean under lldb.
   - host: dlopen -> persist save/load -> worker init (an Au space with
     `au_space_capture`, so its modules register there; `au_space_
     detach`/`promote` move them to the global list at the switch,
     after the old module is erased by pointer and its image purged).
     A failed load keeps the live instance. `SILVER_RELOAD_PARALLEL`
     marks the worker's init.
   - aether: the emitted destroy erases its own module by descriptor,
     not by name; `module_erase_silver` is no longer called there.
   - trinity: `media_app.run` under PARALLEL sizes and draws its
     targets but defers the swapchain (`Display.defer_swap`); `frame`
     takes it once the host clears PARALLEL. vk_context is shared by
     the two overlapping instances, so: `qlock` around every submit,
     present and queue wait; `fence_acquire` returns the fence under
     it; a per-thread command pool (`vk.pool[]`) and transfer command
     (`vk.xfer[]`) - Vulkan forbids two threads on one pool; Command
     `lk` serializes begin/reap/submit against the frame tick; the
     pending list swaps under qlock; `defer_flush[ mod ]` releases
     only the dying module's deferred drops and shader-cache entries;
     the font cache is never cleared (trinity types, keyed name@size).
   - Au: `Au_drop` leaves an object still in another thread's pool to
     that pool's drain (a foreign free left a dangling pool entry).
   - The exchange fades on alpha before the swap: `element.fading`
     (a `:fading` style state, 600ms cubic outward) on every exchange
     element and the AgentAvatar; `Window.swap_fade` sets it when the
     app requests the apply, and clears it if the build fails.
     Verified headless: all seven elements transition to 0.0 at
     "apply requested", before the recompile and the switch.
   - The new Window's input callbacks attach at the switch
     (`Display.attach_input` from `take_swapchain`), not at its init:
     on the shared platform window they took the main thread's mouse
     events into the half-built tree (SIGSEGV in Editor_on_move).
   - The host keeps pending at 2 from the good recompile to the switch
     (was 0 while the new instance loaded): orbiter's 4 s stand-down
     saw pending != 2 and cleared swap_fade, so the exchange faded back
     in right before the cut. `set_pending(1)` on a failed load.
   - A relaunch is needed after host or trinity changes; a reload
     swaps the app library only.
   - FIXED (Sep 22 23:12) reload crash in vk_context_xfer (Kalen's
     runs, -O2 real window): two bugs. (1) trinity.ag media_app.destroy
     had grown `hr9.managed = 1` on the app element again (the change
     Kalen reverted before): freed the process-owned app element, a
     DOUBLE-DROP from the map listing it. Removed. (2) Image.user is an
     Au-typed slot (never held/dropped); Gpu.sync cached the sampler
     Texture there raw, owned only by the Gpu's tx. The diff's
     `resources.clear[]` on rebind now really frees that Gpu, the
     Texture with it, and the next Gpu.sync held the dead texture from
     img.user (found with AU_QUARANTINE=1: DEAD-USE in hold, Texture at
     vk.ag:2486; -O2 reused the memory after the old instance's
     teardown: GaiaShader's scene target faulted). Fix: the Image owns
     the cached texture (Au.hold at Gpu.sync and environment, drop in
     Image.dealloc), and the Texture nulls its sampler after upload
     (no cycle). Verified by Kalen: two reloads, backdrop stays.
     Diagnostic still in: the DEADTX guard in Pipeline.draw_range
     (skips and logs a dead resource texture); remove once trusted.
   - Fade at ready, not at apply (Sep 22 09:00): the host sets pending
     3 once the new instance is built; orbiter fades the exchange on 3
     and hands the go (`au_live_request_apply`) 650 ms later; the host
     switches on the go or after 800 ms. Pending changes restyle the
     status bar. The flame burns from done to the switch.
   - Close-thread leftovers: objects the teardown dropped to zero while
     in the main pool are freed by the main drain, so the close thread
     waits for one main drain (`close_job.destroyed/drained`) before
     dlclose. `defer_flush` releases every deferred list at the flush
     (a deferred Model's dealloc reaches an orbiter-typed shader); the
     defer lists are swapped under `qlock` on both threads.
   - The veil (Kalen's spec): the exchange's blur from BEFORE the
     reload crosses the swap (`persist swap_blur : ReduceBlur`, set in
     request_swap by the instance that asks, taken by the next one on
     its first frame into `Window.veil_blur`). The reloaded window
     mounts it as a `swap_veil` ShotBackdrop on its second frame, and
     once its first fully loaded frame has rendered (`loaded[]`) sets
     `fading`: the style's 600 ms transition takes the old blur to 0
     over the NEW screen, and the window drops it at 0 (`veil_end`).
     Verified with shots: old blur -> mid-fade over the new editor ->
     sharp. A window that blurred its own frame gave black (the frame
     had not drawn yet) - never do that.
   NEXT (Kalen): trinity as the host - a "trinity app". The Window,
   device, swapchain and a Presenter live in the host and never
   unload; app instances render into host targets; the switch is a
   target blit with a crossfade from the old instance's last frame;
   silver-host.c's duties (watch, rebuild spawn, isolate, slots, log
   tee, crash) port to trinity.ag; one host binary for every app.

## Active work: reload leaks and crash (Sep 23 2026)

Test: `support/reload-leaks.sh [reloads]` (headless orbiter-dbg; BIN=orbiter
for -O2; AU_QUARANTINE=1 for use-after-free; CENSUS=1 keeps the log and
writes a per-type census; LEAKS=n + AU_LEAKS_EACH=1 [+ AU_LEAKS_TYPE,
AU_LEAKS_LINE] adds a leak report after each reload). Fails on sampler
growth, object growth over 0.5%, or a fault in the log.
State: samplers flat (88), objects +29 per reload, passes on debug,
debug+quarantine and -O2. Startup 71k -> 55k objects.
1. DONE parser: parsed maps held twice (parse_object, agi, construct_with
   map path); construct_with props now raw (hold_members owns them).
2. DONE aether: stores in a construct are raw like init (post-construct
   excluded); call_construct ends with hold_members.
3. DONE cycles: StyleEntry.bl, StyleBlock.parent, StyleQualifier.bl manual.
4. DONE double holds: svg cache, frost chain, scene targets, scene element
   and name, GitSnap, swap shader, Render framebuffer/render-pass vecs,
   SDF edges, SVG shapes (construct makes them; load reuses).
5. DONE vertex_member_t kept an Accessor (a struct in a vec never drops):
   it holds the accessor index now.
6. DONE shader cache: eviction moved to module_erase (au_module_erase_hook):
   defer_flush was handed the trinity module and evicted trinity's shared
   shaders. A cache hit holds its owner (shader.mod_src).
7. DONE attrib values dropped at module_erase; close thread drains its pool;
   deferred releases flushed after the tree and Window drop.
8. DONE session restore raw during init; state_persist_load's scratch
   object marked owned so its teardown drops the parse.
9. DONE per-owner command pools (Render ring, Command): the close thread
   freed buffers from the main thread's pool (MoltenVK reset spin).
10. OPEN +29 objects/reload: list items orphaned by a list removal
    (list_push hold, no holder), a few parser strings, vector_of vecs.
11. OPEN the app element itself (managed 0) outlives its image by design.
12. OPEN intermittent silver compiler SIGSEGV during the host's rebuild,
    only with the app running; silver now prints its backtrace on SIGSEGV.
    Caught once (Sep 23, `silver --build orbiter`, no app running): in
    LLVM EarlyCSE (ConstantFoldLoadFromConstPtr) under optimize_module on
    a parallel emit_job_run thread; the retry built.

## Active work: editor embedded pane (Sep 23 2026)

1. APPLIED, awaiting Kalen's run: an embedded app whose build
   failed (ended) still took the pane's live-frame paint branch
   (EditorLayer.draw); it now needs a live app (not ended, not
   held), as tick's `live` and the input paths already did.
2. APPLIED, awaiting Kalen's run: the scrollbar hid whenever the
   source showed with app_view on (a stop, a failed build).
   no_scroll is now cleared whenever the source draws and set
   when the live app frame paints.
3. DONE compile-error band at the editor's foot: indent-header
   look in red, counted in the scroll range, click goes to line.
4. DONE spectra no longer imports trinity. The app hosting
   channel (slots, rings, shared textures, the sound ring) moved
   from trinity.cc to devices.c, its declarations to devices.ag.
   A module calling host_* imports devices (orbiter, crashman):
   a cached import does not pass its libs on to the app's link.
5. OPEN silver bug: a top-level `intern func` declared before an
   `ifdef [ apple ]` C header import loses that header's types
   (spectra: AudioQueueRef unknown). Declare after the imports.
6. DONE features builds and runs its expects again: a .hpp/.hh/.hxx
   import is C++ (outside extern "C"); the rust companion's import
   is a C header (no Au headers); a lambda context struct is made
   once and its capture fields take the captured variable's type.
   t_log expects the bracketed 16-wide log stamp.
7. DONE vec conversion: `b = a` between vectors of different element
   types makes a new vector (Au `vector_convert`): primitives convert
   by value, class elements by the source's cast, else the target's
   constructor. features t_vec_convert, _cast, _ctor pass.
8. DONE lambda contexts made during codegen have no type id: they
   were allocated as a zero-size Au and captures overran it (async
   inline lost its writes). They are allocated by byte size now.
9. DONE features t_try_finally_rethrow: a local written before a
   longjmp was lost (setjmp returns twice). In a function that calls
   setjmp, loads/stores to its allocas are volatile (aether_emit).
10. DONE async race: a worker checked `next` unlocked before its first
   job; a sync-all could null it first and the job never ran
   (t_async_inline intermittent). The first job always runs now.
11. DONE Au `check` raises into an enclosing try (t_check); with no try
   it still prints and returns false.
12. DONE features t_type_macros. C/C++ header macros that name a type
   (`#define cpp_int int`, `cpp_intp int*`) become aliases at import
   (aether_macro_type, called from aclang.cc; C keyword types map to
   silver primitives; the alias carries its size). In cmode `?:` binds
   to the whole expression, not to a parenthesis before it.
14. DONE map_clear left the hash table pointing at the items it freed; the
   map's dealloc cleared them again (SIGSEGV in features t_mem_last).
15. DONE features t_mem_last, all 168 expects pass (19 live at start
   and end): engage always records the launch dir and cd's to share
   (was only with argv), and the expect runner engages before the
   tests; vector pop/shift hand a class element back to the pool
   (au_release) instead of leaking the vector's hold; a byte-allocated
   lambda context's object slots are named in lambda.ctx_objs and
   dropped at lambda_dealloc; t_argv and t_self_a_header drop the
   holds they take.

## Active work: MoltenVK video encode, then orbiter find (Sep 23 2026)

1. DONE VK_KHR_video_queue, video_encode_queue, video_encode_h264,
   video_decode_queue and video_decode_h264 in MoltenVK over
   VideoToolbox (MVKVideo.h/.mm, MVKCmdVideo.h/.mm). In
   trinity/MoltenVK.diff. Upstream: KhronosGroup/MoltenVK pull
   request from ar-visions:video-encode-decode (on main).
   - One queue family does encode and decode. H.264 only, 8-bit
     4:2:0 NV12, progressive; VideoToolbox keeps the references.
   - Bitstream buffers are host visible: encode writes them, and
     decode reads them, on the CPU. Decode runs when the command
     is encoded to Metal, then a blit copies the picture in.
   - Decode rewrites the std SPS/PPS as H.264 bytes for
     VideoToolbox; nal_ref_idc is 3 (the std has no field).
   - DPB images may have array layers (trinity uses 2).
   - Verified: orbiter --record (with --record_mic) plays back;
     the scratchpad test vkdecode.mm decodes the recording, a
     Main B-frame clip and a Baseline clip through
     vkCmdDecodeVideoKHR, every frame bit-exact with AVFoundation.
2. DONE orbiter: the remotes switch shows in the finder too (not
   in a resource pick), and index_search skips files outside the
   project root while it is off, as find-in-files already did.
   Verified headless: finder "orion.ag" and find-in-files
   "class Race" list ~/src/orion only with remotes lit.
3. DONE recorder magenta blobs and torn text (trinity/video.ag
   Recorder.capture). The nv12 compute read the scaled copy before
   the copy finished: Layout's SHADER_READ wait is fragment only, so
   a transfer-to-compute barrier now precedes the dispatch. And the
   capture waits for its own submit (new Command.finish): the next
   frame drew over the screen while the copy still read it.

## Compiler coverage (Sep 23 2026)

Live list:
1. DONE coverage runs reset features first (--uninstall with the
   instrumented silver): its checkouts (zlib, fribidi), builds and
   products go, and the run fetches and builds them again.
   silver.c 65.76% -> 67.78% lines. Only silver.c and aether.c
   are reported.
2. DONE silver.c holds only what features exercises: 1,346 lines
   of devices, ios/android bundles and apk writing, cross
   toolchain files, device runtimes, platform triples, release
   version, dylib bundling and deploy_resources moved to
   src/parts/deploy.c, included once inside the tail
   `#ifdef BUILD_LIBRARY` of silver.c (same unit, statics kept).
   It is not in src/ itself: gen.py makes every src/*.c a module.
   silver.c now 77.78% functions, 75.24% lines, 34.06% branches.
   Still in silver.c and never run by features: live reload,
   listen, lldb, the AI codegen classes, verbose printing.
3. DONE tests from a manual read of silver.c's untaken branches:
   features 182 -> 199 expects, all pass, live objects 26 at
   start and end. silver.c 75.83% lines, 34.39% branches.
   Fixed by them: `pct [ 50.0 ]` (a scalar in brackets) was 0 -
   parse_object put the value on the scalar's (no) fields; the
   object header's data_shape was never dropped (Au_dealloc), so
   each shaped vec leaked its shape. t_mem_last moved to the end:
   tests after it never ran.
4. OPEN tests that need features' own files: Launch/Live member
   meta, member meta B, export forms, import defines via a
   features/flags.h, several `>` lines, `with A, B`.
5. OPEN a scalar's own `cast -> string [ '{a}%' ]` recurses
   forever (a is the scalar); the test writes f32[ a ].
6. DONE verify/validate/error/fault leave one branch per use:
   the body is one function (Au au_verify_fail and au_fault,
   aether_fail, silver_fail) over Au's new vformatter (va_list).
   silver.c branches 34.39% -> 56.12% (19,643 -> 12,053
   outcomes), aether.c 49.95% -> 56.22%. The failure side of
   each use is still counted and never taken in a good build.
7. DONE support/coverage-branches.py (run by coverage.sh on the
   llvm-cov JSON export) leaves out those failure sides. clang
   puts a check's branches in its macro expansion, at the
   #define: the whole condition on `(cond)` (false = failure) or
   `!(a)` (true = failure), each &&/|| piece on the bare param
   (kept: real decisions), message-argument branches elsewhere
   (dropped: they run only on failure). Counts come from the
   function records: llvm-cov's own report folds each macro use
   into per-line outcomes, so its totals differ. silver.c all
   52.82%, real 54.33% (323 simple checks); aether.c 49.60%,
   real 50.18% (122).
8. DONE tests for the untested language features: features
   199 -> 211 expects, all pass (live 28 at start and end).
   silver.c lines 75.72% -> 76.86%, real branches 55.70%.
   New: features/feat.h (with feat_add in features.c) and an
   import config block (-D, { (define) ?? }, +NAME=value).
   Fixed by them:
   - a C typedef of a fixed array (typedef int t[3]) could not
     be filled from a list or indexed: read_enode and e_offset
     read elements off the typedef; e_assign stored the temp's
     address into the array slot (fixed_array_of, array_slot).
   - `'{m["a\"b"]}'`: single-quoted strings unescaped their
     {expr} too; unescape_interp leaves braces as written.
   - C macro bodies were tokenized as silver (cmode off):
     "a" "b" never joined, ## and # were not C. aclang's
     c_tokens sets cmode; the join also built the wrong text.
   - `[ {expr}: v ]` used the name as the key: parse_object
     ate the { before parse_field looked for it.
   - `validate(n, ..., next(a, ...))` consumed a token.
   Unreachable, no test can run them: typed_expr (its caller in
   read_enode follows a branch that always takes the [), and
   parse_object's non-field paths after its positional loop
   (collective/prop_value_at keys, stride check, iarray).
9. OPEN `@pt2 [ x: 1.0, y: 2.0 ]` (a ref to a new struct) reads
   x as a declaration; unclear if the form is meant to work.
10. OPEN left untested: `using <codegen>` (needs an AI
   backend), calling a cast member by name, tab-indented
   continuation lines.
11. OPEN an inline lambda does not capture a name used only
   inside an interpolation: `lambda [ t: Task ]` with body
   `log '{mark}: {t.title}'` fails "expected member, found
   mark"; `word : string [ mark ]` in the body works.

`support/coverage.sh` also exports the compiler's map to
install/tmp/coverage.lcov (llvm-cov export -format=lcov), so
orbiter shows it on src/silver.c and aether.c. It deletes the
module's install/build/*-<m>.product to force the compile: a
touch let a watcher (orbiter) rebuild first, and the run then
measured almost nothing (8%).
`make coverage` builds into install/coverage alone: aether,
aclang, silver and silver-lib get -fprofile-instr-generate
-fcoverage-mapping, their libraries stay beside the binary
(rpath @executable_path first), install/lib and install/bin
are untouched. `support/coverage.sh [module]` runs the module's
expects with it and writes a report and install/coverage/html.
LLVM_PROFILE_FILE uses %c: silver execs the test app, so the
counters must already be on disk.
First run, features 167/167: functions 71%, lines 69%,
branches 40% (silver.c 32%, aether.c 49%, aclang.cc 71%).
OPEN: one coverage run reported features failing; the output
was filtered and three reruns passed, so the test is unknown.

silver's own coverage: `silver --coverage [--test] <module>`
writes install/tmp/coverage.lcov (SILVER_COVERAGE_LCOV) from
Au's __coverage_report (end of the expects, exit, SIGINT,
SIGTERM). One map: the last coverage run's; a run deletes it
as it starts.
Each block probe records file, first and last line in a root
table (coverage_probe_open/close); finalize_coverage_map emits
it after the cores; __coverage_lines hands it to Au. A line
takes the count of the smallest block holding it; blank and
comment lines are left out. `aether_init` no longer resets
`coverage` (the flag never worked before), and probes in a
core module declare the root's globals (cov_global).
Checked: a small if/el module marks the untaken return and an
uncalled function 0; features 167/167, 1580 of 1636 lines.
Limits: 4096 probes a module, only the root module is
instrumented, and `el` counts with its enclosing block.
Probes go only on blocks inside a function: a class body never
runs, and its probe marked every declaration line red.
A block starting past its line's indent (a one-line if body)
shares the line: it counts as run if its statement ran.
Probe files come from the function's source_file (a token's
source is weak: it crashed on a codegen thread).
orbiter: a `coverage` row under `debug` in the run panel
(AppView.cov_build, launch bit 1<<16 -> silver-host
HOST_APP_COVERAGE -> --coverage and SILVER_COVERAGE_LCOV) and
`clear` (deletes the map). cov_poll (1 s tick) reloads the map
on a change; each pane shows a green dot per run line, a red
dot and a faint row tint per missed line. Checked headless:
dots on features.ag, the live reload, clear. Not driven yet: a
coverage launch through silver-host (headless has no host).

Coverage-driven tests (Sep 23): features +8 expects (175/175):
t_expect_plain, t_format_suffix, t_for_index_key, t_for_array,
t_spaceship_kinds, t_switch_i64_default, t_typeid_of_value,
t_fixed_array_ops. Compiler lines 69.13% -> 69.72%.
FIXED aether_e_cmp: integer <=> subtracted and narrowed to i32,
so it gave the difference (u32 7 <=> 3 was 4) and lost the sign
for wide values (i64 0 <=> 2^32 was 0, equal). Now signed or
unsigned compares give -1/0/1, like the float path.
No callers, so no test can reach them (dead-code candidates):
aether_e_eq_prev, e_inherits, e_is, e_coalesce_deferred,
e_meta_ids, e_abs, e_inc, e_memcpy, e_materialize,
silver_read_bool, parse_expect, etype_infer.

## Codegen through the user's agents (Sep 24 2026)

`import claude` / `import chatgpt` (codegen classes in silver;
config lines set `model:`), then `func f [ .. ] -> T using
claude` with `{ prompt tokens, {images/x.png} }` under it. A
{path} group is one token whose literal is the path.
1. DONE bodies live in gen/ beside the module source:
   gen/Class.method.ag or gen/method.ag, written as
   `extend <module>`, `# hash: <key>`, the func line, the body.
   The func line must match the declaration (whitespace aside).
2. DONE the hash is a cache key: the request carries P (agent,
   signature, prompt, image bytes); the agent writes
   `# hash: P`; on acceptance the compiler stamps K = key(P,
   body). A build recomputes K: a match uses the file (commit
   it; builds need no agent); a mismatch deletes the file and
   regenerates. Editing gen/ rebuilds; so does removing a file.
3. DONE generation runs the user's agent once, from the shell,
   in the repo folder: `claude -p <request> --permission-mode
   acceptEdits --no-session-persistence [--model m]`, or
   `codex exec -s workspace-write -C <repo> --ephemeral
   [-m m] <request>`. No mailbox, no sessions, no API. The
   request is the source location, the gen path and the three
   header lines. One at a time; killed past
   SILVER_CODEGEN_TIMEOUT (600 s); output in
   install/tmp/codegen-<func>.log. Programs from PATH, else
   ~/.local/bin, ~/.claude/local, the ChatGPT/Codex bundles.
4. DONE gen/'s subfolders are resource folders.
   Stand-in tested (fake claude/codex on PATH): both write and
   stamp, the app runs, a rebuild is up to date, a body edit
   or prompt change regenerates.
   REAL (Sep 24): features t_codegen, gen_add using claude
   (`claude -p`, model claude-opus-5-5) and gen_mul using
   chatgpt (`codex exec`); both wrote features/gen/*.ag as
   asked, were stamped, and features passes 212/212 with no
   agent on rebuild. `model: 'gpt-5'` is refused for Codex on
   a ChatGPT account; features' chatgpt import sets no model.
5. OPEN gemini still errors "not implemented".
6. DONE `using` on export funcs (module level only, as export
   is), on expect funcs, and on methods, including expect
   methods inside a class. The header carries the keyword
   (`export func`, `expect func Board.t`); `[]` for no args.
   The request names LANGUAGE.md, AGENTS.md and
   features/features.ag; `claude -p` gets `--add-dir <silver>`.
7. DONE the expect runner now runs tests declared inside a
   class, on a new default instance, named Class.test, in
   source order (they were compiled and silently skipped).
   features: Tallied.t_class_test; 213/213.

## Active work: shell-mode exchange (Sep 24 2026)

1. APPLIED, awaiting Kalen's run: after a live reload the
   exchange closed. orbiter set swap_fade once the reloaded
   frame loaded and trinity closed the exchange at 0; that fade
   is gone (the old blur's swap_veil still fades on its own).
2. DONE the diff after the agent's edit: its reply's fenced
   ```diff block became note rows, and the edit's own diff
   listed every old line - and every new line +. shell_text
   leaves out ```diff blocks and fence lines; shell_edit_diff
   shows the lines both sides share as context.
3. OPEN a line the agent added at the top draws blank until
   selected or scrolled out and back: the new row is not
   redrawn after the disk reload / live reload.
4. DONE (Kalen confirmed): the PBR ring light (`pl_lit`,
   Avatar.ag) turned the other way and sat on the far side
   from the plasma ring; its angle is mirrored across `pl_tg`.
   Earlier on this item: the avatar's gradient rim. orbiter32.gltf COLOR_0 is soft (839 values on
   Cylinder.001) and 8,000+ triangles span b=0 to b=1, so the
   color blends across them. The two soft reads are now on/off
   at 0.5 (Avatar.ag): the core light (`emit * v_color.r`) and
   the groove glow mask (`v_color.b * (1 - v_color.r)` with its
   gamma and smoothstep). The other reads were already on/off.

## Active work: one exchange box (Sep 24 2026)

1. APPLIED, not built (build needs approval): one box for the
   user's lines and the agent's (Window.notes, ShotEntry.user;
   ShotNotes removed). After the first send the box fills the
   right side; the field and mic are its bottom row. Text 14 ->
   18 px, diff 12 -> 15, prompt 16 -> 18. User lines white,
   agent lines blue.
2. APPLIED, not built: the wheel scrolls the box
   (Window.notes_scroll, px up from the newest; a new entry
   brings the newest back into view).

## MEMORY: the reload transition (Sep 22 2026, fixed)

THE REQUIREMENT (Kalen, said many times, do not reinterpret it):
the avatar and the exchange do not disappear at a reload. The new
instance RECREATES them instantly, in the exact state they had,
and keeps drawing them; the old blur fades out to the NEW screen
with the avatar and boxes still there. Nothing is "the same
object"; it is recreated fast from carried state. Timers start
when the first fully loaded frame has rendered, not at apply.

HOW IT WORKS NOW (verified headless with shots, 7 cycles):
- trinity `ExchangeState` (shot_ask/file/line/text, agent_view/min,
  the two note vecs, agent_pick, the blur, the avatar's halo/flame/
  tilt/listen/spin/settled/shown/done), `Window.exchange_carry/
  restore`, `snap_transitions` (every transition lands on the
  restore frame), the exchange closing itself when shot_bg reaches
  0 under swap_fade. All `:fading` styles are 600 ms.
- orbiter `persist swap_carry : ExchangeState`, filled at pending 3
  (exchange + avatar carry), then the go.
- `element.on_frame [ w: Window ]`: the app's word each frame BEFORE
  the window composes; `Window.on_compose` calls it on the app
  object first. orbiter restores there (guard `!swap_asked`), so
  frame one after the switch composes the full exchange. A restore
  in `render[]` or `tick` was one frame late: trinity's app_render
  reads shot_ask before the app's render runs, and the exchange
  then mounted on frame two already fading (never seen at full).
- Avatar `paint_once` (set by restore) and `fresh9` (made this
  frame): the target renders once inside `draw` (update, draw,
  sync_fence) before its first paint. The frame's element_targets
  pass (vk.ag ~4810) runs before on_compose and skipped the avatar
  at opacity 0, so the compose painted its empty image: a magenta
  square on frame one.
- The fade starts in tick the frame after the restore, once
  `ux.loaded[]`: `ux.swap_fade` sets `fading` on every exchange
  element and the avatar; shot_close at 0. Every `:fading` is
  3000 ms (Kalen: long enough to judge by eye).
- `agent_av_shot` is set at the restore: orbiter's `agent_avatar[]`
  takes a shot_ask it has not seen as a NEW capture and resets
  `settled`, which put the restored avatar at the fresh-exchange
  top-center start and slid it left over 900 ms (Kalen's report).
- OPEN, one frame: the user's box frame (ShotUser 'frame' layer)
  draws at its base one-row height on the restore frame with the
  entries above it; right from frame two. `ShotUserLayer.draw`
  sizes the frame layer during its own draw and marks a repaint,
  so the frame lags the entries by one frame by construction (a
  fresh exchange has the same lag as each entry arrives). A fix is
  the frame height at layout time (text measure outside a paint).
- Drive: headless orbiter (`XDG_RUNTIME_DIR=/tmp/claude-501/ob8`,
  a long runtime dir truncates the socket name), ctrl+shift+S, drag,
  Enter, `app status busy`, touch orbiter/Editor.ag, `app status
  done`, wait for "reload complete" in the host log, `shot` burst.
  With `text carry check` + Enter before busy, the avatar is settled
  (left) and both boxes have entries. Pass: frame one = the exchange
  as it was, avatar at its settled place with its flame; then fading
  over the new screen for 3 s.

## MEMORY: GPU memory across reloads (Sep 22 2026, evening)

Symptom (Kalen): 4-5 live reloads and orbiter dies: Metal "Insufficient
Memory", VK_ERROR_OUT_OF_DEVICE_MEMORY, a SIGBUS in a MoltenVK submit,
or a strncmp SIGSEGV in apply_style on a freed string. Measured
headless: every reload left one instance's worth of textures alive
(about 190 samplers), and one exchange open+close leaked 17.

Instrument: `vk.samp_tags` / `vk.samp_keys` count live samplers by
the texture's `tag`, else by its creation `file:line`; the stats
line (`stats` over the app socket) prints them after `| tags`. The
blur chain's renders carry `tag: 'blur'`. Under `--leaks`, the exit
report walks holders ("held by X via .m"; PHANTOM = a hold no live
object accounts for; "unbalanced hold:" names its call site) and
`au_leak_sites(Au)` (Au.c) prints one object's recorded hold sites.

The rule (Kalen): a store in `init` does not hold; `hold_members`
after init owns the slots. A store in a method that init CALLS
(a delegate) does hold, and hold_members holds again: a double hold
that leaks the object forever. Delegate calls handle it manually;
never patch aether for it.

Fixed, all verified headless (3 exchange reloads, no crash):
- Render: the color/depth/reduction textures are made in `init`
  itself (sized from wscale as update does); `update` only resizes.
  Before: made in `update` from init, held twice, never freed.
- Pipeline: `resources`/`storage_textures` are made in `init` before
  `bind_resources` (its delegate), which now clears or lazily makes
  them instead of re-storing. Before: every pipeline's resource
  vector (Gpu -> bound textures, buffers) was a phantom root.
- Pipeline.dealloc destroys its descriptor pool and the two set
  layouts; Render.destroy_ring destroys the timestamp query pool
  (Metal has a small fixed number of counter sample buffers; the
  "Could not create MTLCounterSampleBuffer" flood was that).
- NOT changed (Kalen's recent code, kept): the five explicit
  `.hold[]` in build_frost_chain and the `mdl.cache.hold[]`/`drop[]`
  pair in Canvas.draw_svg. Removing the frost holds crashed the
  live app on its first reload (vk_context_xfer from rewire_frost
  after the switch, a freed chain object); the svg pair is why
  `svg` grows 38 per reload (icon cache canvases never freed:
  SVGModel.cache is public, the store holds, the explicit hold
  stays at the model's dealloc). Both are open, with Kalen.
- REVERTED (Kalen): touching the app element at destroy. It is
  process-owned (managed 0) and stays as it was. `managed = 1` +
  free gave DOUBLE-DROP from a map that still listed it; then
  `app.drop_members[]` crashed the next instance's first draw
  (Display_draw -> Texture_transition -> vk_context_xfer): objects
  the old app element owns are still used by the new instance
  through unheld context pointers. Do not free or drop-walk it.
- orbiter.on_unload: paused index watches drop their callback (the
  lambda's context held the app object; `watch_pause` drains the
  FSEvents queue first, so clearing after it is safe).
- exchange_restore points the carried blur's Renders and Models at
  the new Display (`r.w = a`): their `w` is an unheld context member
  and the old Display dies under them at the swap (Render_dealloc
  SIGSEGV in `w.element_targets`).
- `save-persistent [ target ]`: the window-handle lookup resolved to
  the previous instance after a reload (the platform window is
  reused) and the session file was never written.

Still growing: samplers 191 -> 271 -> 331 -> 396 over three
reloads, mostly `svg` (+38 each, the pair above) and `blur` (+10,
the frost chain's holds above); rcolor/sdf/image textures now stay
near one instance's count. rss 640 -> 936 -> 1012 -> 755 MB. No
crash and no device loss in three exchange reloads headless. The compiler segfault on
`if [ target ] state_persist_store[ target ]` (one-line if with a
call) is unfixed; write the plain two-line form.

OPEN, separate bug: 1 of 7 cycles SIGSEGV on the close thread in
the old instance's destroy: silver_live_destroy -> map_dealloc ->
Pipeline_dealloc -> vector_clear -> Texture dealloc ->
Texture_release_gpu reads a `tag` string whose header is freed. The
strings freed just before are the old ShotPrompt canvas's sampler
keys ('edges', 'sdf_out', 'atlas', 'text'): a double drop in a text
canvas's pipeline/texture chain. Stack in install/tmp/orbiter.log.

## Active work: orbiter memory (Sep 21 2026)

Target from Kalen: about 120 MB; the avatar (about 30 MB) is
the largest object. Startup footprint measured headless.

1. DONE `.f` map only for loaded buffers: `path.read_format_of`
   skips other sections; the leaking hold on `lines` in
   `path_read_format` is removed. 402,759 tokens -> none kept.
2. DONE Text vertex ring made on first use (`text_ring_add`):
   3,200 text Gpu -> 448. Footprint 2396 MB -> 1893 MB.
3. DONE VMA block size 256 MB -> 8 MB (vk.ag): the graphics
   footprint was mostly empty block. 1893 -> 1382 MB.
4. DONE Editor fade mask canvas replaced by a shader ramp
   (Canvas `image_ramp`, `draw_canvas_ramp`); blur outputs
   (`ReduceBlur` rv/rh) at one pixel per point on retina.
   1382 -> 1257 MB. Full-size gaussians stay full size by
   Kalen's call: no upscaling of blur results.
5. DONE `auto_free` lost its reset_only argument: the `[true]`
   resets pinned 368,048 startup objects (~200 MB) for the run.
   media_app.init no longer drains (members are raw stores until
   hold_members after init); the first drain is in run. Worker
   threads (index, git) drain their own pools. realpath buffers
   in path_absolute were never freed. Packed syntax regions
   (vec i32 x5 per token) replace ColorRegion/rgba objects.
   1257 -> 1046 MB.
6. DONE Scene backdrops (scenes.ag create_target) and the frost
   reduce chain at one pixel per point on retina; r_view, screen
   and compose stay full res. 1046 -> 1001 MB. `scenes` is its
   own module: rebuild it separately (`silver --build scenes`).
7. DONE `compose` and `glyph` window canvases removed: compose was
   cleared to #888 and never painted (no compose draw stage ran),
   so the ux shader read constants from it; glyph was unread.
   The ux fragment is now blur * max(1, dim_floor). 1001 -> 961 MB.
   r_view, m_view and the UXBackground shader are removed too:
   on_screen paints the stage's blur texture (or solid_color) into
   `screen` first. `screen` is Display.final, what presents and
   what a shot reads. All four frost stages checked. 958 MB.
8. OPEN GPU images at startup (~530 MB): 12 window-size
   colour targets (screen, compose, r_reduce, r_view+depth,
   6 blur outputs now halved, 1 empty Render unidentified),
   3 at 2402x2402, editor text canvases per pane (23 MB each
   at 2x plus a 4-byte SDF at half size), avatar 3120x3120
   colour+depth (74 MB), backdrop 4096x2048 (32 MB).
9. OPEN 11,840 uniform Buffers: 64 per `uniforms` (vk.ag:2411),
   all baked into descriptor sets at bind; needs one buffer
   with 64 offsets to fix.
10. OPEN small heap 216 MB not yet attributed; ~5,000 pool
   temporaries per idle frame.
