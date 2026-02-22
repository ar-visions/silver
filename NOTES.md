# silver — Technical Notes

## What it is

silver is a programming language and compiler (v0.8.8, MIT, by AR Visions) written in C. Source files use the `.ag` extension. The compiler emits LLVM IR, links with Clang, and produces shared libraries or executables. It targets Linux, macOS, and Windows on x86_64/arm64.

---

## The two core C files

### `src/silver.c` (~180KB) — The front-end

This is the tokenizer, parser, and module system.

- `main()` at line 331 is trivial: `engage(argv); silver a = silver(argv); return 0;`. The real entry is `silver_init` (line 551), which sets up the module, tokenizes the `.ag` source, parses it, and builds it — all inside a `do/while` loop that re-runs on file changes (`silver_watch` using `inotify`). The compiler is its own file watcher; there's no separate watcher process.

- `parse_tokens` (line 1191) is the tokenizer. It scans the raw source string character-by-character: handles indentation tracking, `#` comments (single and multi-line with `##`), string literals (double-quote for read-only, single-quote for interpolated), shape literals (`4x4`), operators, and keywords. It builds an array of `token` objects.

- `parse_statement` (line 2103) is the main dispatch. It checks for `return`, `break`, `for`, `if`, `switch`, `ifdef`, then handles access modifiers (`public`/`intern`), `static`, `func`, `lambda`, `cast`, `operator`, `construct`, `index`. It reads member declarations, type annotations, initializers, and assignment expressions.

- `parse_func` (line 2373) parses function definitions: arguments in `[]`, return types after `->`, lambda context after `::`, operator overloads, cast methods, constructors, and index methods.

- `parse_import` (line 3556) is substantial (~300 lines). It resolves imports by: looking for local silver modules (`mylib/mylib.ag`), fetching from Git (`user:project/commit`), parsing C headers via libclang (`<stdio.h>`), or registering codegen backends (`import chatgpt`). It invokes external build systems (CMake, Cargo, Meson, Make) and links results.

- `reverse_descent` (line 374) is the expression parser — a Pratt-style precedence climbing parser. It reads an atom, then loops through precedence levels from tightest to loosest, matching operator tokens and building binary expression nodes. Short-circuit logic for `&&`/`||`, type checks for `is`/`inherits`.

- `silver_module` (line 724) initializes the keyword table, operator map, assignment operators, comparison operators, and wires up the precedence table at startup.

- `write_header` (line 3926) generates C/C++ header files so companion `.c`/`.cc` files can interop with silver's Au object model without any binding generation step.


### `src/aether.c` (~205KB) — The back-end

This is the LLVM IR emitter and type system.

- Directly uses the LLVM-C API (`LLVMBuildAdd`, `LLVMBuildStore`, `LLVMBuildGEP2`, etc.) — no intermediate representation between the AST and LLVM IR.

- `build_arith_op` (line 138) dispatches arithmetic to the correct LLVM instruction, handling float vs. integer, signed vs. unsigned.

- `aether_e_assign` (line 187) handles all assignment: type binding on first assignment, compound operators (`+=`, etc.), store instructions, and reference counting for class types (retain/release).

- `aether_e_cmp_op` (line 543) handles comparisons across all type kinds: primitives use `LLVMBuildICmp`/`LLVMBuildFCmp`, classes delegate to a `compare()` method, structs recursively compare each member field.

- `aether_e_op` (line 951) is the general binary operator handler. It checks for operator overloads on user types, determines the result type via C-style promotion rules (`determine_rtype`, line 353), and falls through to arithmetic or comparison emission.

- `e_operand_primitive` (line 899) converts Au runtime values to LLVM constants — bools, integers, floats, strings, handles, symbols.

- `aether_e_inherits` (line 461) emits an actual loop in LLVM IR that walks the type's parent chain to check inheritance at runtime, using phi nodes.

- `e_interpolate` (line 861) handles string interpolation (`'hello {x}'`): splits the string into parts, evaluates expressions, converts each to string, and concatenates with `e_add`.

- `e_vector_init` (line 1108) initializes typed vector data — if all elements are constant, it builds a global constant array and emits `memcpy`; otherwise stores elements individually.

- `determine_rtype` (line 353) implements C-style numeric promotion: comparisons always return bool, floats promote to the wider float type, integers promote by size, unsigned wins over signed at equal size.

---

## The runtime: Au

Au is the C-based object model that underlies everything. It's a component system providing:

- **Reference counting** for class types (retain/release lifecycle)
- **Runtime type information** via `Au_t` (type descriptors with members, methods, traits, ABI size)
- **Type hierarchy** with `is`/`inherits` runtime checks
- **Function tables** (vtable-like dispatch for overrides)
- **Member access** by name or index, with `public`/`intern` visibility
- **Operator overloading** registered in the type descriptor
- **Cast methods** registered on types
- **Constructors** with property-pair initialization (`MyType[field: value]`)

The macros in `macros.h` (93KB) are code generation macros — `a(...)` builds arrays, `m(...)` builds maps, `enum_value`/`enum_method` register enum values and methods into the Au type system at startup. The macro system is what allows C/C++ companion files to participate in Au's type model.

---

## The `.ag` files — what silver code looks like in practice

### `foundation/test2/test2.ag` and `foundation/test3/test3.ag` — Language test modules

Exercise the language basics: enums (`Color[u8]`), structs (`Vec2` with operator overloads for `+`, `==`, `/`, `*`, and a `cast -> string`), classes (`Counter` with `init`, methods, `operator +=`, `cast -> i64`), inheritance (`DoubleCounter` inherits `Counter` with `context` member injection), `for` loops, `if/el/el`, arrays, maps, inline assembly (`asm [x, b]`), subprocedures (`sub`), and module-level configuration (`coolteen: string 'Clint'` set from importer as `coolteen: 'Tim2'`).

### `foundation/img/img.ag` — Image processing

Imports zlib, libpng, openexr from Git (pinned commits). Defines `image` class with pixel formats, resize (delegating to OpenCV via declared-but-not-implemented functions), gaussian blur, PNG write using libpng API directly, and `font` class with hook-based lifecycle management. Shows how silver declares functions with no body (`func opencv_resize_area[...]`) that get implemented in companion C/C++ files.

### `foundation/opencv/opencv.ag` — OpenCV wrapper

Thin wrapper importing OpenCV from Git at a specific commit, exposing 4 functions implemented in C++.

### `foundation/gltf/gltf.ag` — glTF 2.0 data model

~270 lines of pure data model declarations. Enums for component types, buffer targets, primitive modes, interpolation. Classes for `Node`, `Mesh`, `Primitive`, `Material` (with full PBR metallic-roughness extensions), `Animation`, `Skin`, `Transform`, `Model`. Also defines a `HumanVertex` struct for skeletal animation. Shows the language being used as a schema definition tool — the class hierarchy directly mirrors the glTF JSON spec.

### `foundation/trinity/trinity.ag` — Vulkan rendering engine + UI framework

The big one (~1100 lines). This is a Vulkan rendering engine defined entirely in silver. Imports Vulkan headers from Git, defines:

- **GPU core**: `trinity` (Vulkan device/instance management), `window` (swapchain, surface, input events), `target` (render targets with framebuffers), `buffer` (GPU memory with VMA), `texture`, `gpu`, `pipeline`, `model`
- **Shaders**: `shader` base with subclasses `Basic`, `PBR`, `Env`, `Convolve`, `Blur`, `BlurV`, `UXCompose`, `UXSimple`, `UVQuad`, `UVReduce`
- **UI framework**: `element` (with layers, events, styles, transitions), `composer` (event dispatch), `style` (CSS-like styling with hot-reload from file watcher), `canvas`/`sk` (Skia-backed 2D drawing with save/restore, paths, text, SVG)
- **Scene graph**: `scene`, `stage`, `pane`, `button` elements

The `init` method on `trinity` directly calls `vkCreateInstance`, `vkEnumeratePhysicalDevices`, `vkCreateDevice`, `vmaCreateAllocator` — Vulkan API used inline in silver syntax.

### `foundation/canvas/canvas.ag` — Empty (planned)

---

## Key design decisions

1. **No package manager.** Imports resolve from Git URLs or local paths. The URL defaults to the project's own Git remote. Commit pinning provides reproducibility.

2. **Build system orchestration, not replacement.** silver detects CMakeLists.txt, Cargo.toml, Makefile, etc. and builds dependencies with their native tools. It only compiles `.ag` files itself.

3. **`:` vs `=` are distinct operations.** `:` always declares. `=` always assigns. `=` will never create a variable. This is enforced at the parser level.

4. **Module-scope members are configuration inputs.** They can't be reassigned from within the module. The importer sets them at import time. This is the module interface.

5. **Companion C/C++ files are first-class.** Place `mymodule.c` next to `mymodule.ag` and it's compiled and linked automatically. Functions declared without a body in `.ag` bind to the C implementation. No FFI boilerplate.

6. **`[` replaces `(` for calls and grouping, but `(` still exists.** Square brackets are used for function calls, array indexing, argument lists, and block grouping. Parentheses are reserved for sub-expression grouping within expressions and for enclosing conditions in the `??` and `?:` ternary operators.

7. **Indentation-scoped.** No braces. The parser tracks indent levels per token.

8. **Classes are reference-counted, structs are value types.** Instance methods receive `a` (the instance). Struct methods receive a pointer to `a`.

9. **The compiler stays resident.** After building, it enters a watch loop (`inotify` on Linux) and rebuilds on source changes. No separate file watcher needed.

10. **LLM codegen as a language feature.** `import chatgpt` registers a codegen backend. `func method[] using chatgpt` delegates body generation to it, with dictation blocks that include text and image references, cached by content hash.
