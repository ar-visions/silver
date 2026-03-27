# Silver Language Design

This document describes the design principles, syntax rules, and constraints of the silver programming language. It is the authoritative reference for how the language works and why.

---

## Silver is a Build Language

Silver is unique in that the language IS the build system. There is no separate `CMakeLists.txt`, `Makefile`, `package.json`, or `Cargo.toml` for your project. The `.ag` source file describes everything — its dependencies, how to build them, what to link, and what to export. A single `import` line identifies a git repository, detects its build system (cmake, meson, cargo, make, autotools), builds it with that project's own tools, and links the result into your module.

This means silver builds other people's software — not just its own. Vulkan headers, GLFW, freetype, OpenCV, zlib, libpng — all built from source using their native build systems, driven by silver's import mechanism. The local silver install contains the full toolchain: clang, lld, llvm-ar, and the SDK. Nothing external is required.

```
import KhronosGroup:Vulkan-Headers/29184b9
import glfw:glfw/fdd14e6 <GLFW/glfw3.h>
    +GLFW_INCLUDE_VULKAN
    -lvulkan -lglfw3
import freetype:freetype/VER-2-13-3 <ft2build.h, freetype/freetype.h>
    -I{install}/include/freetype2
import AcademySoftwareFoundation:openexr/0b83825
    -DBUILD_SHARED_LIBS=ON
```

Each dependency is pinned to a commit hash. No version ranges, no lock files, no resolver. You know exactly what you're building.

---

## Design Elements

**Native apps and libraries.** A module builds to a shared library or executable. Resources live in the module folder — available during development and packaged for release.

**Debug-first.** Every build emits full LLDB debug information. Debug is the default. Release builds use `-O2` with AVX2/FMA and emit object files directly from LLVM in-memory.

**C interop through Au.** Write `.c` or `.cc` companion files next to your `.ag` source. They compile and link automatically with full access to your types. C headers are parsed through an integrated libclang frontend — macros, structs, functions, and enums are available at design time.

**Self-contained toolchain.** The silver install includes clang, lld, llvm-ar, and the SDK. File watching (`--watch`), build caching, clean builds (`--clean`), and module search paths are built in. One binary, no external dependencies.

---

## Module Structure

A module is a directory containing a matching `.ag` file:

```
mymodule/
  mymodule.ag       # silver source
  mymodule.c        # C companion (optional)
  mymodule.cc       # C++ companion (optional)
```

One module = one `.ag` file = one build unit = one shared library or executable.

---

## Declarations

`:` declares. `=` assigns. These are always distinct operations.

```
x: i32 0           # declare x as i32, value 0
x = 10              # assign 10 to existing x
y: x + 1            # declare y, type inferred from expression
```

`=` will never create a variable. `:` always does.

---

## Types

### Primitives
`i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bool none`

### Built-in
`string array map path shape handle symbol`

### Pointers
`ref T` — pointer to T. Used for C interop, stack arrays, and explicit reference passing.

### Allocation
`new T [ count ]` — heap-allocates contiguous typed memory. `local T [ count ]` — stack-allocates.

---

## Functions

```
func name [ arg1: Type, arg2: Type ] -> ReturnType
    body
```

- No `-> Type` means `-> none`
- At expression level 0, zero-arg calls omit brackets: `ensure_seeded`
- In expressions, brackets are required: `result: add[ 1, 2 ]`

### Access
- `public func` — exported
- `intern func` — module-internal
- No modifier at module level — public by default
- No modifier in a class — private by default

### Expect Functions
`expect func` declares a test. The compiler verifies it returns `true`:

```
expect func test_math [] -> bool
    return add[ 1, 2 ] == 3
```

---

## Commaless Arguments (CSS-Style Matching)

Without commas, arguments match parameters by type:

```
set_margin 20px
transition 200ms ease_in
```

With commas, arguments are positional:

```
resize[ 800, 600 ]
```

No varargs. Fixed arity means every type is known.

---

## Scalars

```
scalar px  : f32
scalar ms  : i64
```

The type name becomes a numeric suffix: `200px`, `16ms`. Type-safe — `px` and `em` are different types. Casts between scalars compile to single arithmetic instructions.

Integer scalars reject float literals: `14.4ms` is a compile error when `ms : i64`.

---

## Classes

Reference-counted, heap-allocated, single inheritance.

```
class Counter
    public count: i64
    func init []
        count = 0
    func increment []
        count += 1
```

Construct with property pairs:

```
c: Counter [ count: 0 ]
```

### Inheritance

The parent class name becomes a keyword:

```
Counter DoubleCounter
    func increment []
        super.increment[]
        count += 1
```

### Member Access
- `public` — accessible from outside
- `intern` — module-internal
- `context` — read-only, injected by type matching
- `expect` — required at construction

### Public Type Exposure Rule
Public members and public function arguments cannot expose C-imported opaque types. Use `intern` for members that reference C types without typesize.

---

## Structs

Value types. Passed by reference to methods. No reference counting.

```
struct vec3f
    public x: f32
    public y: f32
    public z: f32
    operator + [ b: vec3f ] -> vec3f [ x: x + b.x, y: y + b.y, z: z + b.z ]
```

Same-size structs are convertible: `(my_vec4f) to rgbaf` is a bitcast.

---

## Enums

```
enum Color : u8
    red:   1
    green: 2
    blue:  3
```

Bare member names work in switch cases. Enums convert to integers and floats.

---

## Control Flow

### if / el

```
if [ condition ]
    body
el [ other_condition ]
    body
el
    body
```

### for

```
for [ i: i32 0, i < n, i += 1 ]
    body
```

Single expression = condition only (while loop):

```
for [ running ]
    process
```

### while

```
while [ condition ]
    body
```

### switch

```
switch [ expr ]
    case value1
        body
    case value2, value3
        body
    default
        body
```

Cases never fall through. Bare enum member names work when type is inferred.

---

## Expressions

### Ternary
Condition must be in parentheses:

```
result: (x > 0) ? x : -x
```

### Cast
```
y: (expr) to Type
```

### Null Guard
`->` between member accesses returns default if null:

```
value: obj->field->subfield
```

### Short-Circuit
`||` and `&&` are short-circuit. When types match, they return the value (like JavaScript). When types differ, they return `bool`.

```
a: x || fallback     # returns x if truthy, else fallback
b: x && y            # returns y if x is truthy
```

### Null Coalescing
```
value: ptr ?? fallback
```

---

## Strings

Single quotes interpolate, double quotes are literals:

```
puts 'hello {name}, value is {x}'
```

---

## Collections

```
items: array i32 [ 1, 2, 3 ]
data:  map image<string> [ hsize: 32 ]
```

### Type Parameters
Collections with `meta.a` require a type parameter:

```
items: array i32           # required
data:  map image<string>   # value type, <key type>
```

Exception: types that inherit from a collection with meta already set (e.g. `tokens` inherits `array` with element type preset).

### Iteration

```
for [ val: ElementType ] in array_member
    use val

for [ val: ValueType, key: KeyType ] in map_member
    use key, val
```

---

## Construction

### Property Pairs
Objects are constructed with named field pairs:

```
c: Counter [ count: 0, name: 'test' ]
```

### Shorthand Property Pairs
When the variable name matches the field name, use `:name` shorthand:

```
sk [ vk: vk, format: Pixel.rgba8, :width, :height ]
# equivalent to: width: width, height: height
```

### Single-Arg Struct Construction
When a single argument is passed to a multi-field struct and the types are convertible (same size), it acts as a cast:

```
f_color: rgbaf [ ref pbr.baseColorFactor ]   # vec4f -> rgbaf, same layout
```

---

## Shapes

Native dimensional literals:

```
s: 4x4
data: new f32 [ 32x32x1 ]
```

`*` inside `[]` after `new` is a shape operator. Pre-compute products as `i32` before passing to `new`.

---

## Math Builtins

All work on scalars and arrays. Array versions emit loops that LLVM auto-vectorizes with AVX2 at `-O2`.

### Single-arg
`sqrt sin cos tan asin acos atan exp log floor ceil round abs`

### Two-arg
`min max atan2 pow clamp`

Usage: `sqrt[ x ]`, `min[ a, b ]`, `clamp[ val, lo, hi ]`

---

## Element-wise Array Operations

Primitive arrays (`new f32`, `local i32`, etc.) support element-wise arithmetic:

```
a: new f32 [ 1024 ]
b: new f32 [ 1024 ]
c: a + b              # element-wise add
d: a * 2.0            # scalar broadcast
```

Operators: `+ - * / %`. Type promotion follows C rules (`i32 * f64` promotes to `f64`).

---

## Inline Assembly

```
asm x86_64
    mov rax, 1
    syscall

result: i64 asm [ x, b ]
    add x, b
    return x
```

Platform-conditional on the same line. Auto-gathers variables when no `[ args ]`.

---

## Imports

### Local modules
```
import mylib
```

### C headers
```
import <stdio.h>
import <math.h>
```

### Git dependencies
```
import user:project/commit_hash
import glfw:glfw/fdd14e6 <GLFW/glfw3.h>
    +GLFW_INCLUDE_VULKAN
    -I{install}/include/special
    -lvulkan -lglfw3
```

Silver clones the repository, detects the build system, and builds:

| Detected File | Build System |
|---|---|
| `module/module.ag` | silver |
| `CMakeLists.txt` | CMake |
| `meson.build` | Meson |
| `Cargo.toml` | Cargo (Rust) |
| `BUILD.gn` | GN |
| `Makefile` / `configure` | Make / Autotools |

### Import body options
- `+DEFINE` — C preprocessor define
- `+DEFINE=VALUE` — define with value
- `-DCMAKE_FLAG=VALUE` — passed to cmake
- `-I{install}/path` — include path (interpolated)
- `-lfoo` — link flag
- `>> shell command` — post-build shell command (interpolated)

### Module parameters
```
import mymod
    debug: true
    coolteen: 'Tim2'
```

Module-scope members become configuration inputs. Set by the importer, read-only from within.

---

## Error Handling

```
expect condition, 'message {detail}'    # debug trap on failure
check condition, 'message'              # verify assertion
fault 'critical failure'                # unconditional abort
```

---

## Operator Overloading

```
operator + [ b: Vec2 ] -> Vec2 [ x: x + b.x, y: y + b.y ]
cast -> string [ '{x}, {y}' ]
```

### Hat Operator (Multi-Value Comparison)
```
r: x == (1, 3, 5, 7, 9)    # true if x matches any
```

---

## Subprocedures

```
z: i32 sub
    if [ b > 0 ]
        return b + 1
    break
```

`sub` stores the body. `z[]` re-invokes with current scope.

---

## Lambdas

```
lambda on_event [ event: string :: source: string ] -> bool
    puts '{source}: {event}'
    return true
```

`::` separates call args from captured context.

---

## Platform

### Conditionals
```
ifdef linux
    import wayland
ifdef mac
    import KhronosGroup:MoltenVK/main
```

Defines: `x86_64 arm64 mac linux windows`

### Build Targets
| OS | Library | App |
|----|---------|-----|
| Linux | `.so` | (none) |
| macOS | `.dylib` | (none) |
| Windows | `.dll` | `.exe` |

---

## Build & Run

```bash
make                    # build debug (default)
make release            # build release
make clean              # clean headers

# compile a module
./platform/native/debug/silver foundry/myapp/myapp.ag

# with options
silver --watch foundry/myapp    # file watcher mode
silver --clean foundry/myapp    # force rebuild all
silver --release foundry/myapp  # release build with -O2
```

---

## Constraints

- **No `const` locals.** Module-level members are the only constants. LLVM optimizes never-modified locals automatically.
- **No varargs.** Fixed arity enables commaless type-matching.
- **No nested generics.** Use `alias` for complex types.
- **No `goto`.** Use `sub` for scoped reusable blocks.
- **No `-> none` on functions.** Omit the return type entirely.
- **Public members cannot expose C opaque types.** Use `intern`.
- **Collections require type parameters.** `array i32`, not `array`.

---

## License

MIT License. Copyright (C) 2017 Kalen Novis White.
