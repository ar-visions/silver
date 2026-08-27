# silver

silver is a native build language. A `.ag` file is a module. A module
builds to a shared library, and to an executable when it declares an app.

Types are Au types. Au is the C runtime silver is written on: reference
counting, reflection, single inheritance. A class declared in silver is the
same struct in C, with the same fields and the same type descriptor.

An import line is a git repository at a commit, a C or C++ header, or
another module; silver clones it, builds it with its own build system, and
links it. C++ headers import with templates and operators intact. A method
left without a body is implemented in the `.c`, `.cc`, or `.rs` file beside
the module. A function marked `export` runs at build time, and what it
writes ships with the module.

Nearly every example on this page is a line from
[`features/features.ag`](features/features.ag), a module of tests the
compiler runs:

```
silver --test features
```

Read that file for the working version of anything here.

---

## modules

A `.ag` file is a module. It holds classes, structs, enums, aliases,
functions, and tests — declared at the top level. Each becomes an Au type,
inspectable at runtime, callable from C.

A module can span files: a sibling file headed `extend features` compiles
into the same module. A module can be an app:

```
app features
    func init []
        puts 'features module'
```

`export` states a version, sets an environment variable at module init,
or writes a registry entry other programs enumerate:

```
export 1.0.0                        # module version
export FEATURES_MODE: 'test'        # setenv at module init
export extensions [ '.feat' ]       # install/export/features.agi
```

Imports name what the module needs:

```
import <stdio.h>                    # C header, read by libclang
import <math.h, ctype.h>            # several in one line
import <cpp.hpp>                    # C++ header: .hpp marks the unit
import More                         # sibling .ag, headed `extend features`
import random as rnd                # module under a local name
```

A git import names a repository and a commit. silver clones it, detects
its build system — cmake, meson, cargo, gn, make, or silver itself — builds
it with that system's own tools, and links the result:

```
import KhronosGroup:Vulkan-Headers/29184b9
import glfw:glfw/fdd14e6 <GLFW/glfw3.h>
    +GLFW_INCLUDE_VULKAN
    -lvulkan -lglfw3
```

Body lines carry the configuration: `+DEF` defines, `-l` links, `-I` adds
include paths, `-D` flags pass to the dependency's build, `> cmd` runs
before the build, `>> cmd` after every one. The same spec works from the
command line — `silver ar-visions:silver:features` clones the repository,
finds the module, builds and runs it. A checkout builds into its own
install folder under the checkout; the host install is read-through, so
nothing a remote repository builds collides with local modules.

A silver-module import can take configuration — the module's own members
are its parameters, set by the importer and read-only inside:

```
import fcfg
    greeting: 'set-by-features'
    scale:    7
```

A URL imports a single file, fetched once, and `>` lines run afterward
with `{import_file}` set:

```
import https://example.com/LICENSE
    > cp {import_file} {install}/share/features/data/fetched.txt
```

`ifdef [ windows ]` / `ifndef [ windows ]` gate imports and statements,
`el` chains them, and the platform names (`linux`, `apple`, `windows`,
`x86_64`, `arm64`) are plain bool values in expressions.

---

## classes

A `class` is heap-allocated and reference-counted. Every member states
what it is:

```
class Counter
    public  count  : i64        # readable outside, written within
    intern  hidden : i64        # module-private
    mutable label  : string     # writable by outsiders
    static  made   : i64        # one per type, not per object

    func init []                # runs after props are set
        hidden = 7

    cast -> string [ 'count-{count}' ]
    cast -> bool   [ count != 0 ]
    operator + [ n: i64 ] -> i64 [ count + n ]
    operator left / [ n: i64 ] -> i64 [ n / count ]
```

The full attribute set:

```
class Widget
    context owner : Owner       # injected by type; read-only; never held
    manual  peer  : Widget      # a plain slot: never held or dropped
    expect  name  : string      # required at construction
    default rom   : path        # the bare command-line argument lands here
    public  refp  : ref i32     # a pointer member
    attrib  egg   : Chirper [ tone: 3 ]     # a static object built at load

    static func make [ d: i32 ] -> Widget
        return Widget [ depth: d ]
```

`context` members bind by type — `w.set_context_from[ o ]` fills every
context slot the source can satisfy, and construction resolves them from
scope the same way. `manual` opts a slot out of the reference counting.
`default` is where a bare `argv` value lands; `--depth 4` style flags map
onto the public members by name. `attrib` is declaration-time metadata —
trinity enumerates shader bindings this way.

Construction is prop pairs — named values set before `init` runs. When
the local has the member's name, `:name` binds it. An indented block is
an initializer too:

```
c: Counter [ count: 3, label: 'main' ]
p: v2 [ :x, :y ]
c2: Counter
    count: 3
    label: 'multi'
c.bump                          # zero-arg call needs no brackets
c.bump*[]                       # * dispatches dynamically
```

`construct` builds from one other type, constructors chain, and
`post construct` shares the name space and runs after init completes:

```
class Named
    expect  name  : string
    construct [ n: i64 ]
        construct [ 'n-{n}' ]   # chain to the string constructor
    construct [ s: string ]
        name = s
```

Inheritance spells the base name as the keyword; `abstract` marks a base
that never runs on its own; a subclass may re-declare a member to change
its default; `super` reaches the base:

```
Counter Doubler
    func bump []
        super.bump[]
        count += 1

abstract Shape Blob

Shape Sq4
    public sides : i32 [ 4 ]    # same slot, new default
```

A member typed `Shape*` holds any subclass. Access through it uses the
static type; `*[]` dispatches on the dynamic type. `getter` / `setter`
make an object indexable, with as many indices as the getter declares:

```
getter [ i: i64 ] -> i64
setter [ i: i64, v: i64, op: i32 ]
getter [ i: i64, j: i64 ] -> i64    # g[ 1, 1 ]
```

Type introspection is built in: `typeid[ T ]`, `sizeof[ T ]` (or an
expression), `x is T` (identity — usable as a value: the object or null),
`x inherits T` (walks ancestry), `any` / `object` accept every object,
and `moduleid[ features ]` resolves a module's runtime type. A type's
members iterate:

```
t: Au_t [ typeid[ AttribHost ] ]
for [ mem: Au_t ] in t.members
```

---

## companions — C, C++, rust in the module

A function without a body is implemented in the companion named for the
module — `features.c`, `features.cc`, or `features.rs` sit next to
`features.ag` and link in automatically (rust binds through cbindgen):

```
intern func c_add   [ i32, i32 ] -> i32          # features.c
intern func cc_mul  [ i32, i32 ] -> i32          # features.cc
rs_bump[ @n ]                                    # features.rs
```

silver strings pass as `cstr`; `@i32` crosses as a plain pointer, so C
writes into a silver stack array, C++ reads it back, rust sums the same
memory. C varargs and macros work directly:

```
n: snprintf[ @buf[ 0 ], 32, "%d-%s", 7, "x" ]
big: i32 [ INT_MAX ]
```

Importing a C++ header brings in the real thing:

```
pi: pairT<i32> [ a: 3, b: 4 ]   # template specialization
si: scaled<i32,8> [ v: 3 ]      # integral template argument
pi * 2                          # C++ operator overloads work
bb.Beast[ 6 ]                   # constructors import as methods
rc: geo2::rect2 [ ]             # namespaces, nested namespaces
```

---

## lambdas

One keyword, two ends of the same idea.

**Named**: `::` in a function's argument list splits it. Call args come at
call time; everything after `::` binds once, at the instance site. A `::`
func is never called by name — it is reached with `lambda`:

```
func step_twice [ n: i64 :: base: i64 ] -> i64
    return n * 2 + base

f: lambda s.step_twice[ base ]  # bind the values after ::
r: f[ 4 ]                       # 4 * 2 + base
```

**Inline**: declare only the call args; everything else the body touches
is captured implicitly — copied at the instance site:

```
f: lambda [ n: i64 ]
    return n * 2 + base         # base is captured
base = 50                       # too late — the copy already happened
```

A lambda type is written inline — return type, then arg types — so
members and parameters carry them. A bare mention hands the lambda over;
a call always brackets:

```
public on_step : lambda i64 [ i64 ]
func apply_step [ fn: lambda i64 [ i64 ], n: i64 ] -> i64
    return fn[ n ]
```

What a lambda captures, it owns: when the last reference drops, the
captured values drop with it. features.ag proves it by riding ten
megabytes through a lambda and watching resident memory come back.

---

## async

`async` fans work items out to threads — one per element of `work` — and
`work_fn` is a lambda, so self and the bound values travel to the worker.
`sync[ null ]` joins:

```
func run_job [ arg: object :: h: AsyncHost ]
    j : AsyncJob [ arg ]
    j.out = j.x * 2 + h.tag

t : async [ work_fn: lambda run_job[ h ], work: jobs ]
t.sync[ null ]
```

The inline form works the same way, reading captured objects on the
worker thread.

---

## tests

`expect func` declares a test returning bool; the compiler runs every one
and reports:

```
expect func t_declare [] -> bool
    x: i32 [ 42 ]
    return x == 42

silver --test features
[features] expect: 116/116 passed (100%)
```

`expect` is also the runtime assert done right: on failure it prints its
message once to stderr and exits 1 — a clean error, not a crash trace.
It binds, and it carries a value (true — a false already exited):

```
expect v == 1, 'v should be one, got {v}'
expect f : @FILE [ fopen[ rom, 'rb' ] ], 'could not open: {rom}'
ok: bool [ expect 1 == 1 ]
```

The second form declares `f`, checks it truthy, and leaves it in scope —
open-and-verify in one line. `fault 'msg'` is the unconditional abort.
Release builds carry none of the test code.

---

## diagnostics

`log` prints under the object's binding stamp — the declaration that
constructed it names it:

```
b: Chirper [ tone: 7 ]
b.chirp[]                       # prints: features:b tone 7
n.egg.chirp[]                   # member default: Nest:egg tone 3
log 'sum {7 + 3}'               # free-function log: no stamp
```

`__FILE__`, `__LINE__`, and `__SEQUENCE__` give the source position and
statement sequence number.

---

## build time and resources

`export func` runs during the build, then exits — bake an artifact once,
ship it, read it at run time. `u64[ @fn ]` is the token hash of a
function's source, so the bake stamp changes only when the producing code
changes; `silver --export` forces it:

```
export func bake_greeting []
    stamp : string '{u64[ @bake_text ]}'
    ...
    op.save[ bake_text[], null ]
```

Any folder in the module ships to `install/share/<module>`; at run time
the working directory is that bundle, so `data/x` resolves. Tests run
from the launch directory and name the share dir through
`path.path_share_path[]`.

---

## vec — the one vector

`vec T` is silver's one vector type. Elements sit end to end at their own
stride — primitives and structs by value, class elements as held slots —
and a vec keeps its identity as it grows. The declaration forms:

```
v: vec string                   # null slot — a type, no allocation
v = vec string []               # made growable, count 0
v: vec i64 [] [ 3, 5, 8 ]       # made + seeded (second bracket = data)
m: vec f32 [ 2x2 ] [ 1.0 2.0, 3.0 4.0 ]   # shaped constant-size
```

A null vec is falsy and iterates zero times. `+=` and `.push` append;
`.count` reads the length; `for [ x: i64 ] in v` iterates and
`reverse in` walks back to front. Indexing a class element is a
non-holding borrow.

The op surface:

```
v.index_of[ 5 ]   v.contains[ 8 ]   v.remove[ 0 ]   v.clear[]
v.first[]   v.last[]   v.pop[]      v.equals[ w ]
v = v.reverse[]                     # reverse returns a NEW vector
v.shift[]   v.unshift[ 1 ]          # front take / front insert
v.insert[ elem, idx ]   v.concat[ w ]
v.origin                            # raw data pointer, for C
```

Members, arguments, and returns all carry vectors; a prop-pair list
literal seeds a vec member. Shaped vecs do element-wise arithmetic —
`+ - * / %` — one scalar broadcasts, and the math builtins map over
them:

```
a3: a1 + a2                     # element-wise
a4: a1 * 3.0                    # scalar broadcast
s:  sqrt[ f ]                   # per element
mn: min[ f, one ]
```

Maps allocate with a trailing `[ ]`, key type in brackets, literal keys
seed, and iteration gives value then key:

```
m: map i64 [ string ] [ 'a': 1, 'b': 2 ]
m[ 'c' ] = 3
m.contains[ 'a' ]   m.rm[ 'a' ]   m.clear[]
for [ v: i64, k: string ] in m
```

An `alias` names any compound type — `alias ints : vec i64`, and
`alias iptr : @i32` names a pointer type that indexes like one.

---

## structs

A `struct` is an inlay value type. It carries operators (including a
`left` slot for the scalar-on-the-left form), methods, and cast members.
Two structs of the same size bitcast into each other; equality compares
member-wise; construction takes prop pairs or positional values:

```
struct v2
    x: f32
    y: f32

    operator + [ b: v2 ] -> v2 [ x: x + b.x, y: y + b.y ]
    operator left * [ k: f32 ] -> v2 [ x: x * k, y: y * k ]

    func dot [ b: v2 ] -> f32
        return x * b.x + y * b.y

    cast -> string [ 'v2-{x}-{y}' ]

a1: v2 [ x: 1.0, y: 2.0 ]
p:  v2 [ 1.0, 2.0 ]             # positional
f1: 2.0 * a1                    # binds operator left *
eq: a1 == b1                    # member-wise
s1: cast span2 [ a1 ]           # same size: a bitcast
```

Structs pass to methods by reference. `inlay` on a parameter passes one
by value:

```
func inlay_len [ inlay v: v2 ] -> f32
```

---

## enums and scalars

Enums take an optional backing type — integers or floats. Values count up
from zero when omitted. An enum converts to its number, and a number or
a name converts back:

```
enum Color : u8
    red:   1
    green: 2
    blue:  3

enum Weight : f32
    light: 0.5
    heavy: 2.0

i: i32 [ Color.blue ]           # 3
c: Color [ 2 ]                  # green
r: Color [ 'red' ]              # by name, at runtime
```

A `scalar` names a unit; the name becomes a literal suffix and each unit
stays its own distinct type carrying one number. Arithmetic and
comparison unwrap it. Integer scalars reject float literals:

```
scalar px : f32
scalar ms : i64

w: 200px
d: 254mm
d == 254mm && d / 2.0 == 127.0
```

Function calls can be commaless: with commas the arguments are
positional, and without them each argument is matched to the parameter
by type, so units make a call site order-free:

```
r:  commaless[ w, d, a ]            # commas: positional
r2: commaless[ 90deg 200px 16ms ]   # none: each finds its own slot
```

---

## types and declarations

`:` declares, and on an object it binds — the name refers to that object,
not a copy. `=` assigns. A local may take its type from the expression;
naming a type the expression already has is an error, not a style choice:

```
x: i32 [ 42 ]                   # typed, initialized
x = 10                          # assignment
y: x + 1                        # local: type from the expression
w: i64 [ x ]                    # the initializer converts
b: items[0]                     # binds to the element, no copy, no hold
st: p.stem[]                    # `st: string [ p.stem[] ]` is an error
```

Every compound assignment works: `+= -= *= /= %= |= &= ^= <<= >>=`, and
a class can overload them (`operator +=`).

C type spellings read as types — `unsigned long`, `int`, `double` — and
`half` is bf16. `object` is the Au base.

---

## expressions

```
t: (x > 0) ? x : -x             # ternary: the condition takes parens
j = cast i32 [ f ]              # cast Type [ expr ] — the one cast form
c: (a <=> b)                    # spaceship: i32 sign result
sx: (a1 + b1).x                 # member chains continue after ) and ]
```

`T [ x ]` converts through every rule the types allow — constructors,
cast members, the convertible table. `cast T [ x ]` is the reinterpret:
unrelated pointers, unrelated objects, checked by nothing.

A comparison against a list or a range reads the way you'd say it:

```
hat:  x == (1, 3, 5, 7)         # true when any one matches
nhat: x != (2, 4, 6)            # true when none match
rin:  x == (2 ... 10)           # inclusive range
rex:  x == (2 ..< 6)            # end-exclusive
e:    c == green                # bare enum name on the right
```

`||` and `&&` keep the deciding value (`name || fallback` yields the
truthy operand), `??` null-coalesces, `->` walks members yielding a
default on null, and a falsy left can return early — with a value or
without one:

```
v: (name) ?? fallback
g: h->shape->tag                # null anywhere gives null, no fault
w: ok || return false
```

Negation comes as `!x`, `not x`, `~x` (bitwise), and `-x`. Comparison
binds tighter than `|` and `&`, so mask tests parenthesize:
`(a & b) == 8`.

---

## strings

Single quotes interpolate; anything in `{ }` evaluates in scope,
including calls, indexing, and nested quotes. Doubled braces are literal.
Double quotes are raw C strings (`cstr`). One character in single quotes
is a `unichar` — `'a'`, `'\n'`, `'\x41'`:

```
name: 'silver'
msg:  'hello {name}, value is {x}'
q:    'x={m['k']}'
br:   'a {{b}} c'               # a {b} c
lit:  "cstr-literal"            # passes straight to strlen
```

Numeric literals: scientific notation (`1e2`, `2.5e-1`), hex (`0xff`),
hex float (`0x1.8p1`), binary (`0b1010`), octal (`0o17`), float suffix
(`3.5f`), shapes (`2x2`).

Strings carry their methods — `mid index_of rindex_of ucase lcase trim
ltrim rtrim split starts_with ends_with append is_numeric integer_value
first` — `+` joins, `s[ i ]` indexes, and interpolation of an indexed
element loads before formatting:

```
k: 'ab' + 'cd'
n: '42'
n.is_numeric[] && n.integer_value[] == 42
```

---

## control flow

Blocks are indentation. `el` is both else-if and else, depending on
whether a `[ condition ]` follows it.

```
for [ i: i32 0, i < 10, i += 1 ]    # init, condition, step
    if [ i == 3 ]
        continue
    el [ i == 7 ]
        break
    el
        total += i

while [ n < 5 ]
    n += 1

for [ running ]                 # one part = condition only
    running = false

for                             # no [ ] = do-while form
    dw += 1
while [ dw < 3 ]                # tested after the body runs

no-op                           # explicit empty statement
```

`break [ 1 ]` leaves nested loops by depth — 0 is the inner loop, 1
leaves both — and `continue [ 1 ]` continues the outer one. `switch`
never falls through, switches integers as well as enums, and cases infer
the enum type so bare member names work:

```
switch [ c ]
    case red
        r = 1
    case green, blue
        r = 2
    default
        r = 3
```

---

## references and raw memory

`@` takes an address; `ref T` alone is a typed null pointer; `local`
allocates on the stack (a runtime count is allowed); `memcpy`/`memset`
are statements, brackets optional. Pointer arithmetic steps by element;
pointer minus pointer is the byte distance:

```
p: @ n                          # address-of; p[ 0 ] += 1 writes n
q: @loc[ 1 ]                    # address of an element
bytes: ref u8 p                 # ref Type expr casts the pointer
loc: local i32 [ 3 ] [ 9, 8, 7 ]
dyn: local i32 [ n ]            # runtime count
r: p + 8                        # pointer math moves bytes, not elements
d: r - p                        # pointer minus pointer: byte distance
memcpy @dst[ 0 ], @src[ 0 ], 8
```

A `handle` member can hold a vector, and a raw `@T` view over it indexes
the same memory with no header hop — the C handoff in one line:

```
raw : vec f32 [ 4 ]
b.data = raw
p : @f32 [ b.data ]
p[0] = 1.5                      # raw[0] is now 1.5
```

---

## math

`sqrt sin cos tan asin acos atan atan2 exp log pow floor ceil round abs
min max clamp mix` are keywords. On vectors they emit loops LLVM
auto-vectorizes. `mix` interpolates structs through their own `*` and `+`
operators. Floats carry methods too — `x.round[ 2 ]`, `n.is_nan[]`,
`x.is_finite[]` — and a shape is a value: `s: 4x4`, `s.total[] == 16`.

---

## inline assembly

The architecture name is the switch — only the matching block compiles.
In-scope variables auto-gather, or `[ inputs ]` names them; Intel
syntax. With a leading type, the block is an expression and `return`
names the register that carries the value out:

```
asm x86_64
    mov rax, x
    add rax, 5
    mov qword ptr [dst], rax

r = i64 asm [ x ]
    mov rax, x
    add rax, 2
    return rax
```
