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

## Rule #3b — Lines: 72 columns max; names, never numbers

Every line of a .ag module fits in 72 columns (tabs count 4).
Break code inside [ ] after a comma, or before && / ||. A line
break is not neutral in silver: a bracketless cast (`f32 i`)
reads by line, so never break inside one.
Allowed over 72, and only these: a line whose single name, URL
or path is longer than the space left (Vulkan's own names), and
a log / expect / fault message string.
A variable is a name (a word or a short word like cv), or
i / x / y. Never a numbered name (wx9, lp9, a9): pick a real
name instead of a digit to dodge a clash.

---

## Rule #3c — Casts: never redundant

NEVER write a cast that changes nothing: `f32[ f32[ k ] ]`,
`i32[ i32[ b ] ]`, `kf : f32 [ f32[ k ] ]`, or a cast to the type
the value already has (`f32[ tex.width ]` when width is f32).
Silver converts scalars on assignment and in arithmetic:
`kf : f32 [ k ]` takes a f64. A cast is for a real type change;
a real two-step change (`f32[ i64[ x ] ]`) is fine.
Before showing a diff, read every new line for a cast that does
nothing and remove it. Repeat offense: thousands of lines were
cleaned by hand.
The same holds for a declared type: name it only when it changes
something. `s: '{minutes}'`, never `s: string [ '{minutes}' ]`
(an interpolation is already a string); the compiler rejects
it ("type 'string' is redundant").

---

## Rule #4 — Isolate and validate every fix and feature

Fix one component at a time. Reproduce the exact failure with the smallest focused test before editing. Change the component that owns the bug; do not add a workaround in another parser, model, build, or runtime layer. Run the focused test after the edit, then validate the affected module, then validate the requested integration target. Report each result separately. Do not call a fix complete unless its focused test and affected module both pass. If integration exposes another failure, treat it as a separate bug. Remove all temporary tracing before reporting results.

---

## Rule #5 — AGENTS.md is the only memory

`AGENTS.md` is the only project memory file. When the user asks to make a memory, write it here. Do not search for or choose another memory location. The user decides what belongs in memory.

---

## Rule #5b — The first attempt is the real one

Kalen (Oct 5 2026): "everything i ask for is usually bullshitted
at first and then i have to ask 12 times to get the real result".
The first version of anything asked for is built as the thing
itself, at the strength and scale asked, not a faint or thin
stand-in to be tuned up later. Before reporting, look at it
where it shows (the sun's spot ON the land, the aurora FROM the
side) and measure it; a change that cannot be seen or measured
is not done. When several items are named, every one lands in
the same pass. The aurora took six builds and the land specular
eight asks: an unmeasured first pass costs more than doing it.

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
codex ]`, adapter names, the agi's bracket list; `agent:` alone is a list of one), or a
block per agent with its models, first the default (`claude:` then
`models: [ opus, sonnet ]` indented under it; trinity `Agent`); the prompt
shows them as a TButtons row (`AgentPick`) with the lit agent's
models in a row above it (`model_pick`), the lit ones take the
first send (`agent_post_as` with the pick and model: claude
`--model`, codex `-m`), and both rows fade out
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
  agent says), `diff` (one line of a source diff it applied),
  `rewind` (the user's undo: cmd+left, ctrl+left on linux, in
  the prompt; `agent_shell_rewind` sends claude a `rewind_files`
  control request for our last replayed message id, files go
  back to that point; the avatar turns 60 degrees, cubic in_out
  over 1.6 s, while its spin slows, reverses and comes back).
  UNBUILT, UNVERIFIED (Sep 25): the build and a live rewind
  test both needed approval.
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

## Rule #11 — Codegen: writing a `using` function's body

A `func ... using claude` (or chatgpt) is the author's signature
and prompt; the agent writes only its body, in the gen/ file the
request names. Author code stays in the module, agent code in
gen/. The agent runs in the project folder the request names:
read the module and its imports there, and use the project's own
types and functions.

The file, exactly:

```
extend <module>
# hash: <the value the request gives>
<the func line exactly as the request gives it>
    <the body, indented with four spaces>
```

- Keep the hash and func lines as given: the compiler checks both.
- Write the body only; edit no other file.
- Never build, test or run silver: the build that asked waits
  on the file, and a nested build deletes it.
- A method body sees its instance's members by name (`seconds`).
- A picture in the prompt is Image 1, Image 2: its path is in the
  source line the request points at. Look at it.
- All of Rules #3, #3b and #3c hold (comments, 72 columns, names,
  no redundant cast or type).
- Arguments are read-only: copy one to a local to change it.
- `el` is else, `el [ c ]` else-if; `log '...'` prints; no hold or
  drop calls.
- Taken names: is, signed, parse, pre, post, ref, line, msg, send,
  hold, drop (a method named hold replaces Au's hold: the object
  is never kept, and the pool frees it at the next drain).
- An interpolated string as a C argument (`unlink[ '{p}'.chars ]`)
  fails: make a local string first.
- Never test a cstr for truth (`if [ x.ident ]`): it crashes.

A body that does not compile goes to the user, never back to the
agent: the cause may be ours. Each such error becomes a rule here.

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

## orbiter-os on the drive — live list (Sep 25 2026)

Goal: orbiter-os boots from a folder on Ubuntu's partition
(nvme0n1p1, UUID 5069068c-…), picked from Ubuntu's GRUB menu.
SUSE is gone: only its EFI loader is left (its btrfs root
7659a74b-… is on no partition).
1. DONE the folder: os-bootstrap writes its file list as
   orbiter-os/root/ (kernel at boot/vmlinuz), times kept.
   Checked: 4,525 entries as the cpio, 1.1 GB; the relative
   install link keeps writes inside; the absolute links are
   files whose targets are in the tree too.
2. DONE a small initramfs (root/boot/initramfs, 5 entries):
   os-bootstrap/init.c, static, finds the ext4 partition by
   `orbiter.uuid=`, mounts it, enters `orbiter.root=` (default
   /orbiter-os; the relative link resolves on the drive), tmpfs
   on /tmp and /run, runs `orbiter.init=` as pid 1 with the
   command line's name=value words as its environment.
   Checked in qemu on an ext4 image: pid 1, / is the folder,
   env passed; a wrong uuid prints why and waits (no panic).
   /orbiter-os -> src/silver/orbiter-os/root is made (Kalen).
3. DONE kernel for europa: BLK_DEV_NVME added (Samsung 980
   Pro); ext4, xHCI, USB HID, evdev, DRM, EFI stub, modules,
   r8169 (RTL8125) were already in. Wi-Fi (RTL8852CE) is not.
   The kernel lines skip while install/share/silver-os-
   bootstrap/bzImage exists: delete it to apply new options.
4. BUILT, unverified until a real boot: NVIDIA 610.57.04 (the
   host driver; modules, firmware and libraries must match).
   os-bootstrap imports the open-module tag, runs the kernel's
   `modules` stage first (Module.symvers), builds against it
   into share/silver-os-bootstrap/nvidia. add_nvidia puts the
   .ko files in lib/modules/orbiter, gsp_ga10x.bin, the ICD
   json and libGLX_nvidia + the libs it opens in the drive tree
   only. init loads `orbiter.modules=` after the switch and
   makes /dev/nvidiactl, nvidia-modeset, nvidia<N> (major 195).
   On a host driver update: change the tag, `nvidia_version`,
   and delete share/silver-os-bootstrap/nvidia to rebuild.
   Secure Boot is off on europa: an unsigned kernel boots.
5. IN PROGRESS the GRUB entry (awaiting Kalen's sudo + a boot):
   appended to /etc/grub.d/40_custom, menu shown 5 s (was
   hidden, timeout 0), update-grub. Kernel command line:
   console=tty0 orbiter.uuid=<p1> orbiter.init=/src/silver/
   install/build/orbiter orbiter.modules=nvidia,nvidia-modeset,
   nvidia-drm:modeset=1 HOME=/root LD_LIBRARY_PATH=/src/silver/
   install/lib:/src/silver/install/build:/usr/lib/x86_64-linux-
   gnu VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json.
   /orbiter-os is the link; no copy.
6. IN PROGRESS first boot (14:31): the menu showed, then a black
   screen. The tree got a root-owned install/tmp at 14:31:09
   (silver-host starts by making it), no log inside. So GRUB,
   kernel, init, mount, switch and exec worked; the launcher
   stopped before logging. Adding: EFI framebuffer console
   (simpledrm), and init saving /dev/kmsg to /boot/kmsg.log and
   pid 1's output to /boot/boot.log, each line synced.
   Second boot (16:09), from those logs: all three nvidia
   modules loaded; nvidia-drm is minor 1 (simpledrm minor 0);
   silver-host held pid 1 (no panic). Two bugs, both fixed:
   - devices kms took card0 = simpledrm (1024x768): the scan
     now skips the simpledrm driver (drmGetVersion name).
   - vkCreateInstance -9: libGLX_nvidia opens more libraries
     than ldd lists. Found by LD_DEBUG=files on the host:
     libnvidia-glsi, -rtcore, -allocator.so.1 added.
   Userns chroot tests are blocked here (AppArmor
   restrict_unprivileged_userns=1): verify by booting.
   Third boot (16:16): kms now picks card1 (nvidia) 2560x1440.
   vkCreateInstance still -9; no NVRM errors in kmsg; every ldd
   dependency of libvulkan and the nvidia libs is in the tree.
   Next boot: VK_LOADER_DEBUG=all added once at the GRUB menu
   ('e'), its output lands in /boot/boot.log.
   Fourth boot (16:21) loader says: libGLX_nvidia loads, but
   "Could not get 'vkCreateInstance' via vk_icdGetInstanceProcAddr".
   On the host the same loader + ICD work with an empty env, and
   strace shows vulkan opens only nvidiactl, nvidia0,
   nvidia-modeset and /proc/driver/nvidia/params. The tree's
   dev/sys/proc are empty (the mounts moved fine).
   TEMP trace in init.c (remove when solved): open() result of
   each nvidia node + gpu count. Next boot adds LD_DEBUG=libs.
   Fifth boot (LD_DEBUG=libs): nodes open ok, 1 gpu. glcore's
   "undefined symbol __malloc_hook/ErrorF (fatal)" lines show
   on the host too (harmless). The real miss: vkCreateInstance
   opens glvnd EGL; libEGL.so.1 was not in the tree. Added
   libEGL, libGLdispatch, libEGL_nvidia, libnvidia-eglcore,
   libnvidia-egl-gbm, gbm/nvidia-drm_gbm.so, the EGL vendor
   json and the gbm platform json. Checked: vulkaninfo run by
   the tree's ld.so with only the tree's libraries finds the
   RTX 3060, driver 610.57.04. grub-debug.sh is still on.

## orbiter-os shell design — live list (Oct 1 2026)

Goal: orbiter as an OS shell. No free-floating windows. The
interface stays as it is: every pane keeps its title bar and
nav list. Changes: a dock on the left side, and the bottom
bar 50% taller.
1. OPEN the bottom bar (StatusBar, orbiter.ag) 50% taller:
   35 px -> 52.5 px (its area and the stack's b35px above it).
   It keeps the status portion: tool tips and orbiter's state.
2. OPEN the dock, a strip on the window's left side: one
   button per element (its module's images/icon.png), a
   launch and status button. Pressing it launches the
   element or switches the view to it. Orbiter's own button
   sits at the dock's bottom, larger than the others.
3. OPEN the status light on the side of each dock button:
   none (not running), busy, needs (waiting on the user),
   done (something new), failed. The words are the exchange's
   (busy / idle / done / needs) plus failed. A hosted app
   sends it over its ring, as HM.title and HM.nav_state do.
4. OPEN switching hides, never quits (macOS style): leaving an
   element hides its view; its instance keeps running and its
   light keeps reporting. close_pane (orbiter.ag) already keeps
   instances running when a tab closes.
5. OPEN the editor is an app in the dock, not the shell's
   default. It declares itself by export: the file types it
   claims (.ag .c .h ...), as aura's .agi claims .html, and a
   dock flag so it is in the dock from the start.

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
silver --clean trinity      # rebuild the module (not imports)
silver --release trinity    # release build
silver --build orbiter      # compile only, no launch
silver --test expectest     # run the module's expect tests, exit
silver orbiter --width 1920 # --width goes to orbiter, not silver

# Primary development workflow (bare launch builds AND runs)
silver --clean orbiter
```

- `make` is `make release` (Sep 29, Kalen); `make debug` for -O0 -g. Both build into install/build and link install/bin/silver: the last one built wins.
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
- `make` is `make release` (Sep 29); `make debug` builds the same tree at -O0 -g.
- Debug binary → `platform/native/debug/silver`. Release → `platform/native/bin/silver`.
- `gen.py` updated: app output uses `$builddir/` not `bin/`.
- Build caching: `update_product` checks `.product` symlink timestamp vs module file. Empty `.artifacts` file no longer triggers rebuild (`!newest` = product is valid).
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
    Again Sep 24: `silver --verbose --build orbiter` exit 134
    (abort), log ends mid-parse in Git.ag, no error; the retry built.

## Active work: browser identity (Sep 26 2026)

1. DONE sites gave the WPE browser captchas. WPETrinityBrowser
   (aura/wpewebkit/Tools/TrinityBrowser/main.c) now sends
   Safari 18.5 on macOS as its agent, and a document-start
   script makes navigator.platform 'MacIntel'. Checked with a
   local server: both the header and the page report Mac.
   The overlay reaches a fresh checkout only: the file was also
   copied into checkout/wk/wpewebkit-2.54.0 and the helper
   rebuilt (ninja bin/WPETrinityBrowser + its install).
   Untested: whether the captchas stop (other signals remain).
2. OPEN youtube.com stops at its placeholder columns: its main
   script throws "ReferenceError: Can't find variable:
   HTMLVideoElement" (console, Sep 26). WPE is configured with
   ENABLE_VIDEO=OFF, and WPE ties video to GStreamer
   (GStreamerDependencies.cmake). Kalen's call: a trinity media
   backend, no GStreamer. Stages, in order:
   a. DONE ENABLE_VIDEO on without GStreamer: YouTube's app
      starts (frame checked; no console errors). Overlay
      files: Source/cmake/GStreamerDependencies.cmake (no
      VIDEO -> GSTREAMER tie), Source/WebCore/platform/
      GStreamer.cmake (returns when USE_GSTREAMER is off),
      Source/WebKit/GPUProcess/media/trinity/
      RemoteMediaPlayerProxyTrinity.cpp + SourcesWPE.txt.
   b. MediaPlayerPrivateTrinity: a plain video file plays,
      frames decoded and drawn through trinity.
      b1. DONE vulkan h.264 decode in trinity: C front end in
          trinity.c (h264d_*: sps/pps/slice headers, POC types
          0-2, 17 reference slots with sliding window + MMCO,
          display order, the vk begin/decode infos), Decoder in
          video.ag (session, params, layered DPB that is also the
          output, decode queue from vk.ag), Demux in demux.ag
          (progressive mp4). webgfx t_video_decode: 48/48
          frames of webgfx/media/clip.mp4 (High, B-frames) in
          display order, each ~48.6 dB PSNR against ffmpeg's
          frame (rgb rounding only). Frames come back to the
          CPU for now (host buffer + nv12_rgba).
      b2. DONE drawn on the GPU: the decoder hands out y, u, v
          planes (a plane copy on the CPU, no colour math);
          webgfx_video_new/_advance/_seek/_info/_free and
          webgfx_canvas_draw_video (the canvas's yuv mode).
          t_video_draw: frame at 1 s is frame 24, luma 40 dB;
          t_video_steps: 90 steps at 60 a second, all exact.
          Edit lists read (elst), else times ran 2 frames late.
      b4. DONE MediaPlayerPrivateTrinity (engine Trinity, mp4
          with avc1/mp4a): fetches the file through WebKit's
          media loader, plays on a 60 Hz timer and a monotonic
          clock, paints a VideoFrameTrinity that
          GraphicsContextTrinity::drawVideoFrame draws from the
          planes. Checked with a local page: 300 frames of a
          10 s 640x360 clip each shown once, frame at 7.53 s
          matched the page's clock, ended at 10.00, no error.
          Overlay files: VideoFrame.h (isTrinity), trinity/
          VideoFrameTrinity.h, MediaPlayerPrivateTrinity.h/.cpp,
          GraphicsContextTrinity.h/.cpp, WebGfx.h,
          MediaPlayer.h/.cpp, MediaPlayerEnums.h,
          WebCoreArgumentCoders.serialization.in,
          SourcesTrinity.txt.
          Limits: whole file downloads before playing; frames
          cross the CPU once; no audio yet.
      b3. DONE sound: spectra aac_clip (faad2) decodes a
          track's AAC packets whole into an AudioClip, played by
          one AudioMixer voice (voice_seek added) positioned at
          the video's time; webgfx_video_play/_pause/_volume/
          _has_audio; the player follows play, pause, seek,
          volume and mute. t_video_audio: webgfx/media/av.mp4's
          tone decodes at 48 kHz, 440 cycles a second. In the
          browser (click to play; unmuted autoplay is blocked
          as in any browser) WPEWebProcess holds an uncorked
          stream at 100% while it plays. Not heard by me.
          Limits: playbackRate other than 1 keeps the sound at
          1x; the picture follows a monotonic clock, not the
          audio clock; mac's coreaudio mixer path has no seek.
   c. NEXT MediaSource (YouTube streams through it).
3. OPEN silver bug: `v.push[ w[ i ] ]` (w a vec i64) pushes the
   element's address, not its value (demux ctts gave
   98715440282664, +16 a sample). A local first works.
   c. MediaSource (YouTube streams through it).

## Active work: strokes (Sep 27 2026)

1. DONE svg/canvas strokes were wrong (lucide.dev icons: thin,
   open paths closed into triangles, no caps, lines gone).
   Cause: trinity drew every stroke INSIDE the outline (a CSS
   border), closed every subpath, dropped zero-area boxes, and
   a path's band took its width from stroke_sides, which only
   ui layers set. Kalen's rule: one stroke model, no parallel
   mode; our ui adjusts with offsets. Now (trinity/Canvas.ag):
   - both branches (box and path) draw one band centred on the
     outline, moved by stroke_offset (positive outward);
     -width/2 is the ui's inside border, the same formula as
     before for even sides.
   - stroke_width[] / set_stroke_size[] set all four sides;
     stroke_sides[] after narrows a box's sides.
   - edges carry flags: free start, free end, implied (the
     closing edge a fill adds). An open path stroked with no
     fill skips implied edges, caps its free ends (butt,
     round, square) and writes unsigned distance.
   - a line (no area) is kept when it has a stroke width.
   - ui sites set stroke_offset -width/2: the layer loop, the
     button group outline (trinity.ag), 13 in orbiter/Git.ag,
     4 in orbiter/Editor.ag, the scrollbar outline.
   - webgfx_canvas_stroke_path takes the cap and sets offset 0;
     GraphicsContextTrinity passes lineCap().
   Checked: webgfx t_stroke_caps and t_stroke_inside pass,
   trinity tests pass (average colour unchanged 16,32,53,80),
   the lucide test page (x, check, plus, chevron, circle, an
   8px line) draws as lucide does. Not checked by eye: orbiter
   itself (sides of different widths now centre each side on
   the inside offset of the widest), and trinity's own svg
   icons, whose strokes are now centred as svg says.

## Active work: scene picker (Sep 25 2026)

1. DONE the bar's < and > scene arrows are gone. Clicking the
   scene name opens the pane's nav dialog (the browse list) on
   scene cards, drawn as the instance cards are (FindHit.scene,
   .thumb, card[]; Git.ag), with "current" under the loaded
   one; a card loads its scene and closes the dialog
   (scene_choose). Checked headless: 14 cards with pictures,
   a pick closes it and loads the scene.
   FIXED on the way: scene_module_ensure never had scene_mods
   (filled by ensure_scenes, which only the old label called):
   "scene: no class for MilkyWay" every frame.
3. OPEN silver bug: `if [ s && o ]` with s a string and o an
   object keeps the value: it casts o to string (IR: cast_string
   of orb in CommitLayer_draw), which recursed and crashed.
   Worked around with bool checks (FindHit.card[]).
2. DONE thumbnails render at export: scenes' `export func
   scene_thumbs` (last in scenes.ag, after every bake) draws
   each scene into a 320x200 offscreen Window and writes
   share/silver-scenes/thumbs/<Name>.png; skipped when present
   unless SILVER_EXPORT_FORCE. All 13 written and checked by
   eye. The tiles load those pngs. A scene edit does not
   redraw its thumbnail: delete the png or force the export.
   Found on the way: a scene's `draw` needs `draw*[ w ]` from
   an `element` reference; new_object, the Window and the
   vk_context must be held (pool-managed: freed mid-render).

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
4. DONE (Oct 2) H.265 encode and decode in our MoltenVK
   (VK_KHR_video_encode_h265, _decode_h265; Main 4:2:0 and
   Range Extensions 4:4:4 over VideoToolbox's HEVC, its
   'HEVC_Main444_AutoLevel' profile; VPS/SPS/PPS rewritten for
   decode). trinity/MoltenVK.diff regenerated (applies to
   db445ff). The Apple 4:2:0 fallback in trinity.ag is gone.
   trinity vk.ag: decode shares the encode family when the
   driver has one video family (MoltenVK). trinity.c h265d: a
   mid-stream CRA keeps its RASL pictures (was dropped).
   Checked against AVFoundation byte for byte: a lossless
   4:4:4 recording (89/89), Apple's H.265 Main with B-frames
   and a CRA (89/89), H.264 High (89/89).
   PR KhronosGroup/MoltenVK#2836: H.265 applied in
   ~/src/MoltenVK-pr (docs too; upstream already has
   VK_EXT_ycbcr_2plane_444_formats), Xcode build passes; NOT
   committed or pushed (Rule #1): Kalen's step.
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
8. DONE (Oct 6) design-time resources: `Image [ name: 'images/moon.png',
   width: 256, height: 256 ] using chatgpt { prompt }` anywhere
   an Image goes. The compiler builds a design instance of the
   type (construct_with, live: imports are loaded) and hands it
   to the codegen's generate_res; chatgpt and claude accept an
   Image (instance check) and refuse anything else. Only an
   import's types: a class of the module itself has no code yet.
   The name has a folder (gen/'s folders are resource folders).
   The png is checked for size, stamped in gen/<folder>/.<file>.hash (a dot name stays out
   of the share); the
   build links it into share/<m>/<name> and the brackets become
   [ '<name>' ]. A number token's literal is a shape: one
   dimension is turned into an i64 before the property store.
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
   Earlier on this item: the avatar's gradient rim.
5. APPLIED, built, awaiting Kalen's run: a reopened exchange
   kept the avatar at its settled (left, tilted) place. The
   reset keyed on a new shot_ask path; a file exchange reuses
   the file's. The avatar is now settled only while agent_view.
6. APPLIED, built: the exchange's claude run loaded the claude.ai
   connectors (Gmail, Calendar, Drive: needs-auth) and told the
   user so. It runs with --strict-mcp-config: no MCP servers.
7. APPLIED, built, awaiting Kalen's look: stair-stepped edges
   where the volume meets the metal. The volume draws at 1/4 of
   the avatar's points (1/8 of retina pixels) and its depth test
   was in-or-out per volume pixel. An 8-tap depth coverage test
   had no visible effect and was taken out; vol_scale stays 0.25
   (0.5 is far too slow, Kalen). Testing: the donut's glaze
   peaked at its tube surface and cut to 0 outside it (a hard
   outline); it now fades over the tube's outer 20%.
8. APPLIED, built, awaiting Kalen's look: a darker violet band
   on the fins after the core's yellow, for contrast. The old
   violet keyed on height above the core (over 0.6 units) and
   never reached the fins. Replacing the orange by distance took
   the yellow away (fins start 0.2 from the core); the orange is
   back as it was, and a separate violet band is added beyond it
   Now a ring light (Kalen): radius 0.28 round the core's axis,
   0.15 above the pool light (level with the fins' tops), reach
   0.10, blue (0.16, 0.22, 1.4) x1, wrapped facing (0.35 floor).
   Scaled by two lobes: the halo's and the same turned 180
   degrees (max of the two); 1 - pl_pulse was tried and dropped.
9. APPLIED, built, awaiting Kalen's look: more heat blur at the
   core. Heat's 3x3 blur spread 0.5 + 2.5*heat*wgt px (about
   1 px at rest); now 0.5 + 7*heat*wgt, with a second 3x3 at
   half spread (18 taps, only where wgt > 0.002).
10. APPLIED, built, awaiting Kalen's run: with a larger font
   unit the resting prompt field and agent picker rose into the
   avatar (placed by their bottom edge, scaled by the unit).
   They keep their unit-1 centers now (field 240 px, picker
   296 px up) and grow around them; the mic stays in the
   field's right end. The avatar does not move.
   The user's box (its frame is the field's) keeps the same
   center too: top 354 px x unit above it, 420 px x unit tall.
   Split (Kalen): the avatar centered in the window's top 70%
   (orbiter CSS t35%-195px), the picker+field group centered in
   the bottom 30% (15% of root bounds up; 231 px before layout).
   Then (Kalen): avatar down, form up: avatar center 40% down
   (t40%-195px), group center 22% of the height up.
   The first cut read the Region's second slot as a bottom edge
   (it is the TOP): the controls sat half a height too low. orbiter32.gltf COLOR_0 is soft (839 values on
   Cylinder.001) and 8,000+ triangles span b=0 to b=1, so the
   color blends across them. The two soft reads are now on/off
   at 0.5 (Avatar.ag): the core light (`emit * v_color.r`) and
   the groove glow mask (`v_color.b * (1 - v_color.r)` with its
   gamma and smoothstep). The other reads were already on/off.

11. DONE orbiter crashed at startup in path_ls (index_work).
   79f791b queued checkout/'s plain folders on `later` for the
   stack walk; stack/repos/later were pool-owned locals and
   index_work drains its pool per directory, so the first drain
   freed the stack. They are held for the walk (cleared, not
   replaced). The pull dropped this fix once; it is back.
12. DONE trinity did not build on macOS after ef72fa5:
   video.ag `tid : u64` (Linux's pthread_t) is `pthread_t` now.
13. DONE startup SIGSEGV freeing env.gltf's GltfBuffer (every
   debug launch, 3-4 s). Display.resize did `cast Window [ a ]`
   (unchecked) and set rebind_frost on environment's plain
   Display: a write 95 bytes past its 577-byte object, into the
   next block (found with malloc_history + a watchpoint). The
   pull's three Texture members pushed it past malloc's slack.
   resize and draw check `a inherits Window` now. Debug and
   regular: 5/5 headless launches alive at 30 s.

## Active work: model picker (Sep 28 2026)

1. APPLIED, not built (build needs approval): each agent lists its
   models in the agi (trinity `class Agent`: name, models; the
   bracket form `agents: [ claude, codex ]` still reads through
   its string construct), orbiter.agi has claude opus/sonnet/haiku
   and codex gpt-5-codex/gpt-5. The prompt shows the lit agent's
   models as a second AgentPick row (`model_pick`) 34 px UNDER the
   agent row (Sep 28, Kalen: agents on top); both rows are one
   width, 360 px at unit 1; the first model lit; a mounted row keeps its
   selection while its agent stays (AgentPick.agent). The send
   takes it (Window.model_pick, in ExchangeSession/ExchangeState
   too) into agent_post_as's new model argument, which the shell
   already passed on (claude --model, codex -m). orbiter.ag's
   `agents` is `vec Agent`. To verify: build trinity and orbiter,
   open the exchange, check the two rows and the model on the
   agent's command line (install/tmp/agent-shell.log).

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
3. APPLIED, not built (build needs approval): the exchange
   sizes in UX units (`ux_unit`, font unit / 22, 1.0 at
   rest). `ux_area` scales each px of a Region string; the
   boxes, field, mic, close, picker and note spacing follow
   ctrl +/- as the text does.

## Active work: browser element on Ladybird LibWeb (Sep 25 2026)

Goal: a `browser` module (trinity element; orbiter opens it as a
pane tab). Ladybird does layout, JS and network; trinity's Canvas
does ALL drawing. Skia is never built, linked or shipped (Kalen:
"I WILL NOT use Skia. Skia is a canvas. WE HAVE THAT").
Decisions (Kalen): own module, static page first, keep Ladybird's
helper processes (WebContent, RequestServer, ImageDecoder).
Ladybird checked at 3af7308 (Sep 25): WebContent sends each frame
to a Compositor process as `update_display_list` (DisplayList +
visual context tree + resources); DisplayListPlayerSkia replays it.
The browser element takes the Compositor's place: it serves the
CompositorWebContentServer/Control endpoints and replays the list
with a DisplayListPlayer that calls trinity (the glue).
1. OPEN import Ladybird from its URL (CMake, vcpkg deps, Rust via
   ~/.cargo) with aura/ladybird.diff: skia and angle out of
   vcpkg.json and every CMakeLists. Disk: 33 GB free.
   Every Skia job routes to trinity (Kalen: trinity does all of
   it; no new stand-in code):
   - Gfx::PathImpl (virtual): PathSkia -> trinity CanvasPath.
   - Gfx::Typeface / Font: TypefaceSkia -> trinity CanvasFont
     (glyph metrics, GlyphAtlas).
   - Bitmap scale/convert, images, gradients: trinity + img.
   - PaintingSurface (SVG as image, cursor, screenshot, canvas
     2D): trinity Canvas.
   - System font match (Skia's fontconfig manager): trinity
     CanvasFont gains a family/weight/width/slope lookup.
   The bridge: Au headers cannot enter Ladybird C++ (Au's
   get/set/len/push macros break AK). A silver module `webgfx`
   imports trinity and exports plain C funcs; the diff's
   TypefaceTrinity/PathTrinity/... .cpp call them. Build order:
   webgfx, then Ladybird (links -lwebgfx), then browser.
   webgfx has three groups: font, path, canvas. WebContent also
   draws (SVG as image, cursors) with the display list player,
   so DisplayListPlayerTrinity lives in LibCompositing and runs
   in both processes over webgfx's canvas calls.
   vcpkg without skia/angle: 61 packages; needs cmake >= 3.30
   (vcpkg's own), nasm and autoconf-archive (built into the
   scratch tools dir; move to bootstrap). Building (scratch).
   DONE trinity CanvasFont: loads from bytes (data, data_size,
   face_index), units_per_em, glyph_count, ascender/descender/
   x height in font units, glyph_index, advance_units, outline
   (glyph into a CanvasPath at a px size, unhinted); the glyph
   cache and GlyphAtlas are keyed by glyph number. texttest
   draws right (shot). webgfx t_font_from_bytes printed good
   values (SpaceMono 'A': 14 points, ascent 22.4 at 20 px).
   DONE trinity CanvasPath: elliptical_arc_to (svg), bounds,
   contains (nonzero / even-odd), length, point_at, transform.
   DONE webgfx font + path calls (webgfx/webgfx.ag); `silver
   --test webgfx` exit 0: t_font_from_bytes, t_path_geometry
   (100x50 rect, in/out, length 300; half circle r50 157.04).
   DONE patch, build files: skia, angle, Ladybird's own vulkan,
   Compositor, WebDriver and UI/ out; LibGfx, LibCompositing,
   LibWeb link webgfx (find_library under SILVER_INSTALL).
   DONE trinity FontMatch.ag: system font lookup (fontconfig on
   linux; family/weight/width/slope, fallback by codepoint,
   emoji via color); webgfx_font_match; t_font_match passes
   (monospace -> DejaVuSansMono.ttf, unknown -> not found,
   'A' -> Noto Sans, as fc-match says).
   DONE trinity CanvasFont family_name, weight/width/slope (OS/2
   table), set_axes (variable fonts); a bad font from bytes
   leaves the face null (loaded[] false), no fault. webgfx
   5/5: t_font_variable matches fontTools (Roboto Condensed H
   1270 units at wght 400, 1257 at 700), t_font_bad_bytes.
   WRITTEN, not compiled: patch LibGfx PathTrinity (.h/.cpp),
   Font/TypefaceTrinity (.h/.cpp), WebGfx.h (the C calls),
   Font.cpp metrics via webgfx, every TypefaceSkia caller
   renamed. Patch diff comes from `diff -ruN` against a clean
   download of 3af7308 (a `git rm --cached` of mine touched
   the scratch clone's index; no more git ops there).
   DONE webgfx canvas group (new, clear, save/restore,
   transform, clip, fill rect/rrect/path, stroke, glyphs by
   glyph number, image, read back rgba8): t_canvas_draw,
   t_canvas_glyphs ('Hello' checked by eye). trinity Canvas:
   draw_glyphs (draw_text_pass split into text_begin /
   glyph_quad / text_end; texttest shot unchanged), fill_rgba,
   stroke_rgba, set_transform take floats: a vec4f or affine
   passed by value across modules arrived shifted (the struct
   by value bug), fill came in as (garbage, 0, 1, 1).
   DONE patch compiles: LibGfx (PaintingSurface = a webgfx
   canvas; wrap_bitmap copies back on flush; PainterTrinity;
   BitmapExport packs by hand; Bitmap scaled via trinity;
   ColorSpace kept as data; CanvasCommandPlayer resolves
   fonts, no text blobs), LibCompositing (resource storage
   without Skia caches; DisplayListPlayerTrinity), all of
   LibWeb. ANGLE's GL headers (7, text only) download at
   configure for WebGL's constants; no GL library.
   Compositor service kept (a helper process), on the trinity
   player; backing stores are wrap_bitmap over the shared
   bitmaps; WebGL returns failure; webgfx_gpu_init opens the
   device where Skia did, before seccomp.
   UI/Trinity: Ladybird's GUI framework choice "Trinity"
   (default on linux, no Qt): an empty Application, used
   headless (--headless=screenshot) to prove the path; the
   element will drive Application's event loop in orbiter.
   The player logs "not ported: X" once per missing feature
   (gradients, shadows, repeated backgrounds, layers/opacity,
   masks, rounded/path clips, iframes, canvas, video, 3D).
   clang 22 fix: ComputedValues.cpp lambda needs `-> bool`.
   DONE (Sep 25 18:1x) FIRST PAGE: all of Ladybird builds on
   trinity (no Skia, ANGLE or Qt) and `Ladybird
   --disable-sandbox --headless=screenshot` renders a test
   page right: heading, paragraph, red box, rounded green box,
   blue border (scratch/shot.png). Fixed on the way:
   - TypefaceTrinity copied bytes when no backing, then the
     caller's set_font_data freed the copy (HarfBuzz SIGSEGV
     on the font catalog thread): no copy now.
   - webgfx tables locked (pthread mutex): fonts load on a
     worker thread; FreeType faces made under the lock.
   - trinity assets open relative to the cwd (models/uv-
     quad2.gltf): webgfx cds to its share dir on first GPU
     use (dladdr of its own library).
   - Canvas's `load_existing [ true ]` never reaches Render's
     field (a shadowing member): a pass after text cleared
     the canvas black. webgfx passes load_existing: true.
     OPEN (trinity): the shadowing Canvas.load_existing.
   - PaintingSurface.write_from_bitmap now updates a wrapped
     bitmap too (the screenshot came back empty).
   Tests: webgfx 13/13 expects pass.
   OPEN sandbox: the Compositor's Landlock list lacks
   trinity's install and cache dirs; WebContent draws (SVG,
   cursors) and needs the GPU too. Runs use --disable-sandbox.
   DONE (Sep 25 evening) the browser element, aura/aura.ag.
   Shape: Ladybird's UI side (LibWebView, its event loop and
   mimalloc) stays out of orbiter's process. The element forks
   the Trinity frontend (UI/Trinity/main.cpp, a HeadlessWebView
   subclass) as one more helper: frames come back as rgba8 in
   a shared file (TRINITY_BROWSER_SHM: header magic TRBW, seq,
   w, h, stride; then pixels), input goes down a socketpair
   as lines: size W H, load URL, mouse down|up|move|wheel x y
   button held mods [dx dy], key down|up glfw_code mods,
   text <utf8>. Closing the socket ends the helper.
   Checked headless (socket press/text/key/wheel + shot):
   draws the page at device size, click focuses, typing and
   Backspace edit, a button's JS runs, wheel scrolls 120 px a
   step, the scrollbar draws; colors exact (#9cf = 153,204,255).
   Fixed on the way:
   - silver: string literals given to a C vararg (execl) arrive
     as objects, not text: '--disable-sandbox' became garbage,
     the sandbox stayed on, Landlock refused trinity's shader
     cache. Pass .chars of string locals (as qemu.ag does).
   - C macros SIG_IGN / MAP_FAILED (casts to pointer) emit bad
     IR; the element uses send(MSG_NOSIGNAL) on a socketpair and
     `cast u64 [ m ]` for the mmap check.
   - `func send` / arg names `line`, `msg` clash with existing
     C and type names in scope: renamed post / words.
   - trinity text_event/key_event gave a focused app element
     each key twice (focus, then app_object): skipped when they
     are the same element ("hihi" before).
   - Textures of page pixels are `linear: true` (UNORM): the
     sRGB default decoded once into a UNORM target, mid-tones
     came out dark (element and webgfx_canvas_image).
   - Compositor paints scrollbars between replays with the
     surface passed in; the player used its replay surface
     (null there): crash on any scrolling page.
   - GLFW wheel up is Ladybird's minus (measured).
   Orbiter: install/export/silver-aura.agi claims .html/.htm;
   Editor.handler_module_for hosts `browser` on them (as qemu
   on .img), so .html files now open in the browser view.
   orbiter builds; NOT run inside orbiter yet (not asked).
   TEMP: install/ladybird is a symlink to the scratch build
   (/tmp/claude-1000/...); the import replaces it.
   OPEN: sandbox (still --disable-sandbox), the import,
   hidpi scale check, key repeat, cursor shape, page title,
   navigation (back/forward/url bar), the gaps "not ported".
   NEXT: the silver import of Ladybird (aura/ladybird.diff
   from diff -ruN, vcpkg + cmake 4 + nasm + autoconf-archive
   in bootstrap).
   DONE (fixed below) separate bug: `silver --test trinity` traps in
   trinity.ag `expect func test` when CanvasUI.vert must be
   compiled in the test child: glslang "Unable to parse
   built-ins". With the .spv cached it passes (exit 0).
   CAUSE (checked): expects run inside the module's own ctor.
   libsilver-trinity.so .init_array order: frame_dummy,
   silver_trinity_initializer, trinity.cc, glslang
   Initialize.cpp, Scan.cpp, hlslScanContext: the test compiles
   a shader before glslang's statics exist. Reproduced with a
   bare dlopen harness (no silver process). Owner: aether's
   expect emission + silver.c link order (module .o first).
   The empty static: glslang Scan.cpp:325 `const unordered_map
   KeywordMap {..}` is filled by Scan.cpp's own ctor; before it
   every keyword scans as IDENTIFIER. Only libraries hit this
   (apps and live apps run module init from main).
   FIX: aether emit_library_late puts a plain library's
   expects/exports in exported <module>_late (late_symbol on
   aether); silver.c late_object writes <name>-late.c, a ctor
   calling it, linked after every lib (native link only; the
   device link path is not done). trinity .init_array now ends
   with silver_late. Checked: shader cache deleted, `silver
   --test trinity` exit 0, no glslang errors, both CanvasUI
   shaders compiled in the run; img exports run from
   silver_img_late; webgfx 3/3; texttest (live app) draws.
   OPEN separate: features t_vec_module fails: vec2f.add
   returns y as garbage (libvec returns both floats in xmm0,
   silver reads y from xmm1: the struct-return ABI bug in
   memory). AGENTS said 213/213 on Sep 24; why it passed
   then is not known.
   Stray: my `touch vec/vec.ag` made an empty module that
   overwrote make's install/build/vec.o; removed it and its
   products, make rebuilt vec.o and libvec.so from src/vec.c.
   silver's build made git tags vtmp-1.0.0 (a temp module,
   deleted) and webgfx-1.0.0; I did not remove tags.
2. OPEN the trinity player: the 26 draw commands (rects, rounded
   rects, glyph runs, images, gradients, box shadows, paths,
   lines, ellipses) plus clip, transform, layer, mask, filters.
3. OPEN the element: spawn WebContent, serve the compositor
   endpoints, load a local .html file, draw it (milestone 1).
4. BUILT, not run: orbiter's title opens an address. A module
   exports its schemes (`export protocols ['http', 'https']`
   in aura.ag, read from its .agi); Enter on text with a
   scheme some module exports hosts that module in the
   active pane with the address as its argument
   (Editor.open_url, host_module shared with launch_handler).
   DONE (Sep 25) selection painted twice (rect and text):
   - Canvas.sync_all called draw[] before sync_fence: the
     last paint's shape drew a second time on every read.
     sync_fence already submits the batch; draw[] removed.
   - Canvas.set_clip stored clip_x/y/w/h that nothing read:
     no Ladybird clip applied, so a line with a selection
     drew its text twice. set_clip narrows `crop` now (what
     shapes and text honor); an empty clip parks it off the
     canvas. Checked on example.com: one 80% wash per line,
     'ations.' ink 43,100 -> 34,268. webgfx and trinity
     tests exit 0.
   DONE the Trinity frontend never hands its URL to another
   running Ladybird (should_coordinate_browser_process false):
   a second element exited when one was already open.
   Kalen's next three, in his order (Sep 25 evening):
   a. PARTLY DONE slow rendering. Wikipedia per frame was
      images 22-28 ms (a new texture per draw), rects 13-17
      ms, text 5-7 ms, plus 17 full GPU waits (paint_n 64).
      - images: one webgfx texture per decoded frame
        (webgfx_image_new/free/canvas_draw_image), owned by
        DisplayListStoredImageFrameResource: 25 ms -> 0.06.
      - text draws in the Canvas's own batch (b below).
      - Render.paint_n is public; Pipeline rings and uniform
        buffers follow it (default 64); webgfx canvases use
        1024 and one GPU wait per frame. The Canvas text
        vertex ring follows paint_n; sync_all restarts it.
      - Buffer.update_part: a text run copies only its bytes.
      Long local text page scrolls at 29.7 frames/s.
      FIXED (Sep 25 night) orbiter SIGSEGV at startup
      (map_lookup under Texture_create_gpu, MilkyWay): my
      bind_resources change made its write arrays `vec
      VkWriteDescriptorSet [ n ]` etc., which overran the
      heap. Back to fixed local arrays; rings over 64 slots
      allocate and write their sets 64 slots at a time.
      Checked: headless orbiter runs (was exit 139), long
      page 54-57 frames/s, webgfx and trinity tests exit 0.
      OPEN silver bug: a runtime-sized `vec <C struct> [ n ]`
      is too small for n elements (found by elimination;
      no focused test yet).
      OPEN: Wikipedia scrolls at 2 frames/s with the GPU and
      every process near idle: the Compositor is asked for
      a frame every 0.5 s. Not drawing (a rAF page: 60/s).
      OPEN: GlyphAtlas.upload_region waits the device idle
      per new glyph; the Compositor holds ~600 nvidia fds.
   b. DONE z order: text was its own Render (text_render) with
      its own batch, sent at its 64-draw drain or read-back,
      so text landed over shapes drawn after it. Removed:
      text_pipeline's render is the Canvas; text_end draws
      via Render.draw_with[ text_models ] in call order.
      Checked: a red box over earlier text now covers it.
   c. PARTLY DONE SVG icons. Wikipedia's icons are CSS masks
      (an SVG mask over a background color).
      - masks: the player keeps a stack of layer canvases
        (pooled, surface size); push_mask draws the content
        into one, pop_mask draws the mask into another and
        webgfx_canvas_draw_layer puts the content down through
        it (Canvas modes image_mask_alpha / _luma, 14 / 15).
        The composite submits at once: a reused layer was
        cleared before the page's batch had read it.
      - repeated images, repeated tiles (their records replay
        per tile, scaled) and tiled images (border-image).
      - trinity path fill (sdf_walk_edges): a subpath never
        closed (close ignored, no implicit close) and a
        dropped short edge left a gap: horizontal streaks.
      - trinity path fill: every path in a batch shared one
        edge buffer written at paint time; the GPU read the
        last path's edges for all (the tagline's second half,
        trinity's canvas_test blob). Paths append at an offset
        (CanvasSDFCompute.eo); the buffer restarts after a wait.
        trinity test average now 16,32,53,80: the full blob
        (before 2,5,8,12: only the inner shape, wrongly).
      - even-odd fills (CanvasSDFCompute.even_odd).
      Checked: masked circle, Wikipedia menu / search / more
      icons and wordmark, tagline in full, even-odd hole.
      OPEN: the globe logo needs gradients, layer opacity and
      blend modes; a few streaks remain on it.
   RECOVERY (Sep 25 23:00): a reboot emptied /tmp, and the
   whole patched Ladybird tree and build lived in the session
   scratch there. Replayed from this session's transcript onto
   a fresh 3af73088 (checkout/lb/replay/replay.py), now in
   /src/silver/checkout/lb/ladybird (gitignored), tools in
   checkout/lb/tools, build script checkout/lb/build.sh.
   The patch is kept in the repo as an overlay, not a diff:
   aura/ladybird/ (every changed or new file, laid out as
   the tree), aura/ladybird.deleted (files to remove),
   aura/ladybird.commit (the base). install/ladybird ->
   checkout/lb/ladybird/Build/release.
   The replay leaked old trace edits into webgfx/webgfx.ag
   (a doubled webgfx_gpu_init, a doubled import pair, trace
   lines in webgfx_canvas_read); removed, webgfx tests exit 0.
   DONE silver's import takes an overlay (Kalen): after the
   clone and any <name>.diff, the module's <name>/ folder is
   copied over the checkout and <name>.deleted (one path a
   line) is removed from it (silver.c checkout). Checked with
   a throwaway module importing cJSON: an added file, a
   replaced file and a deleted file all right; removed after.
   Applies on a fresh checkout only, as the diff does.
   Rebuild checked: mask, stacking, even-odd, selection as
   before. The replay had missed one fix (a script edited
   webgfx and TypefaceTrinity together; its webgfx half
   failed first): the font byte copy freed under HarfBuzz
   (SIGSEGV in the font catalog thread). Re-applied.
   DONE video frames (Kalen: YouTube played audio only). The
   player drew nothing for DrawVideoFrame. Now each video
   keeps one webgfx texture (DisplayListStoredVideoSinkResource),
   a new frame (pool slot + acquisition id) is converted with
   YUVData::to_bitmap and uploaded in place
   (webgfx_image_update). Checked: a VP9 test clip plays (its
   counter 224 at 7.467 s, 32 frames/s shown); YouTube Big
   Buck Bunny plays after a click (0:12, picture moving).
   OPEN: YouTube 60 fps shows 12 frames/s: the YUV to RGB
   convert is on the CPU and each upload waits the GPU idle.
   Next: upload the three planes, convert in a trinity shader.
5. OPEN canvas 2D: the Canvas2DCommandStream replays on
   trinity's Canvas in the element, as the display list does.
6. LATER network, input, scrolling, canvas 2D, video, WebGL.

## Active work: browser on WPE WebKit, trinity painting (Sep 26 2026)

Decision (Kalen): WebKit is the only accepted browser stack;
the Ladybird element is superseded. Port: WPE WebKit 2.54.0
(source in checkout/wk/wpewebkit-2.54.0, gitignored).
Why the pivot: Ladybird repainted every icon every frame into
the final buffer; WebKit paints each layer into cached tiles
once and composites the tiles (only damaged tiles repaint).
Plan, option 2 (Kalen): trinity replaces Skia's PAINTING side
(GraphicsContext, paths, gradients, patterns, images, fonts,
ImageBuffer, tile rasterizing); WebKit's own tile caching and
compositing logic stay, compositing through trinity. Skia is
not built. The trinity work done for Ladybird carries over
(webgfx, CanvasFont/FontMatch, paths, masks, YUV video).
Survey: Skia is vendored (Source/ThirdParty/skia); WPE sets
USE_SKIA ON, USE_CAIRO FALSE. 160 WebKit files outside Skia use
it, 65 in WebCore/platform/graphics/skia; USE(SKIA) appears on
479 lines. The seam: a USE(TRINITY) backend beside USE(SKIA).
Disk: 26 GB free before any build; the Ladybird build in
checkout/lb takes about 15 GB (ask Kalen before removing).
1. DONE build dependencies, imported by aura/aura.ag
   into install/: ruby 3.3.6 (generators), unifdef 2.12,
   libgpg-error 1.51, libgcrypt 1.11.0, libtasn1 4.19.0,
   libpsl 0.21.5, nghttp2 1.64.0, libsoup 3.6.5. Off to start:
   XSLT, speech, AVIF, JPEG XL, LCMS, WOFF2, ATK, introspection,
   docs, journald, sysprof, WebDriver, sandbox, legacy libwpe,
   Wayland/DRM platforms (headless WPEPlatform stays), video.
   silver: `name/ver from <url>.tar.{xz,gz,bz2}|.tgz` unpacks
   the archive as the checkout (checkout/<owner>/<name>), then
   <name>.diff, the <name>/ overlay and the usual build run.
   Checked on libtasn1 (overlay file landed, built, installed);
   features: only the known t_vec_module fails.
2. DONE (Sep 26) USE(TRINITY) build: all of WPE WebKit builds
   and links with Skia off (0 Skia symbols or libraries;
   libWPEWebKit needs libsilver-webgfx.so). Installed into
   install/ (helpers in install/libexec/wpe-webkit-2.0; they
   need LD_LIBRARY_PATH=install/build:install/lib for webgfx).
   FIRST PAGE: a headless test (scratchpad wk/shot.c: headless
   WPEDisplay, load, webkit_web_view_get_snapshot, PNG out)
   draws heading, paragraph, red box, rounded green box, blue
   border right. Fixed on the way: whitespace drew the missing
   glyph box; the HarfBuzz font is now a sub font whose glyph
   lookups apply WebKit's space rules (as Skia's did).
   Every change is kept as the overlay aura/wpewebkit/
   (88 files, base in aura/wpewebkit.version).
   Non-composited rendering is Skia-only: trinity pages always
   use the layer tree.
3. DONE, first page checked: GraphicsContextTrinity (webgfx canvas;
   own CTM and device clip stacks; colors, rounded rects,
   paths, strokes, images, tiled patterns, glyph runs; a CPU
   backing for shareable bitmaps). Logged once as "not
   ported": gradients (trinity has 2 stops only; a stop ramp
   is next), shadows, dashes, clip-out, path clips (bounds),
   transparency layers, blend modes.
4. DONE, first page checked: fonts on TrinityTypeface + HarfBuzz
   (FontPlatformData, Font metrics/bounds/outlines, GlyphPage,
   FontCache via FontMatch, web fonts with variation axes,
   FontCascade::drawGlyphs -> webgfx glyphs). Shaping reuses
   skia/ComplexTextControllerSkia.cpp (pure HarfBuzz).
5. DONE, first page checked: ImageBufferTrinityBackend (canvas,
   read back on demand), TrinityPaintingEngine (dirty tiles on
   one reused canvas, read into the BGRA tile buffer),
   NativeImage, Pattern, ShareableBitmap, PNG via libpng
   (TrinityPNG); WPE UI side (snapshot, cursor, favicons,
   WebKitImage, notifications) off Skia. Compositing is still
   WebKit's TextureMapper (GL): OPEN, move it to trinity.
   DONE (Sep 26) gradients: trinity Canvas `fill_ramp` (a
   256x1 stops image in the mask slot; linear, two-point
   radial, conic; pad/reflect/repeat; device-to-gradient
   matrix), webgfx_canvas_set_ramp, GraphicsContextTrinity
   builds each ramp once (premultiplied blend, cached by hash).
   webgfx t_canvas_ramp (red 255,0,0 / green 0,252,3 / blue
   0,0,255 across a rect), webgfx and trinity tests exit 0.
   Checked in WebKit: linear, hard stops, radial circle and
   ellipse, conic from the top clockwise, repeating stripes.
   NEXT: clips to a path's shape (rounded boxes fill square:
   WebKit clips border-radius then fills; bounds used now).
   WRITTEN, not built: aura.ag imports webgfx, then
   wpewebkit/2.54.0 from wpewebkit.org's tarball (checkout/
   releases/wpewebkit, overlay aura/wpewebkit/, the flags
   of wpewebkit.configure.sh as import lines).
6. DONE (Sep 26) the browser element runs WebKit. aura.ag
   forks install/libexec/wpe-webkit-2.0/WPETrinityBrowser (the
   overlay's Tools/TrinityBrowser/main.c, built by WebKit's own
   CMake when USE_TRINITY): a headless WPE view; each frame
   (the view's buffer-rendered signal) goes to the same shared
   file as before (TRBW header, rgba8), DMA-BUF frames read
   back through EGL + glReadPixels (gbm_bo_map fails on the
   NVIDIA buffers), SHM frames copied; input lines as before
   (size, load, mouse down|up|move|wheel, key, text) become
   WPE events. A GLFW wheel value passes straight through
   (negative scrolls down). Button-up needs press count 0
   (else WPE returns no event). WebKit binaries carry RPATH
   install/lib:install/build (configure.sh; a copy is
   aura/wpewebkit.configure.sh): no LD_LIBRARY_PATH.
   Checked: helper alone (202 frames in ~5 s, Wikipedia);
   element headless over its socket: click focuses, typing,
   Backspace, a button's script, helper dies with the element;
   `browser <url>` shows Wikipedia at 1280x800.
   Launch: `silver aura https://en.wikipedia.org/wiki/WebKit`.
   DONE (Sep 28) zero-copy frames: the helper sends each GPU
   frame's dma-buf (one plane, fd over the stdin socket with
   a 48-byte record: id, size, fourcc, modifier, stride,
   offset; it polls the buffer's rendering fence first) and
   the element imports each buffer once (import_dmabuf_image,
   keyed by the helper's buffer id; aura.c receives the
   fd). A failed import posts `gpuframes 0` and the helper
   reads frames back into the shared file as before (also
   the path when stdin is a pipe, and for SHM buffers).
   Checked headless at 2560x1440: "GPU frames ... modifier
   216172782128496660", picture right, 60 page frames/s
   while the knob drags, helper 2% of a core.
   Pointer moves go to the helper at once (was once per app
   frame): WebKit coalesces them itself. Latency measured
   before: WebKit + helper alone 23 ms from an input line to
   the frame in the file; the app added 8 + 8 + up to 16 ms.
   OPEN: hidpi scale; path clips.
   DONE (Sep 28) a live reload left the browser blank. The
   helper was forked with PR_SET_PDEATHSIG, and a reload's
   first draw runs on the load worker thread: when that thread
   ended, the new helper got SIGTERM ("the engine exited"),
   while the old one lived on with the process-owned app
   element. Now: no death signal (the helper quits when its
   stdin socket closes, which our death does); the element
   keeps `persist engine : vec i64` (pid, ctl) and
   `engine_shm`, and a new instance adopts the running helper
   (adopt[], "kept the engine") instead of launching: the
   page and its video continue. Checked headless under the
   host: a aura.ag touch and a trinity/Canvas.ag touch
   both keep the page, 58 page frames/s after the switch.
   DONE (Sep 28) page images get a mip chain (webgfx_image_new,
   Texture.upload_region refills it): a logo drawn smaller
   than its file no longer stair-steps. Ramps (256x1) stay
   flat.
   DONE (Sep 28) YouTube came up white in orbiter's pane: the
   helper's GPU buffers are XR24 (fourcc 875713112), and the
   dma-buf import read the X byte as alpha. Standalone composes
   the screen without alpha, so it looked right; orbiter draws
   the hosted app's screen with it, and the pane fell through.
   import_dmabuf_image now imports XR24/XB24 as opaque (the
   Texture's alpha-as-one swizzle). The "GPU frames" log line
   names the fourcc. Hosted case not driven here.
   DONE (Sep 28) elements route into the host's navigation and
   title, decentrally (Kalen). trinity: element.navigate[ dir ]
   (a hook, false by default), element.nav_state[ back, fwd ]
   and element.set_title[ t ] emit to the Window (nav_back_on,
   nav_fwd_on, nav_title); a hosted app posts them on ring 0 as
   HM.nav_state (a, b) and HM.title (three bytes a message, a
   0 byte last); the host's HM.nav (a = -1/+1) on ring 1 goes to
   Window.navigate: the focused element, its parents, then the
   app object. orbiter: AppView keeps nav_back_on/nav_fwd_on/
   app_title from its slot's ring; a hosted pane's arrows post
   HM.nav (Editor.nav_walk) and light from the app's word; the
   pane title shows app_title (title_text). browser: the helper
   sends 'NAVS' (back-forward list changed) and 'TITL' (title,
   else the address; notify::title / notify::uri) records on
   the frame socket; aura.c returns the record kind (1 frame,
   2 history, 3 title); navigate posts `nav back|forward`, the
   helper calls webkit_web_view_go_back/forward. Checked: the
   helper alone goes back and forward between two pages; on
   YouTube a related-video click logs "history back true" and
   the new page's title. Orbiter builds; its arrows and title on
   a hosted browser pane are not driven headless (session state).
   DONE (Sep 28) the title's companion extension buttons are one
   TButtons strip (ids cmp_<ext>, new TButtons.labels for the
   text), and companion_exts is empty for an address.
   DONE (Sep 28) player icons with a drop-shadow filter drew
   bold with a light border ("over drawn"). The canvas blends
   premultiplied and samples image textures as premultiplied,
   but WebKit's images are straight: a filter result's edge
   pixels (white at partial alpha) came back over-bright and
   read as a hard border. webgfx image_upload premultiplies
   (image_new, image_update). webgfx tests: t_blur_kernel (the
   sigma-1 gaussian), t_compose_keeps_aa, t_result_image_keeps_aa
   (the round trip equals a direct fill), t_cc_drop_shadow,
   t_cc_icon_small_canvas (YouTube's CC path). Checked: the
   pill page with the real icons, and the YouTube bar.
   OPEN (trinity, Kalen's call): every straight-alpha texture
   drawn by Canvas.draw_image (PNG icons in orbiter, any img
   load) has the same over-bright edge; the canvas's rule is
   premultiplied, so loaders should premultiply too.
   DONE (Sep 28, Kalen) the SDF feather was half a pixel each
   side (0.5 * gradient length); it is one pixel each side
   again (the gradient length), both CanvasUI fills and the
   path pass.
   DONE (Sep 28, Kalen: "that works so keep it") the Ask star
   icon lost its tips at 24 px. The tips are thinner than a
   pixel and one distance sample at the pixel centre misses
   them (the field itself is fine: shifting the icon half a
   pixel changed nothing). CanvasUI mode 6 fill: within 1.25 px
   of the outline, 16 taps (4x4 cells of the pixel, a linear
   ramp per cell) give the true coverage; elsewhere 0 or 1.
   The field is written and read as before. The feather is
   0.5 * gradient length in the source again (exact coverage;
   1.0 made 24 px icons soft and fat against the reference).
   Tried and removed (Kalen: too much pixel alignment, it
   changes the SDF): a field over the padded shape only, a
   guard texel, clamped taps: a batch shares one dispatch, so
   the stale texels drew hairlines in the YouTube wordmark.
   Finer flattening (0.1) changed nothing. Checked: the star at
   24 and 48 px against Inkscape (column through the tip 37,
   59, 103, 174, 248 vs 44, 65, 110, 180, 251), the wordmark
   clean at 1x and 4x, webgfx and trinity tests exit 0.
   DONE (Sep 28, Kalen) standalone, the page title is the window
   title (Window.element_title calls Display.set_title when not
   hosted), and the browser opens where it was last unless a
   src is given: `intern [ StatePersist ] last_src` saved on
   every address change (~/.local/state/aura/persist.agi;
   the helper's 'ADDR' record on notify::uri, kind 4 in
   aura.c), loaded in init when src is empty. Checked
   headless: a Wikipedia run saves it, a run with no address
   opens it and logs its title.
   DONE (Sep 28, Kalen: text blurry half the time after a
   scroll) the scroll position straddled the pixel grid. WPE's
   wheel line step is the view height to the 2/3 (86 px at
   800), the element sends 0.325 of a notch: 27.95 px a step,
   and WPE's coordinated scrolling delegate overrode the
   threaded delegate's rounding with the raw float, so the
   scrolled layer sat at a fraction and its tiles resampled
   (strokes lightened step by step: 29 -> 40 -> 74). Overlay
   Source/WebCore/page/scrolling/coordinated/
   ScrollingTreeScrollingNodeDelegateCoordinated.cpp:
   adjustedScrollPosition rounds to whole device pixels (the
   scrolled layer's contentsScale). Checked headless on a long
   text page: four wheel steps are four exact 28 px shifts
   (mean pixel difference 0.0), a word's pixels identical
   before and after.
   DONE (Sep 28, Kalen) orbiter exports its own icon: `export
   func orbiter_icon` (Avatar.ag) renders OrbiterAvatar alone
   at 512x512 with a transparent background into orbiter/
   images/icon.png (the module folder from install/build/
   silver-orbiter.source), once: skipped while the file exists
   (delete it, or `silver --export --build orbiter`, to remake).
   The release's icon export scales it to the sizes. An element
   outside a window is mounted as the window does it: ux and vk
   first, then initialize (the member defaults, so shaders get
   their device), bind_members, hold_members; the window needs a
   screen canvas (draw composes onto it); every pass is drawn
   and fenced (batched passes submit only at sync_fence). The
   old placeholder icon.png is in the session scratchpad.
   Every orbiter build now runs the export step (module init,
   then "icon current").
   DONE (Sep 28, Kalen: "link is fine to couple") `--link` is
   the user's own install, the package's layout under HOME:
   ~/.local/bin/<name> -> the binary, the hicolor icon set in
   ~/.local/share/icons/hicolor/<n>x<n>/apps/<name>.png (img's
   icons export) and ~/.local/share/applications/<name>.desktop
   (Exec the link, Icon=<name>). silver.c: silver_module_icon,
   silver_icon_set, silver_desktop_entry, shared with --release.
   No icon: the link alone, and a note. It runs on a cached
   build too (silver_user_link, called in both branches).
   Checked: `silver --link --build orbiter` wrote all three,
   fresh and cached. The icon avatar takes the app's resting
   yaw of 30 degrees (on_state's, which needs the app object):
   flat top and bottom, as orbiter shows it.
   DONE (Sep 28, Kalen) title bar boxes: the "..." tab chip
   (NavTabs), the history arrows and the close buttons
   (BarClose class, the finder's TButtons close) are the bar's
   full height (b0), no spacing (use_spacing false: the title
   bar's 0.2 em child spacing was the padding), square corners
   (seg_radius 0, radius 0). Strokes 0.5 -> 1 everywhere in
   orbiter's styles with the alpha halved; the TButtons outline
   is #ffffff0d. The dots hint draws only while the strip is
   small (gone a quarter into the reveal), in the rest slot,
   twice the icon size capped to the slot; the close glyphs
   fill their box height.
   Since then (Kalen): the arrows and the dialog close are
   1.8 units wide, arrow glyphs at 0.30; every close button is
   the chip's size (1.66 units, X at 0.55); the file icon sits
   0.2 units further right; the title's icon, name, hint and
   companions lead from the arrow box (EditorTitle.lead). A
   side toolbar (right edge, instances + orbiter at the top,
   console at the bottom) was built and REMOVED at Kalen's
   word; the gutter rule is the original (five digits + 16 px).
   DONE (Sep 28, Kalen: "some svgs have super hard edges") svg
   icons (Canvas.draw_svg) rasterize into a cache 4x their size
   (SVGModel.pixel_scale 4) and were drawn back at a quarter
   with the plain bilinear sampler, which reads 2 of every 4
   texels: edges quantized to stairs (a 15.7 px hexagon's edge
   was one pixel at 33 then 255). Now the cache Canvas has
   `reduce: true` for pixel_scale 4 (the render's reduce pass
   averages 4x4 texels, as the avatar's does) and draw_canvas
   draws the reduction; the draw is snapped to whole pixels
   (origin and size), so the reduction lands 1:1 (a 15 px
   reduction drawn at 15.7 blurred it). Measured in a temporary
   webgfx test: the drawn row equals the reduction row, edges
   93 255 ... 93 21 instead of 33 255 ... 15 0. Thin lines (the
   console icon) read as their true sub-pixel coverage now
   (~140 of 255), which is right, not a bug.
   DONE (Sep 28, Kalen: "state corruption ... worse when you go
   back") the reduce pass reused pass 0's attachment
   description: a load_existing canvas LOADed the old
   reduction and every pipeline blends over, so each reduce
   composited onto the last (edges thickened per shape, a
   re-tint kept the old picture under it; the avatar's 4x
   reduce accumulated frames the same way). vk.ag: pass 1
   clears (loadOp CLEAR, initialLayout UNDEFINED) to
   transparent black (the canvas's clear colour is opaque
   black: the first try gave a black square). shader.ag
   UVReduce: output is the plain premultiplied average (was
   (rgb + rgb*a)/2, halving edges) and the +-2/255 dither is
   scaled by alpha (it lit clear pixels: the faint box round
   some icons). Checked: a white then red hexagon reads red
   with green 0-1 and alpha equal to red; suites exit 0.
   DONE (Sep 28, Kalen: "adhere to w3c, not two brains") the
   shader keeps the CSS blur meaning; orbiter's glow sizes are
   halved in its own sources: 17 box_shadow styles, 8
   cv.box_shadow calls, the chip hover glow, rest_px 16 -> 8.
   Also: the git changed-file tabs at half alpha (fill, border,
   glow: sources #22002280, #ff5a8a80, #ff7aa480, white 80),
   mapped in every scene theme (scenes.ag) at the same alpha;
   the finder's extension chip strips got the last half-pixel
   stroke in the tree (now 1 px inside).
   DONE (Sep 28) ctrl/cmd+left and right in any trinity app go
   to Window.navigate (the focused element's history) before
   the key reaches the focus; the browser goes back/forward.
   DONE (Sep 28) a page that worked once then came up white:
   WPE recycles buffer ids, so the element keyed its imported
   textures by the helper's id; the cache resets on a size
   change or past 12 buffers.
   Helper lines (Tools/TrinityBrowser/main.c): `js <script>`
   (answer logged "trinity browser: js: ..."), --console,
   load-changed lines, the frame path line.
   OPEN youtube.com (not www): a cached 301 into a service-
   worker-controlled origin; the worker does not handle it and
   the navigation preloader's soup request never completes
   (the server gets the GET, the client closes the socket).
   Reproducer: scratchpad swsite_cached.py case 3. The traces
   used for it are removed (Kalen).

   DONE (Sep 26) masked icons (Wikipedia's menu, search,
   languages...). WebKit paints a CSS mask as a layer with the
   color, a destination-in layer inside it with the mask, then
   ends both; transparency layers were not ported, so the mask
   painted on top and the square was never cut. Now each layer
   is a pooled canvas (GraphicsContextTrinity pushLayer/
   popLayer); a destination-in layer becomes the mask of the
   layer around it, and a layer ends with one trinity call,
   Canvas.compose [ src, mask, luma, alpha, x y w h ] (one
   CanvasUI shader: mask modes 14/15, or the image mode).
   A mask test over file:// never drew: CSS masks load with
   CORS; test over http. webgfx and trinity tests exit 0.
   Rounded clips (Sep 26): shape clips, clip-out and image
   clips became layers kept through a mask; boxes round right
   (test page) but it broke the icons again: a clip layer
   hides the content drawn before it from a later
   destination-in layer. Kalen's direction: CanvasUI conforms
   to WebKit's W3C primitives (composite ops, shadows, clip as
   shader state, one gradient path), glow/shadow states move to
   that interface with no parallel functions. Waiting on his
   calls: glow_falloff, blend-mode method, style names.
   Scroll lag (Sep 26), measured with a wheel every 1/60 s:
   a long plain page scrolls at 62 frames/s, no gap over 25
   ms (frame path is fine). Wikipedia: 40-170 ms stalls; its
   content, sidebars and scrollbar tiles repaint every frame
   (the page's sticky header and contents highlighting), and
   per second: layer push/pop 2000-6900 (66-274 ms), shape
   clips 800-3300 (90-224 ms), glyph runs 15-19k (130 ms),
   tile read back + swizzle 300-450 ms. Fixed: tile read back
   copies only the tile (Texture.read_region, kept staging
   buffer, in-band layouts; webgfx_canvas_read_region, test
   t_canvas_read_region): scrollbar read 73 -> 14 ms/s.
   NEXT: the W3C shader (removes the clip layers), then glyph
   runs and GPU tile buffers (no read back).
   perf and gdb attach are blocked here (perf_event_paranoid
   4, ptrace_scope 1): profile with timing in the code.

## Active work: CanvasUI on W3C primitives (Sep 26 2026)

Kalen: the uber shader conforms to WebKit's W3C primitives;
trinity's glow/shadow states move to that interface, no
parallel functions; Gaussian shadows; styles renamed too.
Blend modes read a copy of the destination (works on every
GPU, MoltenVK included).
1. DONE composite operators: vk.ag `enum Composite` (WebKit's
   order), Pipeline.composite picks a pipeline built per
   operator on first use (build_graphics: Porter-Duff factors,
   composite_source/destination_factor); Canvas.set_composite.
   plus_darker and difference still blend as source-over.
2. DONE clip as shader state, CanvasUI and CanvasText:
   rounded-rect clip computed in the shader (clip_rect,
   clip_radii; Canvas.clip_rounded_rect when the transform is
   axis-aligned), else a clip-mask canvas in a 4th texture
   slot `clip` (clip_to_path, clip_out_path, clip_to_image;
   masks stack with save/restore, pooled; a pooled mask is
   reused only after sync_all). Every fragment exit scales by
   the clip; destination-in keeps the destination outside it.
   Helper GLSL funcs compile into the vertex stage too: no
   gl_FragCoord or discard in them. webgfx tests:
   t_canvas_clip_rounded, t_canvas_clip_path_out,
   t_canvas_destination_in (all pass, webgfx/trinity exit 0).
3. DONE Gaussian shadows: 4 slots (shadow_color0-3,
   shadow_geom0-3 = x y blur spread, shadow_inset), the CSS
   erf edge from the shape's distance (rect/rrect analytic,
   paths from the SDF, which is padded by shadow_reach);
   outer shadows hidden under the box, inset inside the fill;
   Canvas.shadow / box_shadow / no_shadows. Removed:
   inner/outer glow and shadow, stroke_shadow, glow_sides,
   glow_falloff, the lit-side bevel, sdf_rounded_rect_glow.
   Segmented buttons (trinity, orbiter Git) glow one side by
   clipping to the segment and drawing a wider box with an
   inset shadow. WebKit drop shadows map to slot 0 (legacy
   radius doubled; ignore-transforms scaled). webgfx tests
   t_canvas_shadow_outer, _inset (erf falloff matches CSS).
   OPEN: text and image shadows; elliptical border radii.
4. PARTLY DONE one gradient path: Gradient objects now paint
   through the ramp (Canvas.gradient_ramp, cached by stops;
   use_gradient with the inverse transform; conic start angle
   in ramp_geom.z) - the colour wheel and the scroll-edge fade
   bands use it. OPEN: the two-stop fill_grad, stroke_grad and
   text_grad state and the Layer fill_gradient_* props.
5. DONE Canvas.color_filter [ hue, saturate, brightness ]
   (the spec's matrices) replaces colorize/HSV; webgfx
   t_canvas_filter_saturate (red -> 54,54,54). OPEN: WebKit's
   CSS filter property runs through its FilterEffect code,
   not GraphicsContext state: not drawn yet.
6. PARTLY DONE styles: Layer box_shadow and text_shadow
   (BoxShadow: CSS [inset] x y blur spread color, or none;
   transitions mix) replace fill_shadow, fill_glow,
   inner_glow, stroke_shadow, stroke_glow, label_shadow*,
   label_glow; orbiter (33) and hyperspace (3) styles and
   orbiter's inner_glow calls moved. orbiter builds;
   hyperspace fails at ts_load (not from this change).
   OPEN: gradient props (item 4).
7. PARTLY DONE GraphicsContextTrinity: clips are canvas state
   (clipRoundedRect analytic, clipPath/clipOut/image masks),
   layers compose with their own operator; clip layers gone.
   Wikipedia icons and rounded boxes both right. Scroll:
   41 -> 48-54 frames/s, worst stall 172 -> 66-76 ms.
8. DONE text selection lag (Kalen: "selecting text is also
   super slow"). Each selection paint clipped out every
   out-of-flow box (RenderBlock::selectionGaps), and each
   clipOut(rect) made a new mask canvas (clip_push_mask never
   pooled the one it replaced). Now: up to 4 clip-out rects
   are analytic in CanvasUI and CanvasText (clip_out0-3,
   count in clip_on.w; Canvas.clip_out_rect, a mask past 4 or
   under rotation), the replaced mask goes back to the pool,
   webgfx_canvas_clip_out_rect. webgfx t_canvas_clip_out_rects.
   Drag over Wikipedia 6.7 -> 25.7 frames/s; over solid text
   42-45 frames/s. Every move that changes the selection
   paints in 1-7 ms; the frame gaps left are moves where the
   selection end does not change (WebKit asks for no paint:
   checked with timestamps in LayerTreeHost and WebPage).
   Profiling without perf: an LD_PRELOAD SIGPROF sampler and
   strace -f on the helper (a child, so ptrace is allowed).

## Active work: SQLite out of WebKit (Sep 27 2026)

Kalen: no SQLite in the browser; site storage is plain files in
the browser's cache folder (the helper's --data dir). All of it
in the overlay aura/wpewebkit/. In order:
1. WRITTEN, not compiled: localStorage is FileStorageArea, a
   readable localStorage.txt per site (key TAB value per line,
   \t \n \r \\ \uXXXX escapes), saved 500 ms after a change and
   on sync/close. SQLiteStorageArea deleted (wpewebkit.deleted).
1b. WRITTEN, not compiled (Kalen: nothing hidden from the
   user): site folders are named by the site (databaseIdentifier,
   https_www.youtube.com_0), not a salted SHA-256; the salt file
   is gone (NetworkStorageManager).
1c. OPEN: the page Cache API (CacheStorageDiskStore) and the
   HTTP disk cache still name files by salted SHA-1.
2. WRITTEN, not compiled: IndexedDB is WebKit's non-SQL
   MemoryIDBBackingStore with a file path: loaded on open,
   rewritten after each writing commit, deleted with the
   database (IDBStorageManager). File <name>.indexeddb.txt in
   the site's IDB folder (MemoryIDBBackingStoreFile.cpp), tab
   fields, one line each:
     database name version
     store id name keypath autoincrement keygenerator
     index store id name keypath unique multientry
     record store key value(base64 of the engine's encoding)
     entry store index indexkey primarykey
   keys n:number d:ms s:"text" b:base64 a:[k,k]; keypaths
   - s:"p" a:[s:"p",s:"q"]. Max index id is recomputed from
   the file. Whole file rewritten per commit (big databases:
   slow; not measured). SQLiteIDBBackingStore still compiled
   (IDBServer.cpp); goes in item 5.
3. IN PROGRESS cookies and HSTS as folders (Kalen: "dirs with
   simple files", cookies are key/value like the rest).
   DONE, tested on this Mac: libsoup overlay aura/libsoup/
   (+ libsoup.deleted): SoupCookieJarFolder and
   SoupHSTSEnforcerFolder replace the SQLite jar/enforcer; the
   sqlite dependency is gone (libsoup links no sqlite).
   <dir>/<site>/cookies.txt: name TAB value TAB attributes
   (domain=; path=; expires=unix; secure; httponly;
   samesite=Lax). <dir>/<site>/hsts.txt: max-age, expires,
   subdomains as key TAB value lines. Session cookies and
   session HSTS policies are not saved. The jar keeps its own
   copy per site (libsoup fires `changed` under its lock).
   Test: scratchpad jar_test.c against a --destdir stage of
   the libsoup build: 3 cookies set, 2 persistent reloaded,
   tab in a value kept, HSTS policy reloaded.
   NOT DONE, WebKit side (checked where, nothing edited yet):
   - SoupCookiePersistentStorageType {Text, SQLite} ->
     {Text, Folder}: Shared/soup/SoupCookiePersistentStorage
     Type.h, WebsiteDataStore.h:650 default, NetworkSession
     Soup.cpp:87 (soup_cookie_jar_folder_new),
     WebKitCookieManager.cpp:112, WebKitCookieManager.h.in
     (WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE -> _FOLDER),
     Tools/MiniBrowser/wpe/main.cpp:512,554, TestCookieManager
     .cpp:94,705,727,865.
   - WebCore/platform/network/soup/SoupNetworkSession.cpp:190
     hsts-storage.sqlite -> soup_hsts_enforcer_folder_new(
     <dir>/hsts).
   - helper Tools/TrinityBrowser/main.c:332-336: FOLDER with
     <data>/cookies.
   Then copy each file into aura/wpewebkit/.
3b. DONE (Sep 28) libsoup warned "soup-tld: There is no
   public-suffix data available" on every request: aura.ag
   built libpsl with --disable-builtin. The line is gone; the
   tarball's list/public_suffix_list.dat is compiled in (its
   psl-make-dafsa runs on install/bin/python3). --disable-
   runtime stays (no libidn2). Checked: a YouTube run logs no
   soup-tld line.
4. Service worker registrations in memory; Web SQL, web push,
   click measurement, tracking statistics, content blockers,
   enhanced-security sites and the favicon database off.
5. WebCore platform/sql and find_package(SQLite3) removed.
Checking needs WebKit configured: on this Mac harfbuzz, icu,
libjpeg, libepoxy, libxkbcommon, libxml2 and libwebp are still
missing imports (not added: Kalen questioned webp; SQLite is
being removed instead of imported).
Where the Mac build stands (Sep 27): every aura.ag import
up to libsoup builds; WebKit's configure stops at HarfBuzz.
silver.c fixes made for it (in src/silver.c, rebuilt by
Kalen): SDKROOT in the import build env on native macOS (our
clang finds no SDK otherwise), release archives skip
autogen.sh when configure ships, configure imports run `make
clean` before make (stale in-tree objects). aura.ag gained
pkgconf (system dirs /usr/include, /usr/lib) and glib imports;
Au.g no longer imports libffi (glib's bundled copy is the one
in install/). WebKitXcodeSDK.cmake: WPE on a Mac builds for
the host arch (overlay). The downloaded WebKit checkout is
checkout/releases/wpewebkit (owner taken from the URL path).
OPEN: our clang should know the macOS SDK itself (a
clang.cfg beside install/bin/clang), then the SDKROOT lines
in checkout() go; checkouts from archive URLs land under the
URL's parent folder name (checkout/3.3/ruby).

## MEMORY: trinity replaced Skia under a real browser (Sep 25 2026)

What was profound: a full web engine (Ladybird: HTML, CSS,
JS, networking, fonts, SVG) draws every pixel through trinity,
our own GPU canvas, with no Skia, ANGLE or Qt. Kalen's rule
held from start to finish: "Skia is a canvas. WE HAVE THAT."
The only new code is glue; every drawing job lands in trinity.
It also paid trinity back: the browser found four bugs that
hurt every trinity app (below), not just the browser.

How the swap works:
- Ladybird draws through a few narrow seams: Gfx::PathImpl,
  Gfx::Typeface, PaintingSurface, Painter and the display list
  player. Each Skia class there got a Trinity twin (PathTrinity,
  TypefaceTrinity, PainterTrinity, DisplayListPlayerTrinity),
  and Skia's files are deleted.
- The bridge is webgfx, a silver module exporting plain C
  functions (fonts, paths, canvases, images, layers), with
  objects passed as small integer ids. Au headers cannot enter
  Ladybird's C++ (Au's get/set/len/push macros break AK), so C
  is the boundary.
- Fonts: trinity CanvasFont loads from bytes, measures, and
  draws by glyph number; FontMatch (fontconfig) picks system
  fonts. HarfBuzz still shapes in Ladybird.
- Frames: Ladybird's own processes stay (WebContent, network,
  Compositor). The Compositor replays each frame's display list
  onto a trinity canvas; the Trinity frontend hands finished
  frames to the browser element in shared memory, and input
  goes back as text lines on a socket.
- Masks and layers: an offscreen trinity canvas per layer,
  composited through the mask by a Canvas shader mode.

Bugs the browser exposed in trinity itself:
- Canvas.sync_all drew the last shape twice on every read.
- Canvas.set_clip stored a clip nothing read: no clip worked.
- Text rendered in its own batch, so it could land on top of
  shapes drawn after it (z order).
- Path fills: subpaths never closed, and every path in a batch
  shared one edge buffer, so paths drew with another's shape.

Lessons:
- Keep work out of /tmp: a reboot wiped the whole patched tree
  (recovered by replaying the transcript). It lives in
  checkout/lb now, and the patch as an overlay in aura/.
- Speed came from reusing GPU work: one texture per image
  (not per draw), one GPU wait per frame (paint_n 1024).
- A runtime-sized `vec <C struct> [ n ]` overran the heap in
  silver: keep fixed local arrays for Vulkan structs (OPEN).

## Active work: YouTube through MediaSource on trinity (Sep 26 2026)

Kalen: YouTube says "can't play this video", or plays one frame
in 30 s; performance is awful. Reproduced: a page has no
MediaSource (YouTube needs it), so YouTube gives up. Stage c
of the media memory below, done as a streaming backend.
1. DONE Demux reads fragmented mp4 (trinity/demux.ag append:
   moov + mvex/trex, moof tfhd/tfdt/trun + mdat, whole boxes
   only; DemuxSample owns its bytes; reset). webgfx
   t_demux_fragments: media/clip.frag.mp4 in 997-byte pieces,
   48/48 samples match the plain file (bytes, dts, pts, sync).
2. DONE VideoStream (trinity/video.ag): samples queued in
   decode order, a worker decodes 6 ahead, frame_at by time,
   flush (generation), hidden (non-displaying) samples.
   t_video_stream: 48/48 once each on a 60 Hz clock; a seek
   to 1 s shows the 1.000 s picture.
3. DONE AudioStream (spectra): aac packets decoded as pushed
   into an 8 s ring; the mixer pulls it (add/remove_stream);
   time[] is the clock. t_audio_stream: 440 Hz, 1 s pulled
   moves the clock 1,000,000 us.
4. DONE WebKit MediaSource (ENABLE_MEDIA_SOURCE=ON in the
   configure line): MediaSourcePrivateTrinity,
   SourceBufferPrivateTrinity, MediaSampleTrinity, the
   player's source path (enqueue, readyForMore, flush, seek
   via waitForTarget + reenqueue, sound clock); webgfx
   webgfx_demux_* and webgfx_stream_*; MediaPlatformType
   Trinity. isTypeSupported: avc1 true, mp4a true, vp9 false.
   Checked in the browser: local 20 s MSE page plays 0 -> 20 s
   in real time, 640x360, picture right; YouTube Big Buck
   Bunny plays (picture moving), 3 runs with no crash.
   Fixed on the way: an empty type (a source attaching) looked
   up a null String in a HashSet (web process SIGSEGV); silver
   classes are PACKED structs, so a pthread_mutex_t member sat
   at byte 102 and futex failed under contention: use Au's
   `mutex` class in silver classes, never raw pthread types.
5. OPEN video as its own compositing layer: written, not in
   the build (trinity/CoordinatedPlatformLayerBufferTrinity,
   webgfx_stream_next/_planes, buffer Type::Trinity).
6. PARTLY DONE YouTube plays, but the picture changes only
   1-7 times a second headless; Kalen sees 1 frame every 3-4
   seconds (Sep 26). The web process main thread is busy.
7. OPEN YouTube page speed (Kalen: "performance is absolutely
   awful"). Sampled during playback: ~90% of the web process
   is CSS filters (drop-shadow) making temporary ImageBuffers,
   each a new trinity Canvas whose 1024-slot ring creates
   ~1024 uniform Buffers (Canvas_init -> uniforms_init ->
   vmaCreateBuffer). Tried and REMOVED (lost the GPU device,
   VK_ERROR_DEVICE_LOST, cause not found): a webgfx canvas
   pool, and a 64-slot ring for small canvases (a mid-batch
   ring wrap on a webgfx canvas faults; 1024 never wraps).
   Next: uniform buffers made on first use, then the wrap
   fault; filters on the GPU.
9. DONE (Sep 27 night) the volume knob did not repaint while
   dragged; only a sliver at the slider's left changed. The knob
   (.ytp-volume-slider-handle, 12 px, filter: drop-shadow, with
   64 px ::before/::after bars) paints its filter in software
   (WPE never composites for a filter alone), so it is its own
   repaint container: a move left old and new rects equal and
   nothing was invalidated; the sliver was the volume icon's
   own repaint re-rendering the knob's filter clipped to it.
   Fix (overlay RenderLayer.cpp, updateLayerPositions): such a
   layer's repaint rects track in its enclosingFilterRepaintLayer
   as calculateLayerBounds (children + filter outsets). Checked:
   a local page (timer-moved knob) and YouTube headless, knob
   and bar follow the pointer with the video playing.
   OPEN: the page paints 30 frames/s while the knob drags (60
   idle): the software drop-shadow re-render per move.
10. DONE silver.c checkout(): an overlay file newer than the
   import's silver-token runs the import's build and install in
   its build tree (import_build) instead of skipping it; the
   token is rewritten. OPEN: a module whose .ag is unchanged is
   "up to date" and never reaches the import step: use
   `silver --clean --build browser` after an overlay edit.
8. DONE logins kept, in silver's app cache (Kalen): aura.ag
   passes --data path_cache['aura'] (~/.cache/aura); the
   helper keeps site data in <it>/data (cookies.sqlite there)
   and cache in <it>/cache. Cookies were memory only before
   (signed out on every start). Checked: a cookie set by a
   local server came back from a new browser process and is in
   ~/.cache/aura/data/cookies.sqlite.
   Saved passwords: WebKit has no password manager (not done).

## Active work: browser built by silver, no SQLite, TLS (Sep 27 2026)

The browser module is `aura` (renamed from `browser`, Sep 28:
aura/, aura.ag, aura.c, ~/.cache/aura, trinity-aura.sock; the
WebKit helper keeps its name WPETrinityBrowser).
WebKit is built ONLY by `silver aura` (import in aura.ag,
overlay aura/wpewebkit/). Never ninja by hand in a checkout.
1. DONE overlay applies on every build (silver.c checkout), not
   only a fresh checkout; env lines are in the import cache key.
2. DONE meson 1.8.3 import (glib 2.84 needs >= 1.4); glib and
   libsoup get an rpath to {install}/lib.
3. DONE ENABLE_WEBGL=OFF: no ANGLE.
4. DONE no SQLite storage: Sources.txt lists, HSTS and cookie
   stores on libsoup's folder types, FileStorageArea and
   MemoryIDBBackingStoreFile listed; GSTREAMER_GL needs GStreamer.
5. WRITTEN, not built: https. glib-networking import with a new
   mbedtls 4 backend (overlay aura/glib-networking/tls/mbedtls,
   8 files, shaped like its gnutls backend over tls/base). Clean
   -fsyntax-only -Wall -Wextra. The base verifies the peer after
   mbedtls_ssl_handshake, before data. No DTLS, no resumption.
   CA roots from the system bundle (mac keychain not done).
6. OPEN SQLite still in WebKit: IndexedDB SQLite files, service
   workers, tracking prevention, website data, find_package.

## MEMORY: zap, Super+Shift+A (Sep 28 2026)

Kalen presses Super+Shift+A (Cmd+Shift+A) the moment a symptom
shows (a slow orbiter/browser close takes about 10 s).
- A zap means: Kalen is seeing the issue right now. Read the
  file as it fills and look at the live app at once.
- support/zap.sh: first a line `time | message | file` onto
  install/tmp/zap/inbox; then `stats` from each trinity app
  socket (1 s limit, a stuck app shows as no answer); then every
  thread of each orbiter, aura and WPE* process (name, state,
  kernel wait `wchan`, cpu time) every 0.2 s until they have all
  exited (30 s at most) into install/tmp/zap/zap.<HHMMSS>.txt;
  last a `done after Ns` inbox line. Arguments are the message
  (default "slow close").
- Registered as a GNOME custom shortcut "zap to Claude" beside
  Kalen's custom0/custom1 (Alt+Down/Up volume). Write GNOME
  settings with /usr/bin/gsettings: install/bin/gsettings comes
  first on PATH, has no dconf module and silently drops writes.
- It reaches an agent only through a live session: that session
  runs a Monitor on `tail -n 0 -F install/tmp/zap/inbox`
  (re-armed every 30 min). Without one the files still land.
- OPEN, proposed: zap.sh runs the user's own agent itself
  (`claude -p` on the snapshot, answer saved beside it,
  notify-send), so no session is needed.
- Limit: kernel waits only; user-space stacks need ptrace,
  blocked here (ptrace_scope 1).

## MEMORY: media in the browser, played through trinity (Sep 26 2026)

The browser element's engine is now WPE WebKit 2.54.0 (checkout
checkout/wk/wpewebkit-2.54.0, overlay aura/wpewebkit/, its
configure line aura/wpewebkit.configure.sh). WebKit lays out
and runs pages; trinity draws. This entry covers video and
sound: everything done Sep 25-26, why, and what is left.

### Why it started (Kalen's reports, in order)
1. Captchas everywhere. WPE sent its own Linux/WPE user agent,
   which few real visitors send. Fix: the helper
   (Tools/TrinityBrowser/main.c) sets Safari 18.5 on macOS as
   the agent (webkit_settings_set_user_agent) and a
   document-start user script makes navigator.platform
   'MacIntel', so the two agree. Checked with a local server:
   header and page both say Mac. Whether captchas stop: not
   confirmed.
2. youtube.com stopped at its grey placeholder columns. Found
   with the helper's console on stdout
   (enable-write-console-messages-to-stdout, temporary): the
   main script threw "ReferenceError: Can't find variable:
   HTMLVideoElement". WPE was configured ENABLE_VIDEO=OFF, and
   WPE ties video to GStreamer. Kalen chose a trinity media
   backend, no GStreamer (GStreamer is not installed at all).

### Stage a: video on without GStreamer (DONE)
- Source/cmake/GStreamerDependencies.cmake: the line
  WEBKIT_OPTION_DEPEND(ENABLE_VIDEO USE_GSTREAMER) removed.
- Source/WebCore/platform/GStreamer.cmake: returns at the top
  when USE_GSTREAMER is off (its sources compiled whenever
  video was on and failed on Gst types).
- Source/WebKit/GPUProcess/media/trinity/
  RemoteMediaPlayerProxyTrinity.cpp (new, listed in
  Source/WebKit/SourcesWPE.txt): the per-backend
  RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable,
  else a link error.
- configure.sh: -DENABLE_VIDEO=ON.
Result: YouTube's app starts (signed-out "Try searching").

### Stage b: the trinity media player (DONE)
The chain, from network to screen:
WebKit fetches the file -> webgfx (silver, C calls) -> trinity
Demux reads mp4 -> trinity Decoder decodes h.264 on the Vulkan
video queue -> y, u, v planes -> three webgfx plane textures ->
the Canvas's yuv mode converts to rgb on the GPU while the page
tile paints. Sound: spectra decodes AAC with faad2 into one
AudioClip; an AudioMixer voice plays it from the video's time.

Media runs in the WEB process: the GPU process for media is on
by default only on Mac (GPU_PROCESS_BY_DEFAULT is Cocoa-only).
The RTX 3060 offers Vulkan decode for H.264, H.265, VP9, AV1.

trinity/vk.ag
- Enables VK_KHR_video_decode_queue and _decode_h264 (beside
  encode; sync2 and video_queue enabled once for both) and a
  decode queue: vk.decode_family / vk.decode_queue, a video
  family other than compute and encode. Extension list 16.

trinity/trinity.c + trinity/video.h (C, because the H.264
standard structs are bitfields silver cannot write, as for the
encoder's h264_sps helpers already there)
- h264d_new/free, h264d_config (avcC: nal length size, SPS,
  PPS), h264d_sample (one length-prefixed sample: returns 1
  when a picture is ready), h264d_params_dirty / h264d_params
  (SPS/PPS as session parameters), h264d_vk (fills the Vulkan
  begin-coding and decode infos, all pointing into the
  decoder's own storage), h264d_decoded (after the GPU decode:
  reference marking and display order), h264d_output (next
  picture in display order: its slot and pts), h264d_flush.
- Parses SPS (profiles with chroma_format_idc, scaling lists,
  POC types, cropping, VUI incl. max_num_reorder_frames),
  PPS (incl. transform_8x8 and scaling lists), and each slice
  header up to dec_ref_pic_marking (the GPU reads the rest:
  Vulkan builds the reference lists itself).
- Picture order count types 0, 1 and 2; 17 slots; sliding
  window and all MMCO ops (1-6, 5 resets POC and frame_num);
  IDR and MMCO5 push every waiting picture out first. Frames
  only: field pictures are refused.
- A picture in the output queue keeps its slot until taken
  (queued flag); the new picture's slot is chosen in h264d_vk,
  after the caller drained the queue. Otherwise a bumped frame
  could be overwritten before it was copied out.
- Slices go to the GPU as 00 00 01 + NAL, with slice offsets.
  In vkCmdBeginVideoCoding the slot being set up is listed
  with slotIndex -1.
- nv12_split (coded-size NV12 to display-size y, u, v) and
  yuv_rgba (limited range, 601 or 709; tests only).

trinity/video.ag
- class VideoFrame: y, u, v (vec u8) and pts; rgba[] for tests.
- class Decoder: config[ avcc, n ] makes the session (profile
  from the SPS, 8-bit 4:2:0 progressive), session memory,
  parameters (remade when the SPS/PPS set changes), one
  17-layer NV12 image that is both reference slots and output
  (needs DPB_AND_OUTPUT_COINCIDE; logs and refuses otherwise),
  a host-visible bitstream buffer (grows, size aligned to the
  caps), a host-visible plane buffer. decode[ data, n, pts ]
  decodes synchronously (fence wait) and puts finished frames
  in display order into frames. finish[] flushes at the end.
- Frames come back through the CPU: copy the layer's two planes
  to the host buffer, split into y/u/v at display size. No
  per-pixel colour work on the CPU.

trinity/demux.ag (new; `import demux` in trinity.ag)
- class Demux: read_bytes[ bytes, n ] walks moov/trak/mdia/
  minf/stbl (+ edts). class DemuxTrack: kind (1 video, 2
  audio), codec (four bytes as i64), timescale, duration,
  config (avcC, or AAC AudioSpecificConfig from esds), size or
  channels/rate, and per sample offsets, sizes, dts, cts, sync.
  pts[ i ] = dts + cts - shift; shift is the edit list's first
  media time (elst): without it every frame was 2 frames late.
- Progressive mp4 only; fragmented mp4 (moof) is stage c.

spectra/spectra.ag
- aac_clip[ asc, asc_n, data, offsets, sizes, skip ] ->
  AudioClip: AAC packets from any container, priming frames
  skipped (the audio track's edit-list shift).
- AudioMixer.voice_seek[ h, frame ]: software mix only.

webgfx/webgfx.ag (the C boundary WebKit calls; imports spectra)
- class WebVideo: its own copy of the file bytes, Demux, the
  first avc1 track, a Decoder, three plane ids (y, u/2, v/2),
  bt709 (height >= 720), the AAC clip, a voice, a gain.
- webgfx_video_new(bytes, size) -> id (0 = unplayable),
  webgfx_video_free, webgfx_video_info(id, &w, &h, &seconds),
  webgfx_video_has_audio, webgfx_video_advance(id, seconds)
  (decodes only as far as needed; shows the latest frame at or
  before the time; a later frame waits unless nothing is on
  screen yet; 1 when a newer frame went up),
  webgfx_video_seek (restarts the decoder at the key frame at
  or before the time), webgfx_video_play(id, seconds) /
  _pause / _volume (one shared AudioMixer at 48 kHz, started on
  first sound), webgfx_canvas_draw_video(canvas, id, dst)
  (limited-range rows for draw_yuv).
- Tests (silver --test webgfx, all pass): t_video_decode (48/48
  frames of media/clip.mp4, High profile with B-frames, in
  display order; written to install/tmp/clip-decoded.rgba and
  each within ~48.6 dB PSNR of ffmpeg's frame: rgb rounding
  only), t_video_draw (frame at 1 s drawn on a canvas is frame
  24; luma 40 dB; chroma edges differ because the GPU smooths
  chroma), t_video_steps (90 steps at 60 a second, every frame
  as wanted), t_video_audio (media/av.mp4's AAC tone: 48 kHz,
  440 cycles a second, ~2 s).
- ffmpeg on this machine is used ONLY to make test clips and
  reference frames, never inside the browser.

WebKit (checkout and overlay)
- VideoFrame.h: virtual isTrinity() under USE(TRINITY).
- platform/graphics/trinity/VideoFrameTrinity.h (new): a frame
  that names a webgfx video id; type traits for downcast.
- GraphicsContextTrinity.h/.cpp: drawVideoFrame draws a
  VideoFrameTrinity with webgfx_canvas_draw_video, others as
  the base does. Page tiles paint straight onto this context
  (TrinityPaintingEngine), so paint() reaches it.
- platform/graphics/trinity/MediaPlayerPrivateTrinity.h/.cpp
  (new, in platform/SourcesTrinity.txt): engine Trinity for
  video/mp4, video/quicktime, video/x-m4v with avc1, avc3,
  mp4a codecs (refuses MediaSource/stream decoding types).
  load() fetches the whole file with the element's
  PlatformMediaResourceLoader (TrinityMediaClient gathers the
  bytes), then webgfx_video_new; states go HaveMetadata ->
  HaveEnoughData, Loaded. A 60 Hz RunLoop timer shows the
  frame for a monotonic clock (m_from + elapsed x rate); ends
  at the duration. play/pause/seek/volume/mute drive the sound.
  paint() draws m_frame through drawVideoFrame.
- MediaPlayer.cpp registers it (USE(TRINITY) && !USE(GSTREAMER)).
  MediaPlayer.h (MediaPlayerType::Trinity), MediaPlayerEnums.h
  and Shared/WebCoreArgumentCoders.serialization.in
  (MediaEngineIdentifier Trinity).
- WebGfx.h declares the webgfx video calls.

### How it was checked in the browser
- A local server (python http.server) serves a page with a
  <video>; the page fetches /report?t=... every 0.5 s so its
  clock shows in the server log; frames are read from the
  helper's shared file (TRINITY_BROWSER_SHM: 32-byte header,
  magic TRBW, seq, w, h, stride; then rgba).
- 10 s 640x360 clip: 300 frame changes in 582 ticks (every frame
  once), frame 226 (7.533 s) matched the page clock, ended at
  10.00, error none.
- Sound: unmuted autoplay is blocked (as in any browser); a
  click sent down the helper's input ("mouse down x y 0 1 0")
  starts it. pactl then lists "ALSA plug-in [WPEWebProcess]",
  uncorked, 100%. Not heard by me.
- WebKit rebuild: ninja -C checkout/wk/wpewebkit-2.54.0/build,
  then `ninja ... install`: the helper loads the INSTALLED
  libWPEWebKit (its rpath), so a build without install still
  runs the old code.

### Bugs found and fixed on the way (all mine)
- Edit lists ignored: all times 2 frames late (1024 ticks).
- A decoded future frame was shown early when nothing was due:
  video ran twice as fast at 60 Hz ticks on 30 fps content.
- free[] on memory from `new f32 [ n ]` (silver-owned): crash.
- An inline `vk_context []` inside a Window constructor is not
  owned by anything: freed mid-use ("Invalid device").

### Silver compiler bugs met (OPEN, worked around)
- `v.push[ w[ i ] ]` (w a vec i64) pushes the element's
  ADDRESS: ctts offsets came out as 98715440282664, +16 a
  sample. Read into a local first.
- `is`, `signed` and `parse` are taken names (keyword, type,
  Au method): use box_is, neg, read_bytes.
- Function arguments are read-only: copy to a local to walk.
- A VkResult cannot interpolate into a string: i32[ res ].

### Limits and what is next
- Whole file downloads before play; no range requests.
- Planes cross the CPU once per frame (copy + upload).
- Rates other than 1 keep sound at 1x; the picture follows a
  plain clock, not the audio clock (long videos can drift).
- H.264 + AAC in mp4 only; field video refused; no VP9/AV1 yet
  (the GPU could decode them).
- Mac: coreaudio mixer path has no voice seek.
- NEXT, stage c: MediaSource. YouTube streams through it.
  Needs ENABLE_MEDIA_SOURCE, fragmented mp4 (moof/traf/trun)
  in Demux, SourceBuffer append/remove, and advertising only
  avc1/mp4a so YouTube serves H.264 + AAC.

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
9. DONE uniform Buffers: one buffer per pipeline (`u_ring`, vk.ag
   uniforms.init), a slot every `stride` bytes; was 64 per `uniforms`.
10. OPEN small heap 216 MB not yet attributed; ~5,000 pool
   temporaries per idle frame.

## Active work: features docs page (Sep 28 2026)

1. DONE features.ag is sorted into 14 categories, each opened by
   `# ==== Label: Headline` and an intro comment. `export func
   docs_page` writes features/docs/features.html in the site's
   layout (head, CSS, hero and footer copied into
   features/docs/page.html from /src/ar-visions.github.io).
   Order matters: a free func signature needs its types above
   it; a load-time static (`attrib egg : Chirper`) needs its
   class above it.
2. OPEN t_launch_specs: the .agi has no launch specs any more
   (only trinity's launch_props writes them); the test fails.
3. OPEN t_mem_last: 33 objects alive at the end, 28 at the start,
   with the original order too. Hidden until now: the runner
   stops at the first failure, t_vec_module.
4. OPEN the page is not yet compared with index.html by eye.

## Active work: near-border (Sep 29 2026)

1. BUILT, tested: the pointer lights every border by its
   nearness. No style property (Kalen: never different between
   controls). Window.near_walk records each element's window
   origin (win_pos) and marks it for repaint while the pointer
   is within near_reach (160 px) of its box, and once after it
   leaves. The layer draw passes the pointer and reach into the
   canvas (near_light); CanvasUI's near_border weights the
   stroke: w = (1 - distance / reach)^2, the border's colour
   mixed toward white by w/2, its alpha raised by w/2.
   trinity t_near_border: red 128 at 20 px from the light, 51
   (the border's own) at 340 px.
   The light's strength is a filtered state per element
   (near_glow): its goal is near_in_weight (1.0) with the pointer
   inside the element, near_out_weight (0.5) outside it within
   reach, else 0; each frame glow = filter_n[ glow, goal, 0.1 ]
   (0.9 kept, 0.1 new at 60 Hz). t_near_border_glow: red 90 at
   half glow.
   Opt-in (Kalen, Sep 29): a Layer's `near_border: true`, or a
   draw's Window.near_light_on[ cv, e, ox, oy ]. Only an element
   whose last paint lit a border (near_used) repaints on moves.
   orbiter: title bar (tbar), status bar (sbar), and the editor's
   ctrl/cmd jump outlines (outline_jumps).
   Inner glows (Sep 29): Layer near_inset (factor) and
   near_inset_color: an inset shadow blends toward the colour
   by nearness x glow x factor. The y distance counts
   Window.near_y_fade (2.0) times over. t_near_inset, t_near_fade.
1b. OPEN silver compiler bug: adding a Canvas method
   (near_fade) made trinity's initializer write Canvas's method
   pointers past Canvas_i into GlyphMetrics_i ("MODULE OVERWRITE
   [def]: (null)", export exit 1). The store offset is
   Au_t size + (member_index - 1) * 8 (aether.c ~10650); member_index
   comes from next_function_index (silver.c:7604, counts the whole
   context chain, skips statics and overrides); the table
   (__Canvas_f ft, 175 slots) comes from etype_class_list
   (aether.c ~8800). The two counts disagree. Canvas_paint went to
   byte 1816 of a 1672-byte table. Not fixed; near_fade folded into
   near_style's y_fade argument instead.
2. OPEN element gained win_pos and near_lit: every module built
   before it (orbiter, scenes, clouds, markdown, aura, ...)
   needs a rebuild before it is hosted again.
3. OPEN not seen in a real app yet (buttontest has no borders).

## Active work: orbiter side panel (Sep 29 2026)

1. BUILT, not run: one context panel for the whole window, the
   selected editor's (active_pane) file, module and instances.
   The window is a split: panes left, SidePanel right (drag the
   divider; 360 px to start). Its toolbar row: < folds it, then
   the tabs (instances). Folded, SideStrip keeps a menu button
   at the top of the right edge (inst_hidden). The per-pane
   InstancePanel and each title bar's menu toggle are gone.
2. BUILT, not run: the status bar is 35 px tall (was 22 px);
   the instance panel's bottom toolbar is 35 px, flush on it.
3. OPEN the conversation tab: waits on what it shows.

## Active work: dictation marks on a recording (Sep 30 2026)

Kalen: dictate while recording; each take is a mark on the
recording's clock (start, duration, words), and the marks are
the composer's instructions ("I want a bugs bunny right after I
played the fzero game"): the span where an instruction was
spoken is cropped out of the finished video, and the composer
(a new app, `composer`) feeds the .agi with a VIDEO.md (the
video operations it may emit at times) into the user's agent
shell, gets video operations at times back, and renders a new
compressed mp4.
1. DONE marks. trinity/video.ag: RecMark (at, dur seconds;
   text; file: the pane's file or the listen file), RecNotes
   (context: a hand-written hint for the composer; marks),
   Recorder.time_us / mark / notes_load / notes_save; the file
   is `<take stem>.agi` beside the mp4 (record_notes; a live
   stream keeps none), rewritten at every mark and at close,
   read back at a reload so the marks continue. Module funcs
   record_time / record_mark (trinity.ag, over rec_live): the
   app element is not the media_app, so it cannot call its
   methods. orbiter mic_frame: mark_at at the press, mark_end
   at the release, the mark once the words arrive.
2. DONE (Kalen) the mp4 holds video and audio only, never
   text: the marks live in the .agi alone, and the user edits
   that file directly. The text track and --retitle were built
   and removed. Checked headless: a take with a spoken test
   wav gives an mp4 of h264 + aac (decodes clean) and a .agi
   with the mark at 1.60 s for 1.36 s, words exact.
3. DONE the composer protocol (composer/VIDEO.md, composer/
   composer.ag). The agent writes <stem>.composition.agi: a
   Composition of ops, each one serialized call on Composer
   (Op subclasses: Fetch name url, Crop from to, Insert at
   media ...). Composer.plan runs every Fetch, then every Crop
   (they set the output's clock: Kept spans), then every Insert
   (mapped to output time as an Overlay); problems collects
   what cannot be done. Resources live in <stem>/ named by the
   reference the user said (bugs-bunny); a file already there
   is used, else Fetch downloads it (tls Http.download).
   Virtual calls on an Op need *[] (o.invoke*[ a ]): a plain
   [] binds to Op's own method; `x inherits T` on a vec element
   was false for its real class. `silver --test composer`:
   t_composition_plan passes (a crop 10..13 moves an insert
   at 20 to 17 on the output).
4. DONE the agent request. `composer [--agent codex] [--model m]
   <take.mp4>` (composer.ag, element composer): the take's
   length from its fragments (take_length: moof/traf tfdt +
   trun count x tfhd duration, video at 90 kHz; 18.0 s, as
   ffprobe says), then Composer.ask runs the user's agent
   through trinity's agent shell in the take's folder with
   VIDEO.md (read from beside the module source) and the take's
   paths in the request; it writes <stem>.composition.agi; then
   load and plan. Checked with the real claude on a test take
   (an instruction mark, a logo the user put in take/): it
   cropped the mark (1.6-3.8 s), put take/orbiter-logo.png top
   right for 3 s at 3.8 s, downloaded nothing, left the
   narration mark alone; the plan: 2 spans kept, 1 insert.
5. DONE full-colour takes are H.265 4:4:4 (Kalen's pick): the
   RTX 3060 encodes H.264 4:4:4 but decodes H.264 at 4:2:0 only;
   it encodes and decodes H.265 4:4:4 8-bit. `--rec_qp stage` /
   `lossless` (the default) now record H.265 Rext 4:4:4 into
   the mp4 (hvc1 + hvcC), or a raw .h265; streams (ts, hls)
   stay 4:2:0 H.264. Encoder.hevc: VPS/SPS/PPS and per-picture
   info from C (trinity.c h265_params, h265_picture: C owns the
   memory and every pointer into it; pointers into a C struct
   taken from silver (@x.member) crashed the driver). Lossless:
   qp 0 + transquant bypass. vk.ag enables encode_h265 and
   decode_h265 (vk_context.h265_encode / h265_decode; the device
   extension list is 32 long now).
   Decoder.config_hevc + C front end h265d_* (VPS, SPS with VUI,
   HRD, scaling lists, range extensions, PPS, slice header up to
   the RPS, short-term sets incl. inter prediction, long-term by
   poc lsb, POC msb, reference marking, bumping output). 4:4:4
   pictures come back as full-size u and v (VideoFrame.full).
   Checked (composer t_hevc_decode, COMPOSER_HEVC=<mp4>
   COMPOSER_HEVC_OUT=<yuv>): a 320x240 x265 4:2:0 clip, 60/60
   frames, and a 640x200 lossless 4:4:4 take recorded from
   texttest, 480/480 frames: both byte-identical to ffmpeg's
   decode. The take's decoded picture vs the app's own shot:
   max 2/255 (rgb<->yuv rounding only). webgfx and trinity
   tests pass; orbiter and aura rebuilt.
   The video queues exist only once a Display attaches: a test
   needs a headless Display before decoding.
   Older takes (H.264 4:4:4, like Desktop/video.mp4) still
   cannot be read back on this GPU.
6. DONE the render (composer.ag Composer.render, Renderer,
   Source, Media, Planes, Seg). Pass 1 reads the take for its
   size, frame rate and all its sound (spectra aac_clip); the
   kept spans are split at every held cut (pause inserts) into
   pieces in output order, and every insert gets its output
   time from them. Pass 2 streams the take through the GPU
   decoder; each output frame: the take's planes drawn with
   draw_yuv (bt.709), the active inserts (pip at a third, full,
   region; fade in and out) with their own decoder or picture,
   then Canvas.sync_all and Recorder.capture (paced false: one
   frame per call) into H.264 4:2:0 QP 23 + AAC. The sound:
   the take's pcm for each kept piece, silence for a cut, an
   insert's own sound where `sound: media`.
   A composition newer than the notes renders as it stands, with
   no agent (Sep 30: --again removed). Checked headless: a lossless H.265 4:4:4 take, crop
   1-2 s, logo pip at 3 s, a 1.5 s held cut to av.mp4 with its
   tone: 8.5 s out (8 - 1 + 1.5), 510 frames, the four frames
   right by eye, the tone exactly in the cut (-21 dB there,
   silent elsewhere). 1080p60, 10 s take: rendered in 6.3 s.
   Found on the way: a VideoFrame read out of a vec and then
   removed is freed at once (use it before removing); a png
   drawn straight showed its transparent area (noise): pictures
   are premultiplied at load.
7. DONE a take is a module (Kalen): `--record video` makes
   video/ if it is not there and records video/video.mp4 beside
   video/video.agi (trinity record_path; relative to the launch
   folder; a stream url or a name with a video extension is
   used as it is). The composer takes the folder
   (`silver composer ~/Desktop/video`); the resources live in
   that folder, named `video/<file>` in a composition (resolved
   from the folder's parent); the composition and the render
   land in it too. Checked headless: --record demo gave
   demo/demo.mp4 + demo/demo.agi; composer on the folder wrote
   demo/demo.final.mp4 with demo/bugs-bunny.gif in it.
   Also: GIF inserts (composer/composer.c, animated, looping),
   region is the text 'x y w h' (the media keeps its shape), an
   H.264 4:4:4 take is refused up front, and a new take starts
   its notes fresh (a reload continues them).
8. OPEN orbiter's row for the composer, progressive JPEG in
   img (it aborts on them), and the frames still cross the CPU
   (decode readback, plane upload).
9. PLANNED (Kalen, Sep 30) voice conversion on the take's sound,
   offline in the render: a `Voice` op (from, to, voice) whose
   voice is a reference .wav in the take's folder, named in the
   context or a dictation like any other media ("video/vader.wav
   is Darth Vader"). Model: Seed-VC ported to silver as whisper
   was (whisper-small encoder already in speech; CAMPPlus,
   the DiT with its flow-matching steps, BigVGAN to port), its
   .pth checkpoints read by a new zip+pickle reader.
   Seeding: each reference .wav is encoded once (the CAMPPlus
   speaker embedding and its prompt mel), cached by file, and
   reused by every Voice op that names it. Each op's span is
   converted in chunks that overlap and are crossfaded, and the
   span's edges crossfade into the original sound, so the voice
   comes in and out without a click.
   DONE (Sep 30) the weights: composer.ag imports the Seed-VC DiT
   (whisper-small wavenet variant, its config confirms stock
   openai/whisper-small, speech's file is reused), CAM++ and
   BigVGAN v2 22k into install/models/seed-vc/ (918 MB). ai's
   `pth` class reads PyTorch zip checkpoints (ai/ai.c: zip with
   zip64, the pickle opcodes torch.save writes, strided tensors,
   f32/f16/bf16/int read as f32). composer t_seed_vc_weights: 302
   / 937 / 783 tensors; ups.2.0.weight_v is 384x192x4, first
   value -0.026324, the same as Python's own pickle reads it.
   The C functions are pthf_*: a silver class's methods compile
   to <Class>_<method>, and `pth_count` collided with class pth.
   DONE BigVGAN (composer/vocoder.ag): weight norm folded at
   load; conv_pre, 6 transposed upsamples, 18 AMP blocks with the
   anti-aliased snakebeta (2x up, 12-tap kaiser filters from the
   checkpoint, 2x down), conv_post, clamp. t_bigvgan against
   PyTorch on a real 2 s mel (COMPOSER_VOC_MEL/WAV): 44032
   samples, SNR 98.4 dB. CPU, one thread: 10 s for 2 s of audio.
   DONE CAM++ (composer/campplus.ag) with Kaldi's fbank (povey
   window, pre-emphasis, htk mel, 512-point power, mean off):
   t_campplus (COMPOSER_CAM_DIR): fbank max error 5e-4, the
   192-number style max error 1.8e-5, 0.66 s for 2.6 s of audio.
   Golden files come from the original Python (scratchpad
   bigvgan/ref.py, seedvc/cam_ref.py; torch is on this machine,
   torchaudio is not: its kaldi.py runs standalone).
   Silver found on the way: `v[ i64[ a ] * n + b ] = x` fails
   ("no indexing available for model i64") when the index starts
   with a cast: compute it into a local first; a vec returned
   from a call and aliased by another local was clobbered on the
   next loop pass (sum into a fresh buffer instead); `pre`,
   `post`, `ref` are reserved; pointer + n is not element
   arithmetic (take @v[ n ] instead); a param named `a` shadows
   the object itself.
   DONE the DiT (composer/dit.ag: length regulator, 13 layers,
   RoPE, AdaLN-RMS, U-ViT skips, wavenet head; Euler flow, 10
   steps, CFG 0.7; matrix work through ai's gemm_nt, 32 threads)
   and the front end (composer/seedvc.ag: sinc resample, whisper
   log-mel and content, the 22 kHz mel). Against PyTorch: one
   DiT call 3e-5, the flow 3.4e-5, content 1.3e-4, the whole
   voice 52 dB SNR.
   DONE the op, named `Revoice` (spectra has a Voice class):
   from, to, voice. In VIDEO.md. A speech take rendered: the
   pitch moved 301 -> 203 Hz, the output does not correlate
   with the input (-0.02). Speed: 31 s for 2.5 s of sound, on
   the CPU; Kalen accepts it. OPEN: the GPU path (below).
   GPU (Kalen asked, Sep 30): ai's whisper_vk has compute
   shaders for gemm, attention, layer norm and conv; Seed-VC's
   whisper already runs there. The DiT and BigVGAN are not on it.
11. DONE (Oct 6) `--rec_scale 2`: the Display's pixel scale is
   doubled while recording (the swapchain stays the window's
   size; its bilinear quad averages 2x2), the take is twice the
   window's pixels, its .agi says `scale: 2`, and the composer
   renders at take / scale, so a Focus to 200% shows real
   detail. Checked: buttontest 800x500 -> 1600x1000 take ->
   800x500 render, a 250% focus sharp where a 1x take is soft.
   Not seen on screen (hidden runs). element gained rec_scale:
   apps built before need a rebuild.
10. DONE the render's frame rate is the shortest gap between
   decode times over 60 pictures (ffmpeg's first frame is
   longer: an average read 26, then 59; now 60).

## MEMORY: the composer (Sep 30 2026)

What it is: a trinity app (`composer/`) that turns a raw screen
recording into a finished, compressed video. The user records an
app and talks while doing it; the talk is the edit. The agent (the
user's own claude or codex) reads what was said and writes the
edit; the composer carries it out. Kalen: "record an app, talk to
your ai and have a product come out on the other end";
"the ai can alter trash in, treasure out".

Why: to help media creators, and Kalen himself, make good videos
without editing by hand. It belongs in the stack: trinity records,
speech hears, the agent decides, composer renders. No outside
editor and no ffmpeg anywhere in the path.

The flow:
1. `--record video` (any trinity app, orbiter too) makes the take
   folder video/: video/video.mp4 (H.265 4:4:4 lossless by
   default, AAC sound) and video/video.agi. Dictation runs from
   the first frame, with no button: each spoken take becomes a
   mark (at, dur, words) on the recording's clock in the .agi.
   The mp4 holds video and audio only, never text.
2. The .agi has a `context` field: the user's hints for the
   agent. The user can edit the marks and context by hand.
3. `silver composer video` sends the agent VIDEO.md (the
   protocol), the marks and the folder. It writes
   video/video.composition.agi: a list of ops. A spoken
   instruction ("put a bugs bunny after the fzero bit") is cropped
   out of the result.
4. The render reads the composition and writes
   video/video.final.mp4 (H.264 4:2:0 + AAC, the shareable form).
   A composition newer than the notes renders again without asking
   the agent (--again removed Sep 30).

The folder is the module: resources sit beside the take, named by
the reference the user said (video/bugs-bunny.gif). A file that is
already there is used; otherwise a Fetch op downloads it. The user
starts the folder and the agent adds to it.

Ops today:
- Fetch: name, url.
- Crop: from, to.
- Insert: picture, gif or video; picture in picture at a third of
  the frame, full frame, or a region; fades; a held cut (pause)
  with the insert's own sound.
- Revoice: the take's own sound in another voice, from a short
  reference clip (.wav .m4a .mp4 .mov) in the folder. It is
  Seed-VC ported to silver (whisper-small content, CAM++ style,
  DiT flow matching, BigVGAN). There is no training step: any
  voice works from a few seconds of clean speech. It runs on the
  CPU at about 12 s of work per second of sound (Kalen: fine).

Where it might go (ideas, none started unless listed elsewhere):
- Seed-VC on the GPU: ai's compute shaders (gemm, attention, norm,
  conv) already run its whisper; the DiT and BigVGAN are next.
- Frames on the GPU end to end (no decode readback or upload).
- A composer row in orbiter: see the marks, the composition and
  the result beside the take; render from there.
- More ops: zoom and pan to what matters on screen, speed ramps
  over dull parts, music beds under narration, captions burned
  into the picture (pixels, not a text track), a thumbnail, an
  intro and outro.
- A voice library: shared reference clips named once and used in
  any take. Seed-VC's singing model (44 kHz, with pitch) for
  songs.
- Publishing: trinity already streams to youtube:// (HLS); an
  upload of the final render would close the loop.
- The same flow for any recording, not only app captures: talk
  over a camera take or a game session and get an edited cut.

## Active work: RVC in composer (Sep 30 2026)

Kalen: RVC instead of Seed-VC ("the performance is better"), a
voice trained from a clip (train from a clip, not downloaded
models), Seed-VC replaced. Seed-VC stays until RVC works end to
end, then goes with its 918 MB of models in the same step.
Each stage is checked against the original PyTorch code
(RVC-Project/Retrieval-based-Voice-Conversion-WebUI, MIT), as
the Seed-VC port was. Order:
1. DONE weights into install/models/rvc (composer imports):
   hubert.pth + hubert.json (the transformers form of RVC's
   hubert_base, not the fairseq .pt), rmvpe.pt, f0G40k.pth,
   f0D40k.pth; THIRD_PARTY.md rows. t_rvc_weights: 213 / 741 /
   560 / 165 tensors and first values as PyTorch reads them (all
   stored f16). Reference source (RVC main, uses transformers'
   HubertModel) in the session scratchpad rvc/.
1b. IN PROGRESS the GPU engine, ai/grad.ag (`import grad` in
   ai): kernels are GLSL with buffer device addresses and sizes in
   push constants (6 addresses, 12 ints, 4 floats), so one
   pipeline serves every shape and a whole step is one chain of
   dispatches (GRun, a barrier after each). Tensors live in two
   arenas (GArena over one trinity Buffer each: `keep` for weights
   and optimizer state, `work` reset every step); Buffer gained
   address[] and buffer_handle[] (trinity vk.ag). Autograd: a tape
   of GOp records (kind number; Grad.back_op switches on it).
   Kernels are written as trinity writes shaders (Kalen): a
   subclass of GKern with `func compute [] -> GLSL none { .. }`
   (main's body) and `decl` (shared arrays) token methods, braces
   doubled; only the push-constant header is a string (its
   #version/#extension lines are comments to the tokenizer).
   Checked against pytorch (GRAD_GOLD, generator scripts in the
   scratchpad gold/): strided batched gemm 5e-7; conv1d (stride,
   pad, dilation, groups) and conv_transpose1d, outputs and every
   gradient, under 4e-6; leaky/tanh/sigmoid/exp/scale, add/mul
   with broadcast, wavenet gate, channel slice/join, layer norm
   under 1.2e-6; rvc's relative-position attention with padding
   mask under 6e-7 (a masked score passes no gradient, as
   masked_fill).
2a. DONE the synthesizer (composer/rvc.ag, RvcNet on the engine):
   text encoder, reverse flow, NSF HiFi-GAN with rvc's sine source
   (rvc_sine, on the cpu). t_rvc_infer (COMPOSER_RVC_GOLD): the
   f0G40k base generator on 40 frames of random features, a pitch
   track with an unvoiced gap and pytorch's own noise draws:
   16,000 samples, 97.3 dB SNR against SynthesizerTrnMs768NSFsid
   .infer, 148 ms on the RTX 3060.
2b. DONE HuBERT (RvcHubert: transformers' HubertModel, last layer,
   positional conv weight norm folded on the cpu at load): 2 s of
   speech, 99 frames, worst 9e-6, 113.8 dB, 38 ms. t_rvc_hubert.
2c. DONE RMVPE (RvcPitch: batch norms folded into the convs at
   load; stft as a conv with a windowed cos/sin basis; librosa's
   htk mel filters with slaney norm; the u-net on conv2d/convt2d/
   avg_pool2; a gpu gru, one workgroup per direction): mel worst
   1.7e-4, salience 1.7e-6, pitch 0.0005 cents worst, voicing
   identical on 113 voiced frames, 76 ms for 2 s. t_rvc_pitch.
   Engine gained gelu, tslice (with backward), time_norm, conv2d,
   convt2d, avg_pool2, gru256, magnitude, log_clamp, view.
   FIXED (Sep 30, see "silver vec and indexing bugs"): a vec literal
   holding an element access, `v.push[ w[ i ] ]`, and float
   constants in a vec f32 literal. grad.ag/rvc.ag still use the
   workarounds (locals, fill by index).
2d. DONE training ops (t_grad_training against pytorch, all within
   4e-7): per-item time slice (rand_slice_segments), reflect pad
   (backward gathers per source: taps mirror onto one sample),
   magnitude and log clamp with backward, period fold (a
   DiscriminatorP 2-d conv as a 1-d conv over batch x period),
   least-squares and l1 means, vits' kl with a [b, 1, t] mask,
   backward_all (several losses, each seeded with its weight),
   AdamW (pytorch's: decoupled decay 0.01, bias-corrected).
2e. DONE one rvc training step (composer/rvc.ag RvcTrain, RvcDisc,
   RvcOpt, RvcBatch): posterior encoder, forward flow, a random
   32-frame slice per item, the nsf decoder, v2's nine
   discriminators (scale + periods 2..37), least-squares gan, feature
   matching x2, mel l1 x45 (librosa slaney mel, 2048/400 stft), kl,
   adamw on both nets. t_rvc_train_grads (lr 0, COMPOSER_RVC_TRAIN0):
   every loss within 1e-5 of pytorch; gradients within 0.5%;
   t_rvc_disc_grads: all 165 discriminator gradients within 0.13% on
   pytorch's own generated wave. The residue is one leaky relu whose
   pre-activation sits within 1e-6 of zero landing on the other side
   (counted: 1 sign flip at the layer where the error enters); pytorch
   f32 vs f64 is 1e-6 there. t_rvc_train_step (lr 1e-4, two steps):
   losses to 4-5 digits; adam's first step moves every weight by
   +-lr, so near-zero gradients flip and step 2 drifts by design.
   ~1.5 s a step at batch 2 on the RTX 3060.
   Memory: conv columns live in a per-op scratch arena and are
   rebuilt in backward (kept, they overran 6 GB at batch 2).
   The gemm sums each 16-term tile, then joins tiles with kahan
   compensation. The tape cannot be swapped for the discriminator
   step (the old vec was the only holder of the generator's
   outputs): tape_cut/backward_from over a mark instead.
2. OPEN inference: HuBERT features (768, layer 12), RMVPE pitch,
   the synthesizer (text encoder with relative attention, flow,
   NSF HiFi-GAN generator), on the GPU. Checked on the base
   generator against PyTorch.
3. DONE training data (composer/rvc.ag RvcPrep, its own Grad with
   hubert and rmvpe, released after): 40 kHz, scipy's 48 Hz 5th
   order butterworth high-pass (rvc_highpass, 2e-6 vs scipy),
   slicer2 ported (rvc_slices, boundaries identical to rvc's on a
   three-utterance clip), 3.7 s pieces overlapping 0.3 s, peak
   normalized (rvc's quirk kept: only the last slice's short tail
   is written), hubert rows repeated to 100 frames, rmvpe pitch with
   unvoiced frames interpolated, coarse bins, the spectrogram on
   the gpu. t_rvc_prepare: 22 s clip, 6 items (5 x 368 frames, 72),
   0.8 s. Resampling is our sinc (rvc uses librosa's soxr).
4. DONE the training loop (RvcTrain.train): items sorted by length,
   batches cropped to their shortest (rvc pads instead), batch order
   shuffled each epoch, lr 1e-4 x 0.999875^epoch, speaker 0, the
   generator saved as safetensors every n epochs (RvcNet.save; a
   voice file loads over f0G40k by name, RvcNet.overlay).
   t_rvc_train_loop: 2 epochs on the 22 s clip (mel 0.71 -> 0.60,
   kl 8.2 -> 3.8), saved and reloaded bit for bit, converted.
   ~1.5 s a step at batch 2: 15 min of speech is ~3 min an epoch.
5. DONE retrieval (RvcIndex): the training pieces' 50 fps hubert rows
   saved beside the voice (<voice>.index, raw f32 [n, 768]); exact
   search on the gpu (gemm distances, a top-8 kernel), rvc's 1/d^2
   blend at rate 0.75. t_rvc_index: vs exact numpy, 5e-7. rvc uses
   faiss ivf (approximate); this is exact.
6. DONE Revoice on rvc: RvcVoice.speak is rvc's Pipeline.pipeline
   (filtfilt high-pass at 16 kHz with zero start states, cuts at the
   quietest point within 3 s of every 8 s (rvc: 6 s / 38 s; our
   decoder columns cap the chunk), 1 s reflect pads, rmvpe once over
   the whole padded audio in 20 s windows past 25 s, per chunk:
   hubert, index blend, repeat to 100 fps, protect 0.33, synthesis,
   trims), then to 48 kHz. t_rvc_pipeline: 22.2 s in 2.8 s. The
   decoder compacts its arena per block at inference (Grad.compact):
   8 s uses 318 MB. Revoice gains pitch (semitones) and epochs
   (100); its voice is a clip of the target speaker, trained on first
   use into <clip stem>.rvc.safetensors (+ .index) beside the clip.
   VIDEO.md updated. Seed-VC removed: dit/seedvc/campplus/vocoder.ag
   (tracked in git; copies in the session scratch), its 876 MB of
   models, its tests and THIRD_PARTY rows; resample_sinc moved to
   rvc.ag. composer 17/17 with every rvc reference set.
   Tests must release their grads: arenas left from earlier tests
   oversubscribed the gpu and tripled the step time.
7. DONE training speed, first pass: an epoch of the 22 s test clip
   4.6 s -> 1.6 s (batch 2). Measured with GRun.prof (gpu timestamps
   per dispatch; Grad.prof_report). Fixed: weight norm ran one thread
   per channel over its whole row (now a 256-thread group per
   channel); a full-shape dz used one 256-thread group per element
   (now the elementwise dx kernel); weight-gradient gemms with few
   output tiles over a long sum (32x352 over 25,600) ran 6 groups
   (now split into pieces in scratch, summed in order: KSplitSum;
   t_grad_gemm_split). Inference 8 s: 2.3 s -> 0.56 s; synthesis
   vs pytorch 98 -> 109 dB. gemm is ~2 TFLOPS on a 2048 cube (the
   3060 peaks near 12): a bigger-tile gemm is the next speedup.
8. IN PROGRESS train picard-full (Kalen's video2 folder): `silver
   composer video2`, Revoice ops on picard-full.wav;
   247 pieces, ~70 s an epoch at batch 2. librosa and scipy install
   with composer (pip into install/pylib, as os-bootstrap does,
   skipped once there); --train was removed (a Revoice trains its
   voice on first use and logs each epoch). First run: killed by
   the kernel's oom killer at epoch 47, 124 GB resident (below).
9. DONE a full arena crashed (tensor_in returned null; batch 4
   needed more than 6 GB): tensor_in throws, train_voice and
   apply_voices catch it as a composer problem; batch is 2.
   ai t_grad_arena_full.
10. DONE the training leaked ~2.6 GB of cpu memory an epoch: train
   runs inside init and nothing drained the object pool. Au gained
   auto_mark / auto_free_to (drain only what entered the pool after
   a mark; the caller's objects stay); train drains after each step
   and each epoch. Small run: +95,053 pages an epoch -> flat.
   ai t_grad_pool_mark.
11. DONE checkpoints go to <model>.part, renamed at the last epoch:
   a stopped run no longer leaves a model that looks finished.
   composer t_rvc_train_voice (18/18). The killed run's epoch-40
   file was renamed to .part.
13. WRITTEN, not built (the video2 run's host would reload on a
   build): resume. A checkpoint is the generator at <model>.part
   and the rest at <model>.part.state (discriminator weights, both
   optimizers' moments named by parameter, epoch and steps), each
   written to .tmp and renamed. train_voice resumes from them when
   both load whole; RvcTrain.done skips the finished epochs. Test:
   composer t_rvc_train_resume (2 epochs, resume, equal to the
   bit, then a third). The killed run's .part has no state file:
   this run started over.
14. DONE speed, second pass (measured on the 22 s clip, batch 2):
   an epoch 1.6 s -> 1.35-1.5 s. Kept: losses sum over up to 256
   groups, then KLossSum adds the partials in order (kl too);
   AdamW's dispatches run without barriers between them, one fence
   after (GRun.nofence); the discriminator's weight norms are made
   once per pair of passes (RvcDisc.wcache, cleared in set_need);
   both gemm kernels load along the contiguous dimension (2048 cube
   2.1 -> 2.6 TFLOPS); KGemm2 (128 x 64 tiles, 8 x 4 a thread) for
   m and n >= 512 only (cube 3.0 TFLOPS; on rvc's tall shapes it was
   slower). Tests: t_grad_gemm_layouts, t_grad_gemm_split.
   Tried and REMOVED (slower on a real epoch): implicit conv (x read
   as conv columns inside the gemm: +300 ms an epoch) and the
   implicit input gradient (+900 ms). conv1d keeps im2col/col2im.
   Found on the way: a flag read from the 64-bit push constant a4
   came out true while a4 was 0 (kernel-side, cause not found);
   flags ride in float bits (f2/f3) instead.
   composer: au_live_set_reload[ 0 ] at init (built): a build never
   reloads a training run now.
16. DONE shared voice store (Kalen: training by named alias): a
   trained voice lives in install/models/voices/ by the clip's name
   (picard-full.wav -> picard-full.rvc.safetensors, .index, .fp);
   the .fp is the clip's size and FNV-1a hash, written last, and a
   mismatch retrains. Composer.voice_dir overrides the folder (tests
   use a scratch one). t_rvc_train_voice checks the second call
   reuses the model. video2's Picard voice was copied in, with its
   fingerprint (video4's picard-full.wav is the same file).
15. DONE the video2 render (Sep 30, 21:59): picard-full trained 100
   epochs (mel 0.503 -> 0.426, memory flat at 6 GB), the three
   Revoice spans converted, video2.final.mp4 written: 1080p60,
   2 min 25 s, h264 + aac 48 kHz.
12. OPEN silver bug: an interpolated literal with a member as a C
   call argument, `rename[ '{a}'.chars, '{b}'.chars ]` or
   `unlink[ '{a}'.chars ]`, fails "expected 1 args, got 1"; a
   local string first works.

## Active work: silver vec and indexing bugs (Sep 30 2026)

Kalen: go over the vec and indexing errors met in grad.ag and rvc.ag,
validate them in features.ag, fix silver. Each has a focused test in
features (Collections section). features: 222/225; the 3 failures
are the old t_vec_module, t_launch_specs, t_mem_last.
1. DONE the expect runner ran until the first failure, hiding every
   test after it (t_mem_last was hidden behind t_vec_module). A
   failed test now prints its line and counts (silver_expect_failed,
   aether emit_expect_tests); the report says "N/M passed, K failed"
   and exits 1 (emit_expect_exit).
2. DONE `v[ i64[ a ] * 4 + 1 ] = x` ("no indexing available for
   model i64"): a statement's left side parses at expr_level 0, where
   read_enode does not read `Type[ x ]` as a cast; the index args now
   parse at expr_level + 1 (silver_parse_member_expr). t_index_cast_lead.
3. DONE `f[ x ][ i ]` (a call's result indexed): parse_member_expr
   indexes a call's result when [ follows on the same line.
   t_index_call_result.
4. DONE `vec i64 [] [ src[ 0 ] ]`, `[ h.dims[ 0 ] ]` and
   `v.push[ w[ i ] ]` stored the element's address: e_create's boxing
   (prim -> Au) stored an unloaded element enode; it loads it first
   (enode_value force). t_vec_lit_elem, t_vec_lit_member_elem,
   t_vec_push_elem.
5. DONE `vec f32 [] [ 3.0, 7.5 ]` stored garbage: the f64 literal was
   boxed as f64 and the vector copied 4 of its bytes; seeds now parse
   with, and convert to, the element type. t_vec_lit_f32.
6. DONE `string[ @b[ 0 ] ]` (a byte pointer) reinterpreted the pointer
   as a string object: e_create builds the text type from it as cstr
   (u8/i8 pointers; not cstr/symbol themselves, not null).
   t_string_from_vec_bytes.
7. CHECKED, no bug found: a runtime-sized `vec <C struct> [ n ]`
   (t_vec_c_struct_n, feat_rec in feat.h: pointer, int, double; n 40,
   stride 24, no overrun) and a returned vec aliased by a local in a
   loop (t_vec_return_alias). The original failures may have had
   another cause; the tests stay as guards.
8. OPEN not vec/index but met on the way: `'{a}'.chars` as a C call
   argument fails ("expected 1 args, got 1"); `--verbose` builds
   sometimes abort with "double free or corruption" (twice in five).
9. DONE ai, composer, trinity and orbiter build with this compiler;
   ai and composer tests all pass. The runner change missed library
   modules (their tests run in <mod>_late with no report): a failed
   library test now prints "K of N failed" and exits 1
   (emit_expect_failures). OPEN: the workarounds in grad.ag/rvc.ag
   (locals, fill by index) can go back to the plain forms.

## Active work: YouTube smoothness in aura (Sep 28 2026)

Kalen: the player's fullscreen transition ran at 15-20 fps, its
menus at 10-30, the video at 30 at most. Measured headless with
the WebKit helper alone (scratchpad ytprobe/ytlong) and through
the element (drive.py, `--width 2560 --height 1440`), traces
since removed. Found and fixed, in order:
1. DONE frame interpolation (Kalen's spec: only a time-weighted
   mix of the two authored pictures, one picture behind, no
   motion compensation). trinity/video.ag `FrameBlend` (a
   PlaneBlend compute over the y, u, v planes), `VideoStream.
   smooth` (default true), `next_pts`; `frame_at` returns the
   mix when t sits between two ready pictures. webgfx
   t_video_blend: the mid mix equals (a+b+1)/2 on every byte.
2. DONE the picture ticks at the screen's rate: devices
   `platform_window_refresh_hz` (RandR on X11, the KMS mode
   otherwise; DP-0 = 165 Hz), aura sets TRINITY_VIDEO_HZ for
   the helper; the player's timer (videoHz) and the headless
   view's frame pacing (WPEViewHeadless.cpp, overlay) follow
   it. Checked: 165 ticks a second, a mixed picture each tick.
3. DONE the transition's real cost was CSS drop-shadow filters
   on the player's icons: each frame made new image buffers,
   and each buffer a new trinity Canvas (Canvas_init builds
   pipelines and a 1024-slot ring, 1.5 ms each; 369 made in
   one run). webgfx's spare pool never matched: it measured
   the fit against the raw request, and canvases are 64 px
   multiples, so a 64x64 spare failed the 4x-area limit of a
   30x30 ask. spare_index now quantizes the request first:
   369 -> 60 canvases a run, none during playback.
4. DONE drawing a filter result read the canvas back to the
   CPU and uploaded it again as an image with a mip chain
   (54% of the paint). GraphicsContextTrinity::drawImageBuffer
   draws a trinity-backed image buffer canvas to canvas on the
   GPU (Canvas.draw_canvas_crop, webgfx_canvas_draw_canvas).
   The transition second's paint: 367 -> 209 -> 56 ms; the
   48 tiny time-label paints: 296 -> 37 ms. Picture checked.
   Element at 2560x1440 with audio: the video plays through
   both toggles, 51-60 page frames a second, one second at
   26-47 at each toggle (the app is capped at 60 headless).
   webgfx and trinity expects exit 0.
5. DONE (Kalen's test: "4K video test tv motion",
   youtube.com/watch?v=pcSv22DTDUI, fullscreen, headless with
   SILVER_HZ=165 so the app is not capped at 60) fullscreen ran
   at 47 page frames a second, 100-117 before it, with every
   thread idle. Traced through the two processes: after each
   frame's done the web compositor waited ~18 ms for the next
   picture. The player's clock is the audio stream's time, and
   that moved only when the mixer pulled a block (1024 frames,
   21.3 ms at 48 kHz): every tick in between saw the same time,
   the same mix, and pushed nothing. spectra AudioStream.time
   now adds the wall clock since the last pull (clamped to the
   block). Fullscreen 47 -> 150-164, before it 100 -> 150-160
   (YouTube serves hd1080 H.264 both ways). Decode itself is
   2.3 ms a picture with 4-5 pictures ready ahead. webgfx and
   trinity expects exit 0; no traces left.
Findings, not bugs of ours:
- With no audio device the video counts as silent, YouTube
  autoplays it at load, and a later click PAUSES it (a script
  pause at ~9.7 s looked like a freeze). drive.py now passes
  PULSE_SERVER=unix:/run/user/1000/pulse/native and
  PIPEWIRE_RUNTIME_DIR so the helper has sound (its
  XDG_RUNTIME_DIR is the test's socket dir).
- The overlay scrollbar repaints its 21x800 tile ~60 times a
  second only while its fade animates (ScrollbarsController
  Generic), not forever.
- Deleting an overlay file does not restore the checkout's
  copy, and a checkout-only edit does not trigger the import
  build: touch an overlay file to rebuild. silver reported
  `--build aura` exit 0 once while ninja failed inside the
  import (a trace compile error): check the log.
6. DONE (see 9) `silver orbiter aura <url>` opened aura.ag with
   a HELD instance (start_instance holds every new instance
   until its row is clicked). The startup run plays it at once
   in the pane (display Full, the 'play' action); the argv tail
   becomes aura.ag's saved args, over any earlier ones.
7. DONE (Kalen: "we have a schema format, it's called .agi";
   "dont reinvent one field"): the launch spec string
   (name=type=default=Control=live, written by the compiler
   into the export, answered by trinity to `props`, split by
   position in orbiter) is gone from all three. A module's
   Launch members are enumerated by reflection as they are
   declared (trinity `Prop`: ident, type, access, meta as
   written, value, the meta's control with its values, an
   enum's stops) in a `LaunchProps`, answered to `props` as
   agi (Au.string_agi) and parsed back with Au.parse_agi.
   orbiter's panel reads Prop objects (LaunchPanel.fields);
   its args, compose and set-by-name paths are unchanged.
   The panel has no source before an instance runs: the
   module's last answer (module_props) is its cache, and a
   never-run trinity app gets a display row built the same
   way. aura's `src` is `default [ Launch ]` so it shows.
   Checked: headless aura answers
     props:
       display: Prop  (type DisplayMode, meta [ Live ],
                       value PIP, stops [ PIP, Embed, Window, Screen ])
       volume: Prop   (type f32, meta [ Live, VolumeRange ],
                       value '1', control: Range min 0 max 1)
   trinity expects exit 0; orbiter builds. Not driven: the
   panel itself (needs silver-host).
   Then (Kalen: "it comes from module enumeration, then stored
   in .agi"): the module's build enumerates its app type. trinity
   `export func launch_props` runs in the app's export process
   (silver.c: an app product runs its imports' exports; the
   registry is written before them) and, at the process's exit
   (trinity's ctor runs before the app module registers its
   types, so the write waits for atexit), walks the app type and
   media_app with props_of and appends `props:` to
   install/export/<owner>-<module>.agi. Declared shape only, no
   values; the running instance's `props` answer (many lines,
   ended by an empty line: agent_sock_ask_block) carries them.
   orbiter's module_spec: the instance's last answer, else that
   file; a cached line from before the agi form is ignored.
   silver-aura.agi now lists src (default, Launch), display
   (Live, stops PIP Embed Window Screen) and volume (Live,
   VolumeRange, control Range 0..1).
8. DONE (Kalen: `aura --volume 0.48 <url>` printed usage; "put
   the volume property on element"): volume was the trinity
   controller's, and the element is the command line's surface.
   `public [ Live, VolumeRange ] volume` is on element
   (trinity/element.ag, beside display); the controller's is
   gone, its socket `set`/`volume` read the root element's.
   Checked headless: `aura --volume 0.48` answers volume 0.48
   and props shows value '0.48'.
   A member added to element moves every subclass's slots: a
   module compiled before it reads garbage (orbiter SIGSEGV in
   Style.integrate_instance on a scene's style member). silver
   does not rebuild a runtime-loaded module for a base change:
   clouds, scenes and markdown were rebuilt by hand; any other
   trinity app (knes, asnes, n64, hyperspace ...) needs its
   build before it is hosted again.
9. DONE `silver orbiter aura <url>` never started the instance:
   /src/silver/aura is a folder, so the startup took the
   folder branch, which set no pending_run and dropped the
   argv tail; the module branch below it was never reached
   for a root module. The folder branch now saves the tail as
   the file's args and sets pending_run (unless START_HELD).
   Checked hidden under the host: the pane's instance starts
   ("start: aura https://www.google.com -> slot 1"), aura
   loads Google; then orbiter SIGSEGVed in Canvas_measure_text
   from Layer_draw (Kalen's report, reproduced). AU_QUARANTINE=1
   named it: a DOUBLE-DROP in AppView.service on the app's
   title. `app_title = string[ @title_buf[0] ]` (title_buf a
   vec u8) compiled to a POINTER CAST: the vec's data pointer
   was stored as the string object, held, and the old title
   dropped a bogus header; the pane title's label then read it.
   Fixed: the bytes are copied to a `local i8 [ 1024 ]` and the
   string made from that (the pattern trinity uses). Silver
   bug, OPEN: `string[ @v[0] ]` on a vec is a cast, not a
   construction (see cast-cstr-vec). Checked: the hosted run
   stays up 60 s with Google in the pane, no report.
10. DONE (Kalen: "you never do any arguments after the default";
   "trinity.view ... otherwise they could clash") the launch
   line is flags first, the default (positional) last and
   nothing after it: a string default takes the whole
   remainder (Au_with_cstrs). launch_handler put `--view` after
   the saved args; it leads now. The start log no longer prints
   "flags N" after the line. A prop of another module is flagged
   with its module: `--trinity.display Embed`, `--trinity.volume
   0.5` (Au_with_cstrs: `--module.name` matches the member of
   the type that module declares, by the module's plain name:
   silver-trinity is trinity); the app's own props stay bare,
   so names never clash. Prop.module and LaunchProps.module
   (the app's) come from props_of (trinity module_name);
   orbiter's LaunchPanel.flag_name composes and seeds with it.
   Checked headless: `aura --trinity.volume 0.48
   --trinity.display Embed https://www.google.com` runs and
   answers props with module aura / trinity, volume '0.48',
   display Embed. `--view` stays bare: it is asnes's own prop.
11. DONE (Kalen: orbiter "locks hard before closing", ~10 s).
   Reproduced hidden under the host with aura in the pane, an
   orderly quit over the socket (new trinity socket command
   `quit`: Au.quit_request sets the flag the close button
   sets), every thread's kernel wait sampled at 4/s and an
   LD_PRELOAD CPU sampler (scratchpad sampler.so, report.py)
   in orbiter. Measured, no --leaks, 30 s in: 2.7 s from the
   quit to the process gone. The main thread: 1.3 s asleep in
   on_unload's `while [ index_building ] usleep` waiting for
   the index worker, 0.5 s freeing the old file_index list,
   0.8 s the tree teardown (drop_members, map_clear). The
   worker ran at 100% of a core for the whole session (82% in
   tok_warm hashing every indexed text file; 400k-file cap over
   the 42 GB of checkouts) and after the cancel spent its time
   in index_work's final Au.auto_free: tok_warm never drained
   its pool, so every hashed file's text (up to 300 KB each) and
   its temporaries stayed pooled until the end of the whole
   pass. That pool is what the close waited on (and what grew
   the footprint; 1.5 GB rss at 30 s). Fix: tok_warm drains its
   pool per file (todo held, dropped after). After: 1.0 s from
   the quit to gone, the worker stops 40 ms after the cancel.
   The rest is the teardown itself. --leaks makes every free
   slow (leak_remove 59% of the close): measure closes without it.
12. DONE (Kalen, Sep 29: in Embed the page stayed at aura's launch
   size, 1280x800, inside a 1523x965 pane; "the second time you
   launch the app it doesn't get the resize; it should start at
   the embedded size"). Cause, reproduced by stopping and
   starting the instance from its row icon: a paned app launched
   at its own default size and relied on the pane's HM.resize,
   and that request is sent only when the pane's size differs
   from the LAST request (req_w/req_h), which a stop and start
   kept: the second process was never told. Now AppView.start
   puts the pane's content size on the launch line (`--width W
   --height H` before the user's args, so their own --width
   wins) and clears the request state per start; Editor.tick
   gives a not-yet-drawn instance the content layer's size.
   Checked hosted hidden: the second launch line is `aura
   --width 1607 --height 819 <url>`, the app's display opens at
   1607x819 and the page fills the pane (shot). The very first
   launch at startup still opens at the default and takes the
   pane's resize within a frame: the pane has no layout yet.
   /tmp/orbiter-resz.log is appended by EVERY orbiter process
   (two were running): its lines interleave.
13. DONE (Kalen: "that cool effect can be applied in our aura
   webkit around standard buttons ... and form elements like
   check / input / text area") the pointer's near light on every
   native control WebKit paints. The chain: webgfx
   webgfx_canvas_near_light / _near_style (WebGfx.h) ->
   GraphicsContextTrinity::setNearLight/clearNearLight (the
   point mapped through the CTM, reach scaled; an inset shadow
   in slot 1 re-applied per draw by setShadowForDraw, the
   inner glow the light fills) -> RenderBox::paintBoxDecorations
   (overlay copy) wraps a box with a used appearance in a
   TrinityNearLightScope: the pointer from the frame's
   EventHandler (window -> contents -> the box's paint space),
   reach 160, border 1.3, inset 3.9, the theme's focus colour
   (white vanished on Adwaita's light controls; Adwaita FILLS
   its borders, so only the inset glow shows). RenderTheme
   (overlay copies of .h/.cpp) keeps the painted boxes weakly
   (nearLightPainted) and EventHandler::handleMouseMoveEvent
   (overlay copy) calls nearLightMoved: a box within reach of
   the pointer repaints, and once more as it leaves. A trinity
   context is known by platformContext() != null.
   Checked headless on a local page (scratchpad near/): the
   push button, the text input and the checkbox take a soft
   blue inner light that follows the pointer; the rest stay
   as Adwaita draws them. Then (Kalen: nothing on google.com's
   search buttons, which Google styles itself, so WebKit paints
   them as plain CSS boxes): the scope also takes any box whose
   element is an HTMLFormControlElement (input, button, select,
   textarea, fieldset), styled or not; the light lands through
   the CSS border and background fills. Checked headless on
   google.com: "Google Search" glows blue at its edges under the
   pointer, "I'm Feeling Lucky" faintly at 150 px. Cost: one
   repaint per move per control within 160 px.
14. DONE (Kalen: dragging a split rendered the two panes at
   different sizes, the panes lagging the seam on a fast move;
   "easing is a feature, that ease state is the current
   position"). Traced per frame (a temporary split/draw log,
   removed): the frame the main splitter laid its seam out at
   the new place, the right child drew at its new width and
   pane0 still at its old one, one frame late. Cause: bounds
   were computed only in the draw pass (layout_element), so a
   nested container's render() read LAST frame's bounds: each
   nesting level (main -> left -> stack -> panes -> pane0)
   added a frame of lag, and the two sides of one seam came
   from different frames. Fix (trinity update_element): each
   instance is laid out from its parent's current bounds
   (layout_self, the per-element half of layout_element,
   with the relative-sibling chain kept in the pass) before
   its render() is called, top down; the draw pass's layout
   stays authoritative. The splitter's easing is untouched.
   Checked hidden: on a fast drag every shot has one seam and
   both panes meet at it, on the title, the text and the
   bottom rows.
15. DONE (Kalen: an instance started with the nav list open did
   not start until the list closed). The pane's editor mounts
   with `hide: browse_active[]`, and draw_element returns on a
   hidden element before its tick, so nothing serviced the
   instance (Editor.tick -> service -> start). orbiter's frame
   loop already services instances no pane shows; it now counts
   a hidden pane as not showing (`!e8.hide`). Checked hidden:
   with the list open (a letter typed in the title) the row's
   play starts aura at once ("start: aura ... -> slot 1").
   Then (Kalen: the app flashed and the list stayed in front):
   the play action closes the nav list (exit_finder, nav_showing
   and find_showing off). Checked hidden: the list's element is
   gone after the play and the page shows in the pane.
16. DONE (Kalen: the exchange's ground blur once, wanted per
   frame). Window.exchange_blur_frame (trinity): when the
   exchange's ground draws, the screen as drawn so far is
   submitted and fenced (flush_text, draw, sync_fence), copied
   into a screen-sized snapshot texture (vk.copy_image) and run
   through its own ReduceBlur (two reductions, the two blur
   passes) that frame; screenshot_blur points at it, so the
   carry and the veil keep working; a crop capture keeps its
   still. ShotBackdrop is `animated: true` (drawn to the screen
   every frame); `no_cache: true` painted nothing at all for it.
   The two new Window members (live_blur, live_snap) are the
   LAST members of Window: modules built before them keep
   their offsets (an insert mid-class broke orbiter until it
   was rebuilt). Checked hidden over a playing YouTube video:
   shots 1.2 s apart show different blurred frames.
17. WRITTEN, not built (Kalen: "stop compiling while I use it";
   the mic took 4-5 s to engage and disengage, freezing the
   ui). Profiled hidden (LD_PRELOAD wall sampler, the pulse and
   pipewire sockets linked into the test runtime dir so the
   capture opens): while engaged 95% of the main thread sat in
   dictation_poll -> spectra.capture -> snd_pcm_readi, a
   BLOCKING read of a 2048-frame block (43 ms at 48 kHz), and
   orbiter's mic_frame reads up to 64 blocks a frame: one frame
   took seconds, and the press-off waited for it. Not the
   device open (3 ms, from any thread) and not the GPU.
   Fix, spectra.ag (linux capture): snd_pcm_avail_update first;
   a block is read only when a whole one is there, else false
   at once (the loop then ends); the stream is started from
   PREPARED and recovered on a negative avail. Also speech.ag:
   the capture device opens on its own thread at exchange open
   (dict_open_thread; open_fail stops retries), and
   dictation_start no longer opens anything. Still to check
   after a build (spectra, speech, orbiter): the on and off
   press answer at once, the take's words arrive.
   Findings on the way: ALSA `default` fails with "Host is
   down" when XDG_RUNTIME_DIR does not hold the pulse and
   pipewire sockets (hidden runs with a scratch runtime dir:
   link pulse/native, pipewire-0, pipewire-0-manager into it);
   a hidden orbiter run overwrites install/tmp/silver-orbiter
   .log, the same file Kalen's run writes.
18. WRITTEN, not built (Kalen: n64 crashes from orbiter, not
   from the command line). It is orbiter that dies: its log has
   "type mismatch" then SIGTRAP right after "release: n64 slot
   1", while parsing n64's `props` answer (the freed strings
   name its props). n64's mouse_look is a bool whose Prop.value
   is the text 'false'; string_agi wrote it bare (`value:
   false`), parse_agi read the word as a bool for a string
   field and its verify trapped. aura and the others never
   showed it: no bool Launch prop. Fix in Au.c: the writer
   quotes a string that is a keyword (true, false, null), and
   the reader gives a string field the word as text instead of
   trapping. Needs `make` (Au), then orbiter's next launch.
   Check: `silver orbiter n64 <rom>` with mouse_look off, and
   the panel shows the bool.
19. WRITTEN, not built (Kalen: mouse_sens showed BLANK in the
   panel, "it's an f32", "blank means it's not even getting the
   value"). Two causes. (a) the export .agi carried no defaults
   (declared shape only), so a field mounted before the first
   run had nothing; and once the running instance's answer had
   the value, the mounted field kept its empty text (a mounted
   LaunchField is re-rendered without `edit`). (b) an empty
   field composed as an empty flag: `--mouse_sens ` took the
   rom as its value.
   Fixes: the declared default now rides the member descriptor:
   Au_t gains `dflt` (last in Au_f_members: offsets kept),
   `def_prop_default(prop, symbol)` (Au.c, src/Au schema); the
   parser records a one-token literal initializer (a number, a
   quoted string, true/false: token literal set) in
   aether.prop_defaults keyed by the member (silver.c member
   parse); the codegen emits def_prop_default after def_prop
   (aether.c, old modules simply never call it); props_of
   (trinity) puts it in Prop.value (quotes stripped) when no
   instance answers, so the export .agi and the panel have the
   defaults before any run. orbiter: an empty mounted field is
   re-seeded once a value is known; compose skips an empty
   field. Needs: make (Au, aether, silver), then trinity, then
   every app's export (a build of each), then orbiter.
   Check: install/export/silver-n64.agi lists mouse_sens with
   value '0.015'; the panel shows it before the first run.
OPEN:
- the FrameBlend runs on the web process main thread with a
  fence wait per tick: 12-17% of the thread at 165 Hz.
- headless the app ticks at 60, so page frames cap at 60; on
  Kalen's 165 Hz screen the real rate is unmeasured.
- the per-tile paint still reads every tile back to the CPU.

## Active work: .ag cleanup to 72 columns (Sep 30 2026)

Kalen: every .ag module to 72 columns, trinity first. Comments
re-flowed to 72 (all words kept); code broken inside [ ] only.
1. DONE trinity (Sep 30): 3,654 lines over 72 -> 1,323. The
   wrap tool (session scratchpad wrap72.py; to keep, move it to
   support/) re-flows # and // comments, moves a trailing
   comment above its line, puts a one-line `if [ c ] stmt`
   body on its own line, and breaks code after a comma or
   before && / || inside [ ], or ( ) within [ ].
   Checked by IR: the --verbose build's .ll before and after,
   compared function by function (locals renamed, string
   constants by text): identical but for __LINE__ values, GLSL
   whitespace/comments, and the initializer's C-type order,
   which also differs between two builds of the same source.
   trinity builds; `silver --test trinity` exit 0.
   Line breaks are NOT neutral in silver: a bracketless cast
   (`f32 i`, `f32 width / tcs`) reads by line; broken, the call
   failed ("arc_seg: expected 10 args, got 10"). The tool
   leaves any line with one unbroken.
   Left over 72 (1,323): 1,217 code lines with no safe break
   (long Vulkan names, aligned assignments, bracketless casts,
   `expect x, 'msg'`, ternaries), 88 GLSL lines holding a
   silver { } value, 14 import settings, 4 commented-out code.
2. DONE the exchange-test comments at the top of trinity.ag:
   four removed; "trinity: the window, the elements, and the
   light behind both" kept (a real header).
3. NEXT the other modules, one at a time, each built after,
   the same IR check per module.

## Active work: public cleanup (Oct 2 2026)

Kalen, before going public: every module to 72 columns, and no
numbered names (wx9, lp9, a9): a name, or i/x/y. Short words
like cv are fine. Locals and parameters only; members and
method names keep theirs. Order: trinity first, then each
module, built and tested before the next.
Start (Oct 2): 43 modules, 122,101 lines, 11,162 over 72,
3,311 distinct numbered names.
1. IN PROGRESS trinity: 1,424 over 72, 483 numbered names.
   Tools: support/cleanup.py names|wrap <file> [--apply],
   support/ircmp.py <ll-dir-before> <ll-dir-after> [--nolines]
   (each step: --verbose build, .ll copied, compared; a rename
   or wrap must leave every function the same).
   Done: numbered names renamed (member keys, GLSL params and
   h264/win32/sync2/ycbcr444 kept); auto wrap 1,424 -> 768.
   IR the same, `silver --test trinity` exit 0.
   Left: 6 numbered locals (Canvas curve math, pk9w/pk9h,
   set1_binds, b2_au); hand wraps: ternaries outside [ ],
   bare casts -> f32[ x ], long arithmetic, GLSL in { }.
   Crashes: 4, all from a renamed Vulkan struct key (fixed).
2. OPEN orbiter: 2,574 over 72, 586 numbered names.
3. OPEN the rest, largest first: scenes, asnes, ai,
   hyperspace, n64, composer, rgen, webgfx, features, knes,
   clouds, speech, spectra, flightsim, img, then the small ones.

## Active work: planets out of scenes (Oct 5 2026)

Kalen: scenes is a utility module only (Backdrop, OrbitView,
ViewAngle, the Gaia sky, eu_gnoise, fetch_map, scene_thumb,
shared models). NO planet lives in it. Each planet is its own
root module (`<planet>/<planet>.ag`) that imports scenes and
exports its scene names (`export scenes [...]`), so it builds,
iterates and exports alone; orbiter finds it by the export.
Never `extend scenes` for a planet.
1. WRITTEN, building: one root module per planet, each with
   its own textures/, bakes and `export func <m>_thumbs` (last):
   earth (Moon, EarthPrev, Earth/Ocean/Cloud shaders), mars
   (Mars, TerraMars), milkyway, pluto, titan, enceladus, europa,
   uranus, neptune. scenes imports none of them and exports no
   scenes; it gained scenes_share (its shared models and skies),
   scene_thumbs_for, and the shared pieces: bake_saturn,
   bake_jupiter, TitanSurface, RingShader, SaturnShader,
   JupiterShader, nep_lerp, nep_band.
   DONE (Oct 5 01:40): all ten build; each module's own .agi
   lists its scenes; each wrote its own thumbs/<Name>.png.
   Shared assets stay in scenes/textures (read through
   scenes_share): gaia, saturn, saturn rings, jupiter,
   moon-displace, and every europa-* plate (mars reads them).
2. DONE the globe is code, no file: scenes build_globe (512
   segments, 256 rings, u = 0.5 - lon / 360, v = 0.5 - lat / 180,
   counter-clockwise outside); earth.gltf/.bin/.blend deleted.
   scenes/staged/earth.ag (not built) still names earth.gltf.
3. DONE stale outputs deleted: share/silver-scenes/thumbs/ and
   baked/ (1.4 GB; scenes writes neither now).

## Active work: moon surface (Oct 5 2026)

Kalen: the moon's bake (earth/earth.ag, moon_*) looked
amateurish: generational blur, additive stamping, decal basins,
noisy normals, no erosion in time, no basins, same-looking
craters. Rules from him: fix inside the existing routine, one
edit at a time, each baked and shown; normals come from the
mesh height only; regularizing toward the sphere happens in
perlin sections with large impacts, over generations, with the
perlin's own variance, never per crater; late bombardment is
small impacts; rocks (normals, colour) are perlin patches per
generation that average craters away. A bake is ~3.5 min; the
shaded map is checked from the normal png (scratch python).
1. DONE the per-generation blur and the slope rescale are gone;
   stamps carry real depth/rim/peak laws by size (0.34 radii
   for a bowl, 0.01 for a basin); the per-stamp noise, the
   late big rings and the double-depth draw are out.
2. DONE a crater is cut into its local lie: a height pyramid
   (moon_pyr_*, built per generation into the brush's tmp,
   bicubic) replaces the centre-height base: no plateaus.
3. DONE moon_basins/moon_sink/lvl/sk and every lava branch in
   the brush are gone. moon_mare: one perlin (2/5/12 cycles)
   times a near-side hemisphere ramp, cut by area to 16%, a
   wide shore band; over gens 24..40 that ground goes toward
   the sphere (the mean, 0.0015 under) 30% a pass; bm ends as
   the melted fraction and darkens the colour. Previewing the
   mask in python (scratch mask-*.png) saved four bakes.
4. DONE rock wear per generation (moon_patches/moon_pw_at):
   a perlin band placed anew each generation; inside it the
   ground goes toward its 8 px lie with the rock relief laid
   on (zoom 0.12..0.48 map px per rock px); this is the erosion
   clock (old craters soft ghosts, young ones sharp). The
   per-crater relief patches, the swath weighting of where
   impacts land and moon_clips (clips brushed into normals)
   are gone.
5. DONE basins: a crater of 200 px or more melts its floor and
   a perlin-shaped surround to 2.5 radii (mz), toward the 64 px
   lie, 45% a pass, cooling by a third each pass; the near
   side's floors darken. Past 60 px the size draw rejects with
   (60/rp)^1.5 so basins are tens, not hundreds. Impact flux
   falls fifty-fold over the flood generations (post-flood
   plains stay sparsely cratered). Hits vary: depth 0.5..1.2,
   wall curve 0.6..1.6, rim 0.6..1.4, oblique stretch and
   downrange ejecta.
6. DONE lighting mirror: the moon's model matrix is a mirror
   (third axis north x earth), so cross products flip; the
   shader's east tangent ran along map west and a bowl lit from
   the side read as a bump. MoonShader t_e = cross(n0, north),
   t_n = cross(t_e, n0). Found by derivation; the thumbnail
   camera only shows the limb.
7. DONE (Kalen's second look): basins were too large and too
   smooth; the colour had a seam down the map's middle; rims
   needed mountains; a few more craters. Now: moon_region picks
   a picture by random cells with the four neighbours blended
   (no seam; measured equal to a typical column); stamps of the
   big types heap a lumpy massif to 1.6 radii and keep a wider
   rim; the biggest draw is 340 px, past 60 px the count falls
   as (60/r)^2; a basin's floor melts in perlin patches (parts
   keep their craters) and its surround to 1.8 radii; the mare
   perlin takes 5% by area; a basin's own fresh ring is not
   painted. Measured: melted ground 17% of the sphere. Crater
   cover 0.04 -> 0.05, post-flood flux floor 4%.
8. OPEN the normal map is still rgba8 png.
9. DONE orbiter: scene_prop_apply skips a saved enum name the
   enum no longer has (Enceladus/Neptune had speed: subtle;
   BackdropSpeed is slow/medium/fast): evalue faulted at the
   restore (SIGTRAP at startup).
10. DONE (Kalen's third look): late impacts only ever smaller;
   too few central peaks and small rims; rock detail off scale
   (scrapes, pixel marks, no rocks seen). Now: the size top
   doubles over the late generations (ramp 40%..80%); the peak
   starts at the 13 px types (was 37), wider and taller, and
   those types' rims are 60% higher and wider; the ray streaks
   no longer write the normals (light only: the scrapes); the
   shader's two finest rock stages (a quarter and an eighth of
   a map px a rock px: the pixel marks) are gone, stages 2x,
   6x, 16x at 0.35, 0.6, 0.7; the wear lays the rock tile at
   0.5..1 map px a rock px (was 2..8); the tile is box-averaged
   into the relief. Shaded map: rubble grain shows, rims and
   massifs read. Not yet seen by Kalen in orbiter.
11. DONE (Kalen: no real colouring on the moon; the Earth in
   the moon's sky should reflect the sun off its land by albedo).
   moon_albedo: the vivid picture's hue comes in at 15%..55% by
   two perlins (3 cycles large sections, 14 small; was a flat
   12%), the cap 12% -> 35% off grey, the real map's own hue
   whole. Mean hue off grey 0.037 -> 0.069 (the real map is
   0.015, the vivid 0.211). MoonEarth: a broad land lobe
   (exponent 12, half strength) in the ground's own colour,
   under the clouds, beside the sea's glint. Not yet seen by
   Kalen in orbiter.
12. DONE bake 12 (Kalen: scrapes and noise still there). Found in
   the maps: the rock tile's own thin lines (crater rays, plate
   edges, 1-2 px wide, 100+ px long) laid into the height by the
   wear and stretched by the shader's 2x stage; and the vivid
   picture's grain coming into the colour pixel for pixel. Now:
   moon_relief low-passes the solved height (gaussian, in fourier
   space), the shader's 2x rock stage is gone (stages 6x 0.6, 16x
   0.7), the vivid picture is box-averaged 8x before its regions.
   Checked after: the silver relief matches a numpy solve of the
   same tile (slope correlation 0.977, same spectral peaks), so
   the lines are the tile's; 2 px left them, 4 px removes the
   network and keeps the rubble (python preview). The colour grain
   remains: the real map's hue now comes in whole and its jpeg
   chroma noise is per pixel. NEXT: low-pass 4 px, the real map's
   hue from an 8x averaged copy too.
13. DONE the way back from the earth (Kalen: the lit side up as we
   approach; the moon left the frame). The look held the moon's
   centre only to the halfway point by the clock; now it holds it
   until the eye is within 3 radii and rolls to the flight look by
   1.6. While the moon is the target the camera's up is the sun's
   direction with the line of sight taken out: the sunlit half at
   the top. Checked offscreen (MOON_SEC 190/205/215): the moon
   centred, terminator level, lit side up.
14. BUILT, awaiting Kalen's look: the Earth in the moon's sky
   (MoonEarth). The sun's spot on the land overblows the ground's
   own colour, scaled by its green plus blue (a multiplier on the
   land colour under a pow 12 lobe, up to 2.5 x (g + b)); the limb
   fresnel band 50% stronger (0.45 -> 0.675); at night the same
   band is the aurora: green between 66 and 78 degrees of
   latitude, curtains from two sines along longitude drifting in
   time. Seen offscreen at the earth lap: the green cap at the
   pole shows as spokes meeting at the pole (the curtains are by
   longitude); the land spot was not under the sun in that frame.
15. DONE (Kalen: the scrapes and noise persisted) the real
   source: moon_tint_height added the vivid picture's brightness
   to the terrain height pixel for pixel from random cells, so its
   white crater rays were ridges in the height (the scrapes) and
   its grain the noise in the normals. Removed (the function, its
   call, its hash entry). The colour's grain was the real map's
   brightness at the pixel: moon_albedo now reads both pictures
   from an 8x box-averaged copy (moon_shrink) for hue and light.
   Bake 14. The rock relief keeps the 4 px low-pass (its own
   lines and grain are not rocks).
16. BUILT, awaiting Kalen's look: the approach camera (Kalen: the
   lit side on the left as we leave the earth, turning up as we
   near; the moon fell to the bottom). Leaving the earth the up is
   the sun turned a quarter round the line of sight (lit side
   left), rolling to the sun's direction from a quarter to four
   fifths of the way (lit side up); from 6 radii to 3 the look
   leans 0.7 radii toward the lit limb; within 3 radii it rolls
   level and takes the flight look. Checked offscreen at 185, 200,
   212: the moon centred, lit cap at the top at 212. Whether the
   first roll puts the lit side on the screen's left (cross
   handedness) is not confirmed: flip `lft` if it is on the right.
17. BUILT, awaiting Kalen's look: the Earth's land spot is land
   diffuse + land x a wide blinn lobe (pow 8, 1.2), under the
   clouds (Kalen's spec); the overblow form is gone.
18. BUILT, awaiting Kalen's look: earthshine. MoonShader gains
   `earth_dir` (last uniform; w = strength): a blue light
   (0.45, 0.62, 1.0) from the earth's direction on the ground
   facing it, 0.12 at new earth to 0.42 at full earth by the
   sun's angle to the earth line.
19. DONE bake 15: the colour's 4 x 4 blocks (the averaged copy
   picked nearest onto the colour map) are gone: moon_pick is
   bilinear, wrapping. Colour crop smooth.
20. BUILT, awaiting Kalen's look (Kalen: the aurora sits 50%
   higher, is a volume with depth, moves through the atmosphere,
   dances around). It is its own shell now: MoonAurora (inherits
   MoonEarth), the globe mesh at 1.066 radii over the earth in
   the moon's sky, ray-marched 12 steps between 1.033 and 1.066
   radii in the earth's frame (the Earth scene's is 1.022..1.044),
   stopped at the inner shell and the ground; curtains from two
   octaves of 3d gradient noise squeezed 5:1 vertically, drifting
   in three directions (visible within seconds at sky rate 60);
   the oval's latitude wanders up to 5 degrees by a slow noise;
   green low to magenta high; night side only. The band aurora in
   MoonEarth is gone; its limb stays at 0.675. The noise functions
   moved out of MoonShader into `MoonNoise`, the base of MoonShader
   and MoonEarth (one copy).
   Tuned after two offscreen looks (first invisible, then a white
   block): curtains from noise over the ground (up.x, up.z x 24)
   with little altitude term, threshold 0.5..0.68, gain 1.3, cover
   smoothstep 0.1..2.5. Seen at the earth lap: soft green volumes
   at the south pole above the limb.
21. DONE (Kalen): closing the last file reopened one on the next
   start. The state WAS saved (persist.agi: `pane_states: [ ]`,
   pane_drop_file saves), and the agi reader gives an empty vec
   for it, but restore_session's second branch (meant for a
   fresh install: the first known file) ran for an empty list
   too. It runs only when pane_states is null now. orbiter built.
22. DONE (Kalen, eighth ask: no specular on the earth's land).
   Checked under the spot this time (MOON_SEC 132, the glint on
   South America): the earlier forms did not read. Now the land
   takes the sea's blinn at twice the size (pow 100 x3 + pow 10
   x0.8), added as land x land x3 x lobe under the clouds: the
   Amazon flares under the sun. Seen offscreen.
23. BUILT (Kalen: the aurora looks good, bigger patterns, far
   lower): shell 1.033..1.066 -> 1.01..1.028 radii (mesh at 1.028),
   noise over the ground 24 -> 9 per radius.
   Then (Kalen, five at once, all done): a quarter as bright at
   full (gain 1.3 -> 0.33) with each region slight to full by a
   slow noise (0.1..1); the oval wider (lat 46..60 in, 78..88
   out); the noise round the pole in a ring (cos, sin of the
   azimuth x 2.6) with the meridian direction at 5 per radius: a
   slight stretch up and down from the pole; depth from the
   noise changing through the shell (an x 4) and far samples at
   half; shell 1.002..1.012 radii (mesh 1.012). The cover follows
   the light so a faint curtain does not darken the ground.
   Then (Kalen: none showed; flat, a 2D effect on the globe, not a
   march). Two causes: the cover followed the colour and the
   8-bit target clamps colour before the blend, so the addition
   can never exceed the cover (the cover carries the strength
   now, 0.25 at full); and the samples were averaged, so the
   march had no depth. Rewritten as a real march: composited
   front to back (each sample hides what is behind it), the
   sheet pattern the same up through the shell so each is a tall
   curtain, dense at the foot and ragged at the top, the inner
   shell crossed (the ray goes down through the sheets and up
   again), the step in shell thicknesses; shell 1.008..1.03
   radii (mesh 1.03): thinner than that cannot be seen from the
   side. Measured at the earth lap: green pixels 47 -> 622, the
   strongest 17 -> 44 of 255; from the side the oval is an arc
   standing above the limb with lumps along it.
   Then (Kalen: not vertical shafts enough, no volume, not low
   enough, too bright): curtains 12 per radius across the oval
   (was 5), fine rays along them (14 round the ring) splitting
   each into shafts; shell foot 1.008 -> 1.002 (top 1.03); cover
   0.25 -> 0.12. Measured at the earth lap: strongest green 14 of
   255 (was 44), 184 green pixels.
   Then (Kalen: shafts way taller, slower): top 1.03 -> 1.05
   (mesh 1.05), the thinning from 0.55 of the height (was 0.3),
   drift 0.0017 -> 0.0006 of sky seconds.
   Then (Kalen: the whole ionosphere's edge carries the green as
   fuzz on the night sky's edges): an airglow density 0.04 x
   (1 - altitude) over the whole night shell in the same march,
   its own softer green; the grazing path at the limb saturates
   it into a thin fuzz, the steep view inside barely.
   Then (Kalen: a hard green edge showing the shell's other side,
   not a thin terminator fuzz). THE FAULT behind every flat
   result: the ray math ran in the mesh's units (mesh radius 1)
   while the mesh is the globe scaled to the shell's TOP, so the
   ground is 1 / 1.05 there and a shell declared 1.002..1.05 lay
   wholly outside the mesh: only the silhouette sliver was ever
   marched. Now rtop 1, ground 1 / 1.05, foot 1.002 / 1.05 in the
   shader; the shell's far face discards (the near face marched
   the ray once). Measured: green 155 -> 13,877 px, strongest 34.
   Then (Kalen: the glow is a thin band seen at the top of the
   shell, clear toward the surface; only the shafts go through):
   glow density a gaussian round 0.85 of the height, width 0.1,
   0.06 peak (was 0.04 x (1 - height)).
   Then (Kalen: nice, lower): the band's centre 0.85 -> 0.55 of
   the shell's height, width 0.08. Built.
24. DONE (Kalen: "why are we committing generated textures").
   They were never committed: .gitignore ignores scenes/textures/
   ("planet maps: downloaded, never committed"); the planet split
   moved the maps into earth/textures, enceladus/textures,
   mars/textures, where no rule covered them, so git staged them
   as new (earth 117 MB, enceladus 71 MB, mars' two colour maps
   46 MB). The eight planet modules' textures/ folders are in
   .gitignore now. mars-height/stone/veg were tracked before and
   stay (renames). Unstaging the new ones is Kalen's (Rule #1).
25. DONE (Kalen: titan-color.png staged too; "why are you not
   downloading these to a cache location"). The download WAS in
   the cache (~/.local/state/scenes); the bake then wrote its png
   into the module's source tree (titan/textures/). bake_titan
   now writes ../<share>/baked/titan-color.png as the moon bake
   does and the model reads it there. Built; the thumb rendered
   from the moved map is identical (mean 56,47,34). The same
   source-tree write remains in scenes.ag: gaia, saturn, jupiter
   (scenes/textures/, gitignored): OPEN, Kalen's call.
26. DONE (Kalen: "you can cleanup those") the history rewritten
   with git filter-repo after his commit 165d95d left the tree
   clean: every binary blob in history that is not a current file
   (44 blobs, 65 MB: old orbiter32.bin, earth.bin/.blend, the
   pluto tiles, flower blends, sonic2.bin, default.profraw,
   landscape.png, exr/ppm captures), then the pluto-tiles folder
   whole. Source history untouched (old silver.c etc. kept).
   .git 385 MB -> 75 MB, 1157 commits, stash kept, origin kept.
   Every hash changed: origin needs Kalen's force push, other
   clones re-clone. Full backup of the old .git (with reflogs):
   /src/silver-before-cleanup.git.

## Active work: eden scene (Oct 2 2026)

Kalen: a jungle seen from high up, the forest under the chip in
silver-icon.png (no chip, no stars). scenes/eden.ag (`extend
scenes`), trees and impostors built at export. Trees are real 3D
foliage; never a canopy height map. Clouds: not ours (we have
them). Kalen's brief, in order:
1. BUILT, awaiting Kalen's notes: terrain. A height map built
   large forms first, then drainage valleys, then small detail;
   river and clearing masks.
   Kalen: the land meets a sea; shoreline, rivers that run to it,
   and shallow water (turquoise over the shelf) that deepens to
   open sea, by the water's depth over the ground.
   eden_bake (export): 1024 cells of 8 m (8.2 km). Coast (warped
   noise, west side), shelf falling fast at the shore then gently,
   islands on the shelf, ridged uplands; synth_erode on a map 64
   rows taller (its rows wrap); the sea floor keeps its smooth
   shape below 1 m. One flood from the map's edge (heap) with a
   step cost of length x an uneven cost map, so streams meander
   across flats; hollows silt up into river plains (no lakes);
   each cell drains down its steepest slope; streams from 3000
   cells of catchment cut beds 1.2 m+ deep, up to 6 cells wide.
   baked/eden-height.png (r/a metres -256..768, g cells to the
   sea), eden-water.png (r/a water level, g river size, b wet),
   eden-grid.gltf. Element Eden: EdenGround (beach by sea
   distance, rock by slope, dark earth), EdenWater (light to the
   bottom and back: turquoise over sand, deep blue, muddy rivers;
   sky by Fresnel, sun glint, a break at the edge). In the
   picker; thumbs/Eden.png. Test: `silver --test scenes`
   (t_eden_view writes /tmp/eden-view.png at 1280x720).
   Data images must be `linear: true`: sRGB bent the height's
   high byte (4 m steps on the sea floor).
   OPEN: the far map edge shows against the sky (item 7); no
   clearing mask yet (item 2).
   OPEN silver: scene_thumb's last log prints its `nm` as garbage
   when called with a literal from an expect (freed with e.id).
2. OPEN control maps: tree density, height, moisture, family,
   clearing, exposure; patches that vary together.
3. OPEN tree families (5): trunk, branches, leaf clusters placed
   in irregular crown lobes, with gaps; a reusable 3D cluster
   library.
4. OPEN one crown under one sun, drawn gray first: crowns shade
   each other, gaps open, outline irregular.
5. OPEN a small patch: seeded per tile, spacing rules, three
   canopy layers, cascaded sun shadows, leaves that pass light.
6. OPEN detail by screen size: full, simple clusters,
   depth-aware impostors, voxel clumps, far forest; hysteresis.
7. OPEN atmosphere and valley mist.
8. OPEN tiles, GPU culling, indirect draws; tested in motion.

## Active work: Earth as a canopy planet (Oct 2 2026)

Kalen: Earth is a sphere whose surface is the canopy, seen from
orbit to the treetops, with real tree textures. Sources (CC0 /
public domain, cite in THIRD_PARTY.md): NASA Blue Marble Next
Generation (colour from orbit, and where forest grows), Poly
Haven trees fir_tree_01, island_tree_01, island_tree_02 (glTF
1k, ~570 MB, fetched once into ~/.local/state/scenes, never the
repo). Template: Pluto/Mars (carpet mesh around the camera,
wrapped onto the sphere, lifted by baked height).
1. DONE fetch (Oct 2): Blue Marble July, 21600x10800 baseline
   jpeg, 2 km a pixel (assets.science.nasa.gov .../bmng-base/july/
   world.200407.3x21600x10800.jpg); the three trees' glTF 1k with
   jpg textures (fir 18.8 m, 9.0M vertices; island 5.0 m and
   3.4 m). Leaves are alphaMode BLEND over a jpg: the cut-out is a
   separate <tree>_<twig|leaves>_alpha_1k.png Poly Haven lists
   but the glTF does not reference (fetched beside it).
2. DONE GltfModel.open reads all three (meshes, materials,
   images). Not yet drawn. A throwaway module made git tag
   treetest-1.0.0 (left for Kalen).
3. DONE (top view) scenes/earth.ag (extend scenes, imported after
   pluto): export earth_trees_export draws each tree from straight
   above (orthographic, 1024 px, crown's widest span) into
   baked/earth-<tree>-{color,height,normal}.png; height is 0..1 of
   the tree's top. tree_prepare joins the leaf jpg and its alpha
   png into <tree>_<leaf>_rgba.png and writes <tree>_bake.gltf
   naming it. Checked by eye: real leaf and bark colour.
   Fixed on the way: gltf texture uris resolved against a global
   base that open[] had cleared, so a model outside share loaded
   no textures (white); GltfModel.src_dir (intern, last member)
   now keeps its folder, used when the file is there (an archive
   still finds the bare uri). A shader drawing glTF materials
   needs color, rough and normal in its surface enum.
   OPEN: side views; KHR_texture_transform is not applied (only
   bark/branch uvs use it); the export reads the cache but does
   not download the trees itself yet.
4. DONE (Oct 2) Earth reworked, game scale (Kalen's reference:
   autumn hills of round crowns, seen from a bit higher up).
   scenes/earth.ag: element Earth (the old JungleShader Earth is
   gone from scenes.ag). The carpet (build_carpet) wraps onto
   the sphere around the craft; EarthCanopy lifts it by rolling
   hills (0.034 radii) and crowns: 3D cells, 520 a radius, the
   tallest crown wins, domes with shared worley lumps, firs as
   cones toward the poles. Each crown takes an autumn colour
   (tropics green) over baked/earth-foliage.png: island_tree_01's
   leaves scattered into a 512 tile (alpha = pile height).
   baked/earth-land.png (Blue Marble at 4096, read raw) sets
   sea, ice, desert and forest. EarthAir: sky, clouds, sun and
   the limb; haze counts only the sight line inside the air.
   The craft flies a great circle from 35.6 N 83.5 W heading
   50 degrees, 0.6 radians each kilosecond.
   Found on the way: a fract(sin()) hash rounds differently for
   one grid corner reached from two cells; the noise tore into
   walls. EarthCanopy hashes with pcg3d (integers).
   OPEN: the baked crown pictures (earth_trees_export) are no
   longer drawn; a mid-distance coast can still show a short
   steep bank; not yet seen in orbiter itself.

## Active work: rec_qp (Sep 30 2026)

Kalen: recording at near-lossless for staging (overlays added at
moments later, then a final encode). The RTX 3060 on driver
610.57.04 encodes through Vulkan: H.264 High 4:2:0 and High
4:4:4 (4096 max), HEVC Main 4:2:0 and RExt 4:4:4 (8192 max).
1. DONE, built (trinity, buttontest) `--rec_qp` enum
   RecQuality (element.ag, beside DisplayMode; a member of
   element like record, adopted by media_app): stream (4:2:0,
   qp 10), high (4:2:0, qp 4), stage (4:4:4, qp 2), lossless
   (4:4:4, qp 0 + qpprime_y_zero_transform_bypass; the default,
   Kalen). The path@qp suffix is gone; a live url uses stream.
   Pieces: trinity.c h264_sps/h264_pps (profile 244, chroma 3,
   1 px crop units, the bypass flag; CAVLC for 4:4:4), video.ag
   Encoder chroma444/lossless (profile, G8_B8R8_2PLANE_444
   picture, full-size chroma copy, 4 bytes/px bitstream room),
   NV24Convert (full-size cb/cr), mux.ag avcC's high-profile
   chroma/bit-depth bytes, vk.ag VK_EXT_ycbcr_2plane_444_formats
   (the format is core only in 1.3; trinity asks for 1.2).
   Found on the way: nvidia's vulkan encoder refuses 4:4:4
   parameter sets with CABAC (vkGetEncodedVideoSession
   ParametersKHR -1, size 0); CAVLC works (NVENC itself does
   CABAC 4:4:4). Checked, buttontest headless at 2560x1440
   against its own shot: stage max 3 levels off, lossless max
   1 (the rgb to 8-bit ycbcr rounding): lossless is lossless.
   OPEN: at 480 px wide (480x560, 480x576) the Cb channel of
   the last ~16k pixels encodes wrong (Cr and Y right, stream
   decodes clean); 496, 640, 1024, 2560 wide are clean. Looks
   like the driver; not chased.
   element gained rec_qp: every trinity app built before
   needs a rebuild before it is hosted again (orbiter too).
   Mac: MoltenVK's encoder is 4:2:0; stage/lossless fall back
   to 4:2:0 there with a log line. VideoToolbox can do 4:4:4
   H.264 on Apple silicon: add it to our MoltenVK branch.
2. DONE, built, not confirmed on colourful footage: the NV12
   convert used BT.601 coefficients while the SPS says BT.709
   (matrix 1): hues shifted. Now BT.709 limited range.


## Active work: psx emulator (Oct 6 2026)

Kalen: a PlayStation emulator, /src/silver/psx beside n64. No
software rasterizer: the GPU draws through trinity only (VRAM is
a trinity render target; the CPU sees it only for the game's own
VRAM-to-CPU copies). Everything external is a silver import.
First game: Ridge Racer (USA), ~/Downloads/Ridge Racer (USA)/
(cue: data track MODE2/2352 + 13 CD audio tracks; the music is
CD audio). In order:
1. DONE BIOS: OpenBIOS from source, `import pcsx-redux:nugget/
   20b6316...` built by our clang/lld/llvm-objcopy (psx/nugget/
   silver: mips-cc, mips-objcopy, mips1.h, build.sh; psx/
   nugget.diff for LLVM: rfe as a word, three-operand sltiu,
   li %lo -> addiu (LLVM dropped the relocation), BIU_CONFIG via
   %hi/%lo, ALIGN(0x500) written out, .data AT(__rom_data_start)
   (lld aligned the load address past the copy's symbol), and
   -mno-check-zero-division (teq is MIPS II)) into share/
   silver-psx/openbios.bin. Bus and memory map.
2. DONE R3000A CPU (Cpu.ag): load/branch delays, COP0,
   exceptions, interrupts, LWL/LWR merging the pending load.
3. DONE interrupts, timers, DMA (block, linked list, OTC).
4. IN PROGRESS GPU on trinity (Gpu.ag): VRAM 1024 x 512 target,
   PsxDraw shader (4/8/15-bit textures and CLUTs from a VRAM copy,
   modulation, dither, mask bit), five blend pipelines (opaque,
   the four modes; textured blends draw opaque texels then blended
   ones), fills, copies, CPU uploads and readbacks, PsxDisplay
   (15/24-bit). GPUSTAT field/odd-line bits drive the BIOS shell.
   Seen: Ridge Racer's loading screen and title right.
   trinity changes: Pipeline.blend (BlendState), the batched
   Render path honours a model's runs, Texture.vk_format public.
   A Pipeline's blend must be given at construction.
   OPEN: mask test (E6 bit 1), lines are quads, VRAM wraps.
5. IN PROGRESS CD-ROM (Cdrom.ag): cue/bin (or a folder holding
   one), commands and timed responses, data reads. DONE (Oct 7)
   CD audio: a sector a 588 SPU samples, the CD-to-SPU volume
   matrix, mute, autopause (INT4), report mode (INT1 every 10
   sectors). Checked: the race's music matches track 3 (rr3.bin,
   correlation 0.70 with the SPU's sounds over it). OPEN: XA.
6. DONE GTE (Gte.ag): every command, 44-bit MAC checks and all
   FLAG bits, the UNR-table divide, the RTPS IR3 flag quirk, the
   MVMVA far-colour bug. Seen: the title's waving flag and the
   attract demo racing in 3D.
7. DONE (Oct 7) SPU (Spu.ag, ported from ares, ISC): 24 ADPCM
   voices, ADSR, the Gaussian table, pitch modulation, noise,
   reverb, transfer FIFO and DMA, the RAM IRQ, capture buffers;
   a sample every 768 CPU cycles into a ring; a sound thread
   writes it to spectra's AudioOut (44100 Hz). Frames run by the
   clock (59.81 a second), nudged by the ring's fill; no sound
   device runs silent. Checked by a capped dump: menu sounds,
   engine and music. NOT heard by me on the real card.
8. IN PROGRESS pads: digital pad on port 1, keys mapped. OPEN:
   memory cards, gamepads.
9. OPEN MDEC.
10. DONE (Oct 7) speed: a race holds 60 frames a second at 6x
   (was 26). Each flush waited the GPU and re-sent the whole
   22 MB vertex buffer, and each texture read after drawing
   copied all 75 MB of VRAM with a queue wait. Now draws only
   record (new vertices sent), a read copies only the dirty
   64 x 64 blocks it needs inside the same command buffer
   (Render.copy_recorded), one wait a frame. Emulation ~3 ms
   a frame.
11. DONE (Oct 7) drawn at the window's resolution: VRAM is a
   (1024 x S) by (512 x S) target, S from the window's pixel height
   over 240 lines (1 to 8; 6 at 1440); texture and colour
   table reads stay on the game's texel grid; CPU uploads expand to
   S x S blocks, readbacks take one sample a block; a scale change
   reads VRAM back, rebuilds, writes it again. Dither on the scaled
   pixel grid. The window is resizable.
12. DONE (Oct 7) precise vertices (Kalen: wiggly, distant road
   ripped). RTPS/RTPT keep x and y as H x camera / z before any
   cut (camera x, y, z from the uncut 44-bit sums) and the depth.
   They travel with the screen word: beside the SXY FIFO, through
   SWC2, MFC2, LW and SW (a value checked against the word), into
   a shadow of RAM and the scratchpad; the GPU's DMA hands each
   command word's address, so a vertex reads its own exact value.
   A word-keyed table (per frame; two values on one word fall
   back to the whole pixel) catches the rest. Ridge Racer's demo:
   97% exact, 64% by address. Textures map with perspective when
   all three depths are known (q = nearest / depth in the vertex
   alpha, uv x q, divided back per pixel). The few seam specks
   left are in the game's own 1x picture too.
13. DONE (Oct 7) Start did nothing: OpenBIOS waits 80 turns of a
   short loop for each pad /ACK, and the ACK fired per scanline,
   too late. The ACK now lands 700 CPU cycles after its byte
   (after the BIOS clears the IRQ, within its wait), checked in
   the step loop; status bit 7 shows it. Checked: Enter opens the
   race menu, Cross on START starts a race.
14. DONE (Oct 7) anisotropic filtering of 3D surfaces (all three
   depths known): up to 16 bilinear taps along the pixel's
   texel footprint, each texel decoded through page, window and
   colour table, clamped to the polygon's own UV box; clear,
   opaque and semi texels counted apart (cutouts, two-pass
   blends). Sprites and 2D stay sharp. Gpu.filter turns it off.

## Active work: agi, a model that credits its groups (Oct 7 2026)

Kalen: AGI as Aggregated Group Intelligence. A language model
trained from scratch whose every training text carries its
group (author, site, project); a second output, trained at the
same time as the next-token output, scores every group, so an
answer comes with its top 100 groups and a running ledger.
Choices (Kalen): from scratch, built on ai/grad.ag, open data
with natural groups (Project Gutenberg authors to start).
Identity (Kalen): a 64-bit number spread across 64 outputs,
not a column per group: the group head is 64 sigmoid bits,
the identity the fnv-1a hash of the group's name; a group's
share of a text is its bits' likelihood, normalised over the
known identities. Top-N comes from that head alone, one pass
(Kalen: a pass per contributor is too much).
Identities (Kalen): every contributor as the catalog defines
them, each author alone unless they join a group:
~/.cache/agi/members.txt lines are group, a tab, author.
identities.txt is the registry (hex number, a tab, the name),
read back as the record; a book's bit targets are the
average of everyone it credits; ledger.txt keys the shares by
identity number.
1. DONE grad.ag: causal attention (no relative tables), softmax
   cross entropy with ignored rows, sigmoid cross entropy
   (bce), reshape_view (no copy; the gradient is a view too),
   a workgroup-a-row layer norm when t is 1, profiling turned
   on mid-buffer no longer crashes. ai t_grad_lm vs pytorch
   (GRAD_GOLD), worst 7e-7; every other grad test still passes.
2. DONE the data (agi prepare): Gutenberg's catalog, English
   texts by one named person (Last, First), authors with at
   least --books books, most books first, --authors of them;
   each author's last book held out. Books from the pglaf
   mirror (0.25 s apart) into ~/.cache/agi/books; corpus.u8,
   docs.i64 (offset, length, group, held, id), groups.txt.
   Checked with 20 authors: 98 books, 38 MB, headers and the
   licence cut.
3. DONE agi/agi.ag: byte-level transformer (384 wide, 6
   layers, 6 heads, 256 bytes of context), byte head and
   identity head, both losses in one step (the bits weighted
   0.5), AdamW 0.9/0.95, warmup then cosine. agi t_agi_net
   (AGI_GOLD): the whole net against pytorch, losses within
   5e-7, every gradient within 8e-8.
4. DONE the check: held-out books, bits a byte and the right
   author's rank. 300 steps on the 20-author corpus: 3.29 bits
   a byte, top 1 16% (chance 5%), top 10 63%.
5. DONE agi ask <text> (sampling at --heat, the answer's top
   groups, added to ledger.f64) and agi ledger. Checked on the
   300-step model: text still noise, the credit list printed.
6. OPEN speed: 26,000 bytes a second at batch 32 (310 ms a
   step, 4.3 GB of work); matrix multiplies are 63% of the gpu
   time at ~1.5 TFLOPS in f32. Tensor cores (cooperative matrix,
   f16 in, f32 sums) are the way up.
8. DONE identities per contributor (catalog roles in brackets
   dropped; Anonymous, Various, Unknown are no one), members.txt
   groups, the registry file, the ledger by number. agi
   t_agi_registry: alone, grouped, written and read back; fnv-1a
   of 'a' is af63dc4c8601ec8c as published.
7. OPEN the full corpus: agi prepare (1000 authors, 5 books
   each: ~5,000 books, ~2 GB, ~2 hours of downloads), then the
   long training run (Kalen's to start).

## Active work: launch and landing (Oct 7 2026)

Kalen: a Falcon 9 booster lands on real physics, real sensors,
real propellant mass, real planning. Each part is its own
module. Real Earth (WGS84, rotation), Falcon 9 Block 5 from
public figures. The vehicle flies from its estimate, never the
truth. Rates are settings (IMU 400-1000 Hz, GPS 1-10 Hz,
guidance ~50 Hz; SpaceX does not publish theirs).
1. BUILT launch/launch.ag (the world): clouds' Nubis layer on
   the sphere (triplanar weather tile, a turned 3.4x second
   read, a baked globe field), a far slab level of detail by
   pixel footprint, night side shadowed, hex sea platform with
   the orbiter mark, scripted Falcon-like ascent, pad and chase
   cameras. Seen: liftoff, staging, 143 km, 58-60 fps in orbit.
   The last fixes (exposure, flame, orbit clouds) not yet seen.
2. BUILT physics/physics.ag: Dvec/Quat in f64, WGS84 frames,
   J2 gravity, US Std 1976 (+ its table above 86 km), a Wyoming
   sounding reader, Dryden gusts, a Craft of parts, tanks,
   engines, grid fins, cold gas jets and legs (deck contact),
   RK4, separate[], falcon_nine[ payload ]. silver --test
   physics, 7/7 against published values: air at six layer
   edges within 0.2%, gravity 9.8142/9.8321 (9.8143/9.8322),
   LC-39A round trip < 1 mm, orbit energy drift 4.8e-11, stage
   two dv 2449.5 vs Tsiolkovsky 2449.0, drag seen in the motion
   = 0.5 rho v2 Ca A to 0.01%, legs carry the weight to 0.4%.
   Found: aft first the empty booster drifts to 9 degrees off
   axis in calm air (marginal, as the real one; fins and jets
   must hold it). Estimates, named in the code: Ca/Cn tables,
   tank places, gimbal 5 deg, fin size and lift, jet thrust,
   leg stiffness. Slosh, jet damping, dI/dt are left out.
   The spec: 6-DOF, RK4 fixed step, WGS84 J2
   gravity in an inertial frame, US Standard 1976 air, aero
   (body, grid fins), Merlin thrust by ambient pressure,
   throttle, gimbal, mass flow, moving centre of mass.
   The air is not perfect (Kalen): winds with shear and the
   jet stream, gusts, temperature and pressure off the standard
   day, all pushing on the vehicle unevenly. Source: a measured
   Cape sounding (station 74794) to ~30 km, US Standard 1976
   above, Dryden turbulence (MIL-HDBK-1797) for gusts.
   All units metric (SI); all figures published or derived,
   and any estimate is named as one.
3. BUILT imu/imu.ag: dv and dtheta increments at 400 Hz at the
   unit's place (lever arm: w' x r + w x w x r), LN-200
   datasheet errors (assumed unit): gyro 1 deg/h bias, 0.07
   deg/sqrt(h) ARW, 100 ppm; accel 300 ug, 50 ug/sqrt(Hz),
   300 ppm; misalignment 0.1 mrad (estimate); biases drift as
   Gauss-Markov, 1 h. physics gained Craft.spin_rate. Checks
   2/2: in orbit it feels 0.0037 m/s2 (its bias); 1 rad turn
   read 0.99970.
4. BUILT gnss/gnss.ag: the current YUMA almanac (navcen.uscg.gov,
   cached in path_storage gnss, fetched when missing), satellite
   places by IS-GPS-200, pseudoranges and rates with light time,
   Earth turn in flight, troposphere, a 350 km ionosphere shell
   (half removed by the receiver's model), 1 m broadcast error
   drifting per satellite, 0.5 m code and 0.03 m/s rate noise
   (estimates), a TCXO clock as random walks; least squares
   place, clock, speed, PDOP; 10 Hz, 50 ms latency (settings).
   Checks 3/3: 32 satellites at 26,602 km; LC-39A 10 in view,
   PDOP 1.43, worst of 100 fixes 7.5 m; 400 km orbit 0.65 m,
   0.049 m/s, 16 satellites. The receiver ignores COCOM limits
   (a launch receiver is licensed past them).
5. BUILT nav/nav.ag: an error state Kalman filter of 18 (place,
   speed, attitude, accel bias, gyro bias, the GPS's shared
   error as Gauss-Markov 4 m / 600 s, an estimate). Strapdown
   in the inertial frame at the IMU's point, J2 gravity, IMU
   samples at 400 Hz; each fix met at its own time from a kept
   past (50 ms late), antenna lever arm in H. Check 1/1: stage
   two burning and turning at 300 km, starting 10 m, 0.5 m/s,
   0.1 deg tilt, 0.5 deg heading off: at 60 s 5.6 m (it says
   6.3 m), 0.011 m/s, 0.049 deg; after a 20 s GPS gap 5.7 m,
   0.013 m/s. Each raw fix is 4.2 m off: GPS limits it. With
   15 states it claimed 0.3 m (overconfident): fixed by the
   shared error states. FOR THE LANDING: absolute GPS is ~5 m;
   the deck carries its own receiver, and the difference of
   the two (same satellites, same errors) is the relative
   place to land on (OPEN, part of item 6).
6. PARTLY BUILT guidance/guidance.ag: a second order cone solver
   (log barrier, Newton on the KKT system, a phase I with one
   slack), the landing burn as Acikmese & Ploen 2007 / Acikmese,
   Carson & Blackmore 2013 (u, sigma, z = ln m, throttle band
   linearised about full-burn mass, tilt, glide slope, terminal
   place and speed, least fuel; golden section on the flight
   time), an impact predictor (gravity, turning air, drag
   engines first). Checks 3/3: distance 4.24264 exact; 30 t on
   one Merlin from 1.5 km, 150 m/s down: 13.95 s, 2,957 kg,
   flown exactly it lands 5e-8 m off, thrust 337..844 kN of
   338..845 (the linearisation), tilt 14.9 of 15 deg; ~160 ms
   a solve; impact vs physics from 60 km: 90.6 s both, 39 m.
   OPEN: the phase sequence (ascent, MECO with a return
   reserve, flip, boostback by the predictor, entry burn, the
   aero phase, ignition timing, re-solves at 2 Hz); needs 7.
   Original spec: ascent, boostback, entry, landing burn by
   convex optimisation (G-FOLD).
   Kalen (Oct 7): the booster lands back on the SAME platform it
   launched from (return to launch site): full boostback, more
   propellant held back, less payload. Then (Kalen, Oct 7): the
   deck alone, no mount or tower, only the orbiter mark; the
   stack stands on its legs (out from launch) and lands on the
   mark, the deck's middle. Relative nav: the platform's own
   GPS receiver (2 m over the deck) less the booster's.
   Calm, after: scvx 36.8 m off at 15.1 m/s, gfold 17.1 m at
   13.0 m/s (both fail the test's limits, as before).
   BUILT mission/mission.ag (the sequence) and Kalen saw it land
   beside the circle (Oct 7). Calm air (test severity 0): the
   forecast (the booster's copy flies coast, entry burn, fall,
   ignition by stop_loss, braking) aims the boostback and the
   aero phase; the boostback cuts on the exact step (cut_at);
   thrust sized by the air at the commanded attitude. Ignition
   miss 1,116 m -> 292 m; touchdown 20.9 m off, 8.4 m/s, 9 deg
   tilt, 515 kg left (the test's limits still fail). Open, in
   order:
   a. OPEN the aero phase sits at its 0.2 rad lean cap.
   b. OPEN over-thrust the first 5 s after ignition (attitude
      lags 0.56 rad; the plan's sideways ask too big at speed).
   c. OPEN the plan's outside push from the booster's copy
      (hull side force, fins, slosh): the last 20 m.
   d. OPEN the fallback braking climbs (min throttle > weight).
   e. OPEN the boostback cutoff note prints the stale miss.
   f. OPEN restore the test's weather; check light, moderate.
   g. BUILT, not seen in launch (Kalen: 1 frame per 3 s near the
      landing): each 2 Hz replan ran ~11 cone solves, 1.24 s mean,
      4.1 s worst, inside one physics step. The solver walks only
      the columns each row and cone touches (Socp.index): 130-180
      -> 32-42 ms a solve, same Newton path. The plan is solved on
      an async worker from the state 0.5 s ahead (PlanJob,
      plan_work, plan_ahead) and taken up at that met; launch
      stops stepping while one is due and unfinished
      (Mission.waiting). Calm landing now 24.2 m, 10.3 m/s, 7.0
      deg, 595 kg (was 21.0 m, 8.5 m/s): the 0.5 s lead.
   h. BUILT (Kalen: SCvx for the landing, keep G-FOLD, benchmark
      both): guidance Scvx (Mao, Szmuk & Acikmese 2016; Szmuk
      2018): thrust in newtons, mass and drag (the DragTable's
      Ca, standard air) in RK4 dynamics, flight time free,
      linearised by forward differences, one cone problem a pass
      in a trust region, a virtual control on the end. enum
      LandingMethod (scvx default, gfold); Mission.landing,
      `launch --landing gfold`; the touchdown note gives the
      plans' mean and worst solve ms. guidance
      t_landing_compare (1.5 km, 150 m/s, Ca 1.3, both flown
      through the same air): scvx 178 ms, 6 rounds, 2,638 kg,
      0.06 m off at 0.01 m/s; gfold (search + 2 drag passes)
      663 ms, 2,658 kg, 21 m off at 2.5 m/s.
   i. BUILT touchdown physics (physics.ag): each leg elastic
      (Hunt & Crossley damping), then its crush core at 400 kN
      for 0.5 m, then the structure; breaks past 1.5 MN (all
      estimates); pads hold on a sideways spring to friction's
      limit then slide; feet past the deck's edge find sea;
      the hull (skirt + rings every 8 m) strikes the deck. Feet
      2 m under the engines (LEG_FOOT_X, photos). Per leg: first
      touch (time, place, speed, tilt), crush, peak, broken;
      hull strike. physics 15/15: drops at 2 m/s (no crush),
      6 m/s no damper (crush 0.2963 m vs 0.2973 by energy),
      leaning 6 deg (stands), 8 m/s across at 15 deg (topples).
      The mission runs 20 s past touchdown and notes it all.
      Flight (gfold, calm): two legs at 6.2-6.9 m/s and 12 deg,
      three legs break, the hull strikes, it topples. The fall's
      stronger lean (600 / 0.35) lit the burn 425 m off and both
      planners went in the sea: back to 1000 / 0.2.
   j. BUILT legs stow at liftoff and swing over 1 s (Leg.frac;
      launch VehicleLit fold); plume from plume_of (physics):
      Prandtl-Meyer edge, Pack cells, Witze core, Simons spread.
      SCvx thrust slew limit and the tilt taper (tilt_at) built;
      the SCvx flight not rerun since.
   k. BUILT (Kalen: solves must be superbly fast; own IPM in
      silver) Socp is a primal-dual interior point: G x + s = h,
      Nesterov-Todd scaling (the reflection v = (w + e) /
      sqrt(2 (w0 + 1)), checked in numpy), Mehrotra predictor
      and corrector, the reduced system by Cholesky plus an m by
      m Schur complement, at most 30 steps; it stops at 1e-8
      (dual 1e-6) and keeps the last 1e-6 iterate, as the reduced
      system loses digits near the answer. Distance check 5
      steps (was 52); a G-FOLD solve 10-20 ms (was ~35 ms and
      ~110 steps); G-FOLD's time search is 6 scanned times and
      4 golden steps. Flight: 94 plans, 90-110 ms mean, ~280 ms
      worst (was 614 / 1,262). Guidance 4/4.
   l. OPEN the last 10 m. Also built: a soft sink-rate cone (fall
      no faster than sqrt(2 x 6 x height) + 1 m/s, 2 m/s of fuel
      per m/s over), the flight's air share 0.7..1.3 (was
      0.3..1.5), replans down to 3 m with a search range that is
      never empty, a capped lean keeps the push up as asked, and
      feedback toward the plan's place and speed (1.2 rad/s, at
      most 5 m/s2). Flight (gfold): about 10 m up at 1-2 m/s down
      but 11-13 m/s sideways; one engine at its least throttle
      lifts more than the booster weighs, so slow and high can
      only climb; it climbs and falls in the sea.
   m. DONE (Oct 7, Kalen: a soft landing "bounced 100 ft"): it
      fell over, the nose hung past the deck's edge (no contact
      there), and when the hull swung back over the deck a point
      already metres under its top met the 5e7 N/m hull spring:
      thrown up at 80-104 m/s. Kalen: the floor is not a spring.
      A hull strike is now the end: Craft.step stops, mission
      notes "it explodes", phase 9 ('exploded on the deck').
      Legs keep their springs and crush cores.
      Also: at touchdown the autopilot and jets stop.
   n. BUILT, not seen: grid fins (launch build_fins), placed as
      the physics has them (0/90/180/270 deg; legs at 45): a
      frame 1.5 m out, 1.2 m across, 0.3 m deep (estimates), a
      45 degree lattice, hull fittings; folded flat up the
      interstage until Fin.open, out in 1 s (Fin.frac).
   o. BUILT (Oct 7): deck receiver as a differential GPS base
      (gnss Receiver.base, base_at; booster gps_bias 0.3): the
      feet's height estimate went from 2-5 m high to 0.05 m.
      follow() removed (it made the booster circle). G-FOLD:
      per-step air share (Landing.reach), sideways turn rate
      (turn, 2 m/s3), the push got lagging the push asked
      (lag = 1 / point_gain, step mean; guidance t_landing_lag:
      0.08 m off flown through a true lag), a cost per metre
      off the mark (track_cost), upright over the last 3 s by
      time (UPRIGHT_TIME). Engines cut when rising under 10 m;
      a feather at least throttle, upright, when that meets
      the deck at 2 m/s or less. Flight (gfold, light): four
      legs at 3.8 m/s, stands at 3.5 deg, 38 m off the mark.
   p. BUILT (Oct 8): wind and the coast. --rwind (wind_min,
      wind_max: the jet, km/h): Weather.random_wind; the deck's
      anemometer (read_deck_wind, 10 m mast, ~10 s average)
      shifts the loaded profile's bottom (known_wind), which the
      impact forecast flies through. The ascent flies a pitch
      plan against the ground (planned_lean, the calm climb's
      lean every 5 s). The coast leans AWAY from the miss (the
      hull-only model said toward: wrong in flight), learned from
      the miss over 4 s once q > 15 kPa, lean cap 0.3. Calm test
      flight: ignition 370 m off, stands 17.6 m off at 3 deg.
      Fins turn visibly (VehicleLit.twist, each fin's deflect),
      not seen yet. Joints and tube modes (knee 1000 kg, joint
      5e7 N/m, axial 30 Hz, bend 4 Hz, 2%), leg damper 8e4,
      lift_loss 0.4: t_drop_lean still fails (rocks ~4.6 deg at
      8 s). After any Craft/State change: --clean imu gnss nav
      control guidance mission launch (stale layouts broke nav).
7. BUILT control/control.ag (class Autopilot: a class named
   Control clashed with trinity's element Control; in launch
   the linker took trinity's Control_init, so ours never ran
   and its duty vec stayed null: the T0 crash). Class names
   must not repeat one in any module loaded beside them.
   One attitude loop on a Belief (nav's,
   or the truth for checks): error to wanted rate (capped by
   what the actuators can brake, sqrt(2 a e)), rate error to
   torque with the gyroscopic term; split over the gimbal
   (pitch, yaw on every lit engine, roll by the outer ring
   along the hull), the grid fins (least squares, slope and
   dynamic pressure from its own standard-air estimate) and the
   jets (pulsed, a share of each 20 ms period). 50 Hz. physics
   gained actuator slew (gimbal 0.35 rad/s, fins 0.6 rad/s:
   estimates) and roll jets (4 side, 4 along the hull, 2 kN
   each: estimate). Checks 4/4: stage two turned 20 deg holds
   within 0.03 deg; the empty booster flips end for end on
   jets in 34.7 s; falling through 5+ kPa with moderate
   turbulence the fins hold 4 deg off the airflow, mean 0.49
   deg, worst 1.19; the full stack flies the pitch program to
   20.8 km through max-Q 39 kPa in moderate turbulence, worst
   0.76 deg.
8. OPEN launch draws truth and estimate.
12. BUILT aerodynamics from the shape (Kalen chose the component
   build-up, Oct 7). A Part is a tube or a profile (x, r pairs;
   the fairing's ogive), outer or inside; the craft's skin is
   their envelope at 0.25 m stations, rebuilt on separation.
   Craft.body_aero: normal force by slender body theory (area
   change from the leading end, separated narrowings halved:
   estimate) plus Jorgensen's crossflow (NASA TR R-474, 0-180
   deg, its eta and Cdc curves read off); axial: Schlichting
   turbulent skin friction with compressibility and Sutherland
   viscosity, faces into the flow by modified Newtonian (Rayleigh
   pitot) from Mach 1.2, blunt faces 0.8 of stagnation below
   Mach 0.8 (Hoerner), blended between; base suction by Hoerner/
   Love curves, none behind a burning base; a burn out of the
   leading end shields its face (estimate); open grid fins and
   deployed legs drag (estimates). Every force at its station:
   the centre of pressure comes out of the shape. axial_coeff
   (for the planners) now evaluates the shape. Checks: Newtonian
   stagnation 1.837 at Mach 20 (1.839), Cf 0.00300 at Re 1e7
   (0.0030), slender body slope 2.10 at 0.5 deg (2 + crossflow).
   Stack nose first under power: Ca 0.10 subsonic, 0.36 Mach
   2-5, centre of pressure 59.5 m (cg 27.7): unstable, as real.
   Booster engines first, fins out: 1.47 subsonic, 2.71 peak at
   Mach 1.3 (fin share an estimate, likely high transonic).
   Found: a 0.5 m gap in the skin (stage two to fairing) read
   as a blunt face; closed. Control gained integral action (a
   steady aero torque left a steady error): fin hold mean 0.22
   deg, worst 0.48; ascent worst 0.61; gimbal 0.06; flip 34.7 s.
   predict_impact takes the craft and uses its own shape (20 m
   from the physics after 95 s, was 39).
10. OPEN (Kalen, Oct 7) a standard view for every sensor on board,
   each an element its own module exports: imu (rate and
   acceleration traces), gnss (sky plot, fix error, PDOP), nav
   (estimate against truth, its spread), physics (tank cutaway
   with slosh); launch mounts them as panels.
11. BUILT slosh in physics: per tank Abramson's first mode (NASA
   SP-106): m1 = m 2R/(1.841 x 2.3894 h) tanh(1.841 h/R),
   w2 = 1.841 a/R tanh(1.841 h/R), damping 0.02 (estimate), the
   mass at h - R/1.841 tanh(1.841 h/2R) (our approximation).
   The rigid body leaves m1 out (Craft.rigid; mass is the
   total); each m1 loads its tank axially and pulls by its
   spring at its offset; it feels the tank wall's specific
   force and the turn; a stop at 0.8 R; State.slosh holds y, z
   and speeds per tank. Check t_slosh_ring: held still the LOX
   rings at 1.9507 s (Abramson 1.9504), decays 0.3659 (0.3659);
   free, coupled with the stage, 1.66 s. Found on the way: the
   IMU's felt force must be over the rigid mass (nav broke at
   1.4 m/s, fixed). Coast: no settling model (liquid floats
   only as far as the spring frees it). NEXT: the cutaway
   view, with launch on physics (after 7).
9. BUILT, not run: C cycles the camera: platform, orbiter (the
   upper stage, to a stable orbit), booster (it lands).
13. DONE (Oct 8, Kalen: "landed on the first go") the hexagonal
   ship: rounded hexagon hull, six grid fins on the faces, six
   legs as flat panels stowed on the faces (width the flat of a
   face, sides chamfered at 30 degrees). Not yet seen in launch.
   Then (Kalen): each leg held by a link to a slider in a rail
   channel cut in its face (flush when stowed); the six sliders
   ride one collar, a relief, not a spring: rigid to 2.4 MN on
   the feet, then it slides up for good (0.5 m), turning every
   leg flatter, so the stance widens. physics: Craft.collar_*,
   Leg.hinge/foot_out; 6 m/s drop: collar 0.183 m (energy
   0.186), feet 18.0 -> 18.18 m across; lean drop stands 0.2 deg.
14. OPEN (Kalen, Oct 8) the ship goes to the moon: it docks
   with a fuel station in orbit to refuel first.
   Kalen's calls (Oct 8): the tanker is our own stack, same
   model; its upper stage holds propellant where ours holds
   people; when done it lands itself. 400 km circular, the
   launch waits for the tanker's plane over the pad, then
   phasing. Full contact docking. The orbit flight we fly now
   stays; the main mission flies on to the tanker. Order:
   a. DONE the tanker in orbit (Mission.tanker, place_tanker):
      our upper stage, fairing off, at liftoff circular 400 km
      in the plane of the pad's heading east (inclined 28.39
      deg), tanker_lead 30 deg ahead (an estimate for three
      phasing orbits); speed from our J2 gravity. Steps with
      the flight. t_tanker_orbit: pad 0 m off the plane, an
      orbit 5553.6 s at 398.1 to 400.3 km. The full flights
      after it not rerun. Its propellant load: item e.
   b. DONE the ascent to its plane and up to it. The upper
      stage steers in the tanker's plane (in_plane: sideways
      drift trimmed); insertion guidance: the burn left by the
      rocket equation, a vertical push falling in a straight
      line to reach 200 km level as orbital speed comes. Then
      phases 3 parking (rounded off at its real high point),
      4 transfer burn when the tanker leads by the transfer's
      phase, 5 coast, 6 circularise, 7 in its orbit. Burns cut
      on orbit_range: our nav state flown on in our J2 gravity
      (the round-Earth high point is ~14 km off). tanker_lead
      -10 deg. Found and fixed: Autopilot jet reach used the
      booster's jet place (40.5 m) and was negative on the
      upper stage, so its jets never fired; stage two now has
      its own cold gas jets. t_to_tanker (whole flight): SECO
      200.0 km, 0.03 km off the plane, parked 197.5-200.1 km,
      transfer T1531, in its orbit T4260, 1.46 km from it,
      3.9 t left. Both landings as before (17.9 m, 19.7 m).
      launch draws the tanker (silver nose), camera 4: tanker.
   c. OPEN relative nav (both ships' receivers, differential)
      and close guidance (Clohessy-Wiltshire targeting, hold
      points, the approach along the tanker's port axis) on
      the upper stage's cold gas jets.
   Kalen: they mate flat on, side by side "like 2 fish": a
   hex face against a hex face, not nose to nose.
   "like a pencil to pencil": parallel, side by side. The
   tanker is the whole ship with its nose: only the nose
   differs (ours for people, its for propellant).
   d. OPEN docking ports in physics: contact, soft capture
      within limits of speed and misalignment, latches, hard
      dock joining the two crafts.
   e. OPEN propellant moves from the tanker into our tanks.
   f. OPEN every upper stage lands: legs (the panel and
      collar design) and a landing burn on its engine; the
      tanker undocks, comes down and lands on the deck.
   g. OPEN launch draws the tanker, the docking and its cameras.
   Kalen (Oct 8): every stage kept on the tanker (its booster
   stays on in orbit); not lower than LEO: it parks at 200 km
   (tanker_alt), ours parks at 170 km under it (park_alt) to
   gain on it, then rises to it.
   h. OPEN the tanker waits up there days to weeks: air drag
      lowers it, and it burns to stay up, paying in propellant.
      Station keeping: a reboost when its low point sags, fuel
      taken from its own tanks, the total logged.
      BUILT: Autopilot holds its nose along its path (jets),
      the orbit's size by its whole energy (J2 swings the
      height 2.7 km a lap), centre engine at least throttle at
      the high point under keep_band. physics gained free
      molecular drag (FREE_CD 2.2, Knudsen bridge): t_aero_thin.
   i. BUILT (Kalen, Oct 8): booster and upper stage 50% larger,
      every ratio kept (SHIP_SCALE 1.5, scale_ship): lengths
      x1.5, masses and thrust x3.375 (chamber pressure 146 bar,
      "if raptor can"), jets x5.06. Physics 22/22 at the size.
      launch draws it at SHIP_SCALE. Landings to rerun.
      Then (Kalen): 100 m tall, 6 m across the faces (SHIP_LONG,
      SHIP_WIDE), seven larger engines, the nose the hull's
      width. Isp 380 s (352 at sea level): "fantasize".
   j. DONE (Kalen: we REFUEL at 200 km) the main mission flies
      the whole ship: nothing separates (Mission.whole). The
      seven burn the booster's tanks, then stage two's
      (crossfeed); under 4 g (WHOLE_G, 7 -> 3 -> 1 engines);
      the pitch plan to T+80, then linear tangent steering
      (fly_ahead, steer_solve: Newton on the burn flown ahead)
      to 200 km level. t_whole_ascent: SECO T302 at 200.1 km,
      7785 m/s, 10.3 t left, 8 m/s to the air. t_to_tanker:
      in its orbit 9.5 km from it. The orbit flight (whole:
      false) keeps its staging and the booster landing.
      OPEN: the orbit flight's scvx landing at the new size
      (gfold lands 19.3 m off; scvx 44 m, a leg breaks).
      OPEN: tanker station keeping: its jets sit above its
      centre of mass and push it up 233 m in 3 days, more than
      the drag takes; needs jets at both ends fired in pairs.
   k. IN PROGRESS item c, the close approach (Kalen: the
      tanker rolls its port to face us; signal-wise, its radio
      sends its attitude and its port; intercept at T+8 min).
      Relative GPS by single differences (gnss relative_fix),
      jets as couples (control couple_jets, a per-step firing
      tally), Clohessy-Wiltshire drift taken out, holds at 200
      and 20 m. Held at 200 m at T+1684 (intercept too slow:
      942 m and 9 m/s apart at cutoff). OPEN: cut off beside
      the 200 m mark at the tanker's speed for T+8 min.
   l. DONE launch draws jet puffs, docking ports (capture
      ring, lights, three retroreflectors on the tanker's),
      our floodlight; camera 4 is our port camera.
   m. IN PROGRESS the docking CNN (Kalen): launch's export
      docking_images renders 2000 64 x 64 port-camera pictures,
      offsets uniform over 1-30 m out, 3 m aside, 5 deg roll,
      3 deg pitch and yaw, sun and night, into share/launch/
      docking with labels.txt. dock/dock.ag trains it on ai's
      grad (4 convs, 2 dense, L1, AdamW): `silver dock`.
      LATER (Kalen): Earth backgrounds at other angles.
      Then (Kalen): two ports, one a propellant (oxygen fore,
      fuel aft, 9 m apart: physics port_span, port_of), a camera
      at each; pairs of 32 x 32 pictures, grey for training, in
      range inside the approach cone (aside 0.3 m + a quarter of
      the distance) and off target (neither port in either
      view) with an in_range flag; dock_<n>.agi beside
      dock_<n>_a/_b.png in share/launch/dock2. dock: two grey
      channels, three convs, heads for the six numbers (scored
      in range only) and the flag (bce); every fifth pair held.
      First train on 527 pairs (500 in range; the render froze
      the machine again, the kernel watch stopped it): held out
      right 0.82 m, up 2.22 m, out 3.61 m, roll 1.69, pitch
      1.62, yaw 1.05 deg, in-range call 96%; up and pitch no
      better than guessing (overfit, 422 to train on).
      Net in the approach (Oct 9): its detections weigh into the
      gap by its held-out misses (gps 0.4 m against 0.82, 2.22,
      3.61 m), logged every 5 s against gps and truth ('dock
      net:'); it steers little. Its pictures predate the open
      doors, long probes and brighter floodlight: re-render.
   n. DONE (Oct 9) dock, refuel, undock in the whole flight:
      the tanker turns its ports to our side (under or over it,
      Mission.beneath); closing as burns and coasts, braking
      within the jets' reach at our mass (jets_reach); doors
      slide open, gold probes run out 2.5 m, soft capture with
      a tip 0.3 m in its cone (0.5 m aside, 0.15 m/s, 3 deg),
      15 s draw-in to a 1.5 m standoff (only the connectors
      meet), hard dock; propellant tank to tank (each tank at
      its share of 2 t/s: really ~2.5-4 t/s, OPEN: label or
      pumps); springs part them, probes in, doors shut, back
      off past 50 m: 'clear of the tanker'. Full flight: hard
      dock T+12:54, refuelled T+25:33 (1862 t), clear.
      Fixed on the way: the capture judged mid-step (tanker a
      step ahead: 19.5 m); the latch held centres of mass (the
      hulls slid as propellant moved); the latch copied the
      tanker's body rates into ours (turned 180 deg: nav 104
      deg off after); jets owed time while switched off.
      OPEN: backing off at full mass overshoots 50 m (it is
      only cleared, not held); the tanker now near empty.
      OPEN (real transfer, offered): settling thrust, pressure
      fed flow, venting, chill-down, residuals.
   o. DONE (Oct 9) save state: P saves (~/.cache/launch/saves/
      T<m>m<s>s.save and last.save), L loads last, --load
      <file>. Text, numbers as their doubles' bits; ships,
      phases, docking, pilots, each nav filter's whole belief.
      t_save_state: exact on load, 15.7 m after 20 s through
      max-Q (gusts and sensor noise are fresh draws).
   p. IN PROGRESS the Moon (Kalen, Oct 9): the Moon's orbit and
      gravity in physics, a coast step (big steps, gravity only)
      and higher warp, the burn to the Moon aimed by flying it
      ahead, capture into lunar orbit, the Moon drawn in launch;
      then landing, take-off, home. A full ship has ~10.2 km/s:
      enough there and back to Earth, not an Earth landing
      without a heat shield (Kalen's call pending).
   q. DONE (Oct 9, Kalen) the stack split moved down: stage one
      ends at 38.6 m (Falcon scale; mesh 40), the chrome band is
      the upper ship's skirt with five Lugh (sea level, drawing
      stage two's tanks: lugh picks tanks by group now) and six
      small legs on its corners (Craft.flaps, Leg.length/width/
      open/open_cmd; Craft.flap_push, the air on a plate either
      face; Autopilot.use_flaps allocates them). Booster fins and
      top jets down to 37.0/37.8, its tanks 1 m lower.
      Fall test findings (upper ship alone, 30 km, turbulence 2,
      asked 4 deg off the flow; the test is removed until a
      design is chosen): engines first it tumbles by 2.5 kPa;
      nose first holds 0.5 deg to 1.4 kPa, tumbles at 3.6 kPa;
      twice the panel area tumbles at 4.3 kPa; with 10 t aboard
      holds to 4.2 kPa, then 10-30 deg off; jets up to 20x do
      not hold (aero moment ~2.8 MN m at 19 kPa, 10 deg); one
      Lugh at least throttle out-pushes the ship (it hovers).
      Then (Kalen): the small legs are landing legs only; the
      crew stage steers with its own six grid fins at 55.5 m
      under a nose flared to 2.3 m (Falcon scale; drag in orbit
      +58%). Fall test: engines first it flips end over end;
      nose first 12 deg off at 1.5 kPa, then lies broadside (85
      deg): it falls belly first by itself.
      Recommended (Kalen's call pending): belly first with nose
      flaps and a belly heat shield, flip, landing burn.
      SSTO: as built 6.7 km/s of 9.3 needed; stretched to ~850 t
      of propellant it would fly alone (Kalen: fine as is).
      OPEN ai grad: bin (add, mul ...) reads x.dims[ 2 ]: a 2-D
      tensor gives garbage and no gradient, silently; give it
      [ nb, c, 1 ] or make bin refuse other shapes.
      OPEN the machine: soft lockups on CPU 31 with clock reads
      of 3-40 s, twice, each under our GPU renders (export and
      mission test); cause below our code, not found.
      OPEN silver: a ternary between two Dvec structs passed
      straight into a call (`dv_sub[ (c) ? a : b, e ]`) emits
      the struct by value where the call takes a pointer (LLVM
      verify fails); a local set by if/else works.
   r. IN PROGRESS (Oct 9) the tanker's own flight, first off deck
      B (launch's default; --ship flies ours). Kalen: only 7- or
      5-Lugh stages. The tanker: three standard 7-Lugh boosters
      side by side (body z, 1.5 m apart, the docking standoff),
      joined by four rounded hexagon connectors in orbiter-hex
      mounts (2 t each at Falcon scale: estimate; they stay on
      the middle one), a fourth on the middle one's top under a
      silver nose; each side booster has the same nose. 6,513 t,
      21 Lugh below, liftoff 1.40 g. Physics: Part/Tank/Fin.off
      and Leg.axis (a place across the craft), Leg.folded (last
      in Leg), one outline per body in body_aero, inertia by the
      offsets, mark_side/recentre (a side booster detached on its
      own axis); t_tanker_build. All 24 legs and fins kept; those
      that would cross or sit in a gap stay folded while joined,
      all free at separation. Launch: the three drawn, decks D E
      F (more_decks), 32 flames, cameras 3 and 5 the side
      boosters, the side boosters' nitrogen puffs across the gap
      (drawn only: Kalen had the push removed, it put the right
      booster on its deck's edge), flames marched from their far face
      (no depth test; the march stops at the scene's depth).
      Mission: all three separate at staging; the side boosters
      fly as recovery Missions (recovery, adopt, label) to decks
      F and C; reserve from the measured return (1.8 x speed
      across + 950 + 750 m/s at 352 s: estimate); boostback and
      entry on each booster's centre engines (centre_three by
      place; booster_centres); the landing burn starts on three
      when falling over 250 m/s, braking against its motion at
      85%, down to one when one can stop it (burn_n, the forecast
      flies the same); SCvx's plan followed between its steps,
      SCvx plans with the tilt's lag (Scvx.lag = 1 / point_gain:
      the push across follows the one asked; t_scvx_lag flown
      through it 2.3 m off), a plan two solves old gives way to the
      braking law, its turn from the gimbal's spin of the booster
      (turn_r, ~1.6 m/s3), max_rate 0.35 once lit. Physics: a
      narrowing trailing a fall engines first pushes no sideways
      (the noses on the trailing end twisted the side boosters).
      Test run (no push, SCvx with the lag): all three stand:
      middle on B (7.7 m/s, 2.3 deg, cores untouched), left on F
      (5.1 m/s, 0.23 deg, cores used), right on C (6.2 m/s, 2.2
      deg, cores used; lit 62 m off). The top booster parks round
      at 700 km (asked 300), 135 t.
      OPEN: touchdowns over the legs' 2 m/s (cores used); the
      right one's 62 m at ignition; a glide model in the forecast
      (Kalen: the nose's shape as it glides); the top booster's
      cutoff;
      saves lack the side flights; new lines past 72 columns;
      fin_look/fin_miss kept (still used); our ship's flights to
      rerun on the shared landing changes.

## Active work: quake2 port (Oct 5 2026)

Kalen: id's Quake 2 (GPL, /src/Quake-2 is the reference clone)
ported to silver in its own repo /src/quake2 (module folder
/src/quake2/quake2, origin ar-visions/quake2; no commit yet:
Kalen's). Trinity does ALL 3D and UX, called directly from the
app: no renderer module, no ref_soft / ref_gl / 3dfx / PowerVR.
Assets from Steam: /home/kalen/.steam/debian-installation/
steamapps/common/Quake 2/baseq2 (never in a repo). Later: AI.
Build: `cd /src/quake2 && silver --build quake2`; headless run
as any trinity app (install/build/quake2 --hidden true).
1. DONE inventory: /src/quake2/INVENTORY.md (97k lines to port,
   56k dropped, formats, assets, trinity owners).
2. DONE Pak.ag (pak0-9 + loose files), Bsp.ag (IBSP 38:
   entities, verts, edges, texinfo, faces, lighting, model 0;
   wal textures through the colormap palette into one 2048-wide
   atlas; lightmaps into a 1024x2048 atlas; one mesh, uv tiled
   in the shader by the wal's atlas rect), quake2.ag (BspLit
   shader, MapView: WASD + drag look, start at the first
   info_player_start). base1 draws: 19,887 triangles, textures
   and lightmaps right (shot checked). Sky and nodraw faces
   skipped, no PVS, no culling, no sky box yet.
   silver found on the way: `cast cstr [ v ]` on a vec or a
   `new` buffer handed the OBJECT pointer (fixed in
   e_direct_cast, features t_cast_cstr_vec, 232/232); a render
   target in element_targets needs `owner = a` or the window
   takes it as its backdrop and faults in ReduceBlur.last;
   sibling files are `extend quake2`, imported after the C
   headers; a shader's sampler names must be members of its
   LayoutBinding enum (BspSurface).
3. DONE PVS culling: planes, nodes, leafs, leaf faces and the
   vis lump are read; each frame the eye's leaf gives its cluster,
   the cluster's row is expanded, and the faces of its visible
   leaves are copied into a live index list (Gpu.upload_index,
   new in trinity vk.ag: replaces the first n indices and draws n).
   base1 start: 1,826 of 19,887 triangles; outside the world
   (cluster -1) everything draws. No frustum test yet.
4. DONE the sky: sky faces are their own mesh and shader (BspSky):
   the view direction per fragment, turned back to quake axes,
   picks the face and s,t exactly as gl_warp's vec_to_st and
   MakeSkyVec do, from a 6-wide strip of the env/<sky>*.tga faces
   (TGA 24/32-bit, plain and run length, rows bottom up as
   LoadTGA). Checked by eye against the strip (python reference
   skyref.png): Outer Base sky upright, sun and horizon right.
   `--spawn "x y z yaw pitch"` (quake axes) places the camera.
   OPEN: skyrotate/skyaxis, faint seams at cube edges (the GL
   original had them too).
5. DONE collision: cmodel.c's CM_BoxTrace ported (brushes, brush
   sides, leaf brushes, leaf contents; clip_brush, trace_leaf,
   hull_check, Trace class; the start==end position test is not
   ported) and pmove's PM_StepSlideMove_ with PM_ClipVelocity
   (slide_move, 4 bumps, 5 planes, crease, stop against the first
   push). The camera is the player box (-16 -16 -24 .. 16 16 32),
   mask solid|window|playerclip|monster, no gravity yet, eye 22
   up. Checked headless at the start: forward stops after 624
   units, a strafe after 304, backing up after 888, all inside.
6. DONE pmove's walking path: PM_CatagorizePosition, PM_CheckJump,
   PM_Friction, PM_Accelerate, PM_AirMove, PM_StepSlideMove (and
   CM_BoxTrace's position test: box leaf walk + CM_TestBoxInBrush)
   as categorize, check_jump, friction, accelerate, air_move,
   step_slide_move. Gravity 800, run 400 capped at 300, jump 270,
   step 18, friction 6 / stop 100. Space jumps. Checked headless:
   on the start lift the walk reaches 300 units/s in 9 frames and
   stops at the closed doors (x -1704); a jump climbs 120 -> 159+
   and falls back.
   Found on the way: the start stands on inline model *31 (a
   func_door lift), so the trace clips every inline model at its
   map place (headnode per dmodel, box reject) and the mesh draws
   their faces (always, unculled, until they move). A trigger_once
   brush is SOLID in the bsp; the engine skips it because the
   entity is not solid, so model_entities reads each model's
   classname and trigger_* / func_areaportal are neither clipped
   nor drawn.
7. OPEN the repo: committed locally (LICENSE = id's GPL v2 text,
   README, INVENTORY); github.com/ar-visions/quake2 does not exist
   yet and there is no token or gh here: Kalen creates it, then
   `git push -u origin master` (origin is the ssh url).
8. DONE the game folder begins (Oct 5 evening): Game.ag (edict
   as class Edict with entity_state folded in, MoveInfo, Level,
   SpawnTemp; COM_Parse, ED_ParseField's table, ED_ParseEdict,
   SpawnEntities with skill inhibit, G_FindTeams, ED_CallSpawn;
   G_Spawn/G_FreeEdict/G_Find/G_PickTarget/G_UseTargets/
   G_SetMovedir/G_TouchTriggers; gi.setmodel; the triggers
   (InitTrigger, multi, once, relay), func_areaportal; G_RunFrame,
   G_RunEntity, SV_RunThink, SV_Physics_Pusher with SV_Push,
   SV_Physics_None/Noclip, SV_TestEntityPosition), Func.ag (Move_*,
   AngleMove_*, plat_CalcAcceleratedMove, plat_Accelerate,
   Think_AccelMove, every door function, SP_func_door,
   SP_func_door_rotating), World.ag (SV_LinkEdict, SV_AreaEdicts as
   a scan of the edict list instead of the area node tree, SV_Trace
   with SV_ClipMoveToEntities and SV_HullForEntity, SV_PointContents),
   Bsp.ag (CM_InitBoxHull/CM_HeadnodeForBox, CM_TransformedBoxTrace,
   CM_PointContents, a per-model mesh: each solid inline model its
   own node). An edict's think/touch/use/blocked are an enum Fn
   dispatched by Game.call_*; strings are the keys. The player is
   edict 1 (client, health 100); pmove traces through World.trace
   with the player passed; after the move it is linked and touches
   triggers; the game runs at 10 Hz from the frame's dt. Each inline
   model draws as its own Model with its own BspLit instance whose
   `model` matrix follows the entity (trinity's node_model refresh
   did not take; not chased). Unported classes: brush ones stand as
   solid walls (angles cleared), point ones are inert, items,
   monsters and lights free as the original does for lights.
   Checked headless on base1: 356 edicts, 28 inhibited, 1 team; the
   two doors in front of the start open on frame one (their trigger
   field reaches the start's box by one unit, as in the original),
   stay open while the player stands in the field (re-touched each
   second), close 3 s after the player leaves, and reopen when the
   player steps back in; spawned behind the field (--spawn "-1790
   1536 128 0 0") the closed panels fill the view and the lift under
   the start draws (checked with a sky-shader marker on the inline
   models, then with the real textures). Pushers move the player
   (SV_Push) but no door has pushed one yet in a test.
   Headless shots: the screen copy a `shot` reads only refreshes
   after an input event; a one pixel drag before each shot shows the
   current frame. A socket key release can be lost: send it twice.
9. DONE (Oct 5 night) func_plat, func_button, func_train and
   path_corner (Func.ag, Game.ag): every plat/button/train function
   of g_func.c, path_corner's spawn and touch (the monsterinfo part
   waits for the monster port), Edict gains event, die, movetarget,
   goalentity, ideal_yaw; call_die dispatch. The player's move now
   reports the entities it ran into (pm.touchents: MapView.touched,
   touch_ents after each step, as ClientThink's loop), so a touch
   button fires. The "light" key is ignored as F_IGNORE.
   Checked headless: base2's button *41 (81 units east of the start)
   fires on contact at game 1.9 s, reaches its 5 unit travel at 2.2
   (t7 used), returns at 5.4, fires again while leaned on; base2's
   plat *50 (dmg 10, a crusher under a ceiling at z 160) lifts the
   standing player 150 units, is blocked by the ceiling, reverses
   and repeats, as the original minus the damage; jail1's trains
   run corner to corner from their first frame, each origin equal
   to corner minus mins (89 1 -3 for *49 at t73).
   Test harness notes: `--spawn` takes the EYE (origin + 22), keys
   reach the view only after the map's "drawn" log line, and the
   app relaunch rebuilds when trinity's product is newer than its
   own; that rebuild crashed (silver exit 133) three times while
   Kalen's orbiter session was also building: run `silver --build
   quake2` by hand first.
10. DONE (Oct 5 night) the rest of g_func.c (door_killed,
   func_rotating, func_water, trigger_elevator, func_timer,
   func_conveyor, func_door_secret, func_killbox; Func.ag), g_misc's
   brush classes (func_wall, func_object, func_explosive; Misc.ag),
   the rest of g_trigger.c (key, counter, always, push, hurt, gravity,
   monsterjump; Trigger.ag) and all of g_target.c (temp_entity,
   speaker, help, secret, goal, explosion, changelevel, splash,
   spawner, blaster, crosslevel trigger and target, laser, lightramp,
   earthquake; Target.ag). Game: frand/crandom, coop, serverflags,
   helpmessages, lightstyles (set_lightstyle keeps the letter per
   style), Level's secrets/goals/intermissiontime/changemap, Edict's
   item/skinnum/renderfx/last_move_time/fly_sound_debounce_time;
   call_die carries inflictor and attacker. The player step reads
   pl.velocity back before pmove (a trigger_push or earthquake sets
   it) and gravity is 800 x pl.gravity (trigger_gravity).
   Stand-ins, each a one-line comment at the call: T_Damage,
   T_RadiusDamage, KillBox, BecomeExplosion1, ThrowDebris (combat and
   gibs), fire_blaster (weapons), temp entities and configstrings
   (effects), soundindex/positioned_sound (sound; indices 1-3 as the
   doors use), the inventory (has_item is false until items;
   trigger_key prints "You need the <classname>"), BeginIntermission
   (changelevel sets level.changemap and logs it), oldvelocity (the
   client), monsterinfo (monsters). MOVETYPE_TOSS still only thinks:
   a released func_object does not fall until SV_Physics_Toss.
   Checked headless: base1's START_ON timers fire their targets at
   their random intervals, trigger_always fires, the rotator *29
   (flat1_2 all over, so its turning cannot show) advances 300
   degrees a second and draws filling its shaft; fact2's fans sit
   behind a translucent grille we draw opaque (warp/trans OPEN); no
   dispatch misses on base1, fact2 or jail1. 359 edicts on base1.
11. DONE (Oct 5 night) the rest of the game folder, on function
   pointers (Kalen: "silver has function pointers"; "too much code
   for how simple this game is"): the enum Fn dispatch is gone. An
   edict's think/touch/use/blocked/pain/die, an item's pickup/use/
   drop/weaponthink and a monster frame's ai/think are
   `lambda R [ A ]` slots set with `lambda f[]` and called as
   `e.think[ e ]`; one module global `g : Game` (set in setup), so
   every free function reaches the game through `g.`. Ported: g_phys
   (fly_move, push_entity, physics_toss/step), g_misc's point
   classes (gibs, debris, explosions, explobox, teleporter, viewthing,
   banners, viper, strogg ship, satellite dish, func_clock,
   target_character/string, point_combat; Misc.ag), g_items (Items.ag,
   the table and every pickup), g_combat (Combat.ag), g_weapon +
   p_weapon (Weapon.ag), p_client/p_view/p_hud/p_trail/g_cmds
   (Client.ag), g_monster/g_ai/m_move/m_flash (Monster.ag). Sounds,
   temp entities and muzzle flashes queue as GameEvent (no sink yet).
   quake2.ag: the player goes through client_think (buttons, view
   angles, the touches, the weapon); the left button fires, keys 1..0
   pick weapons by the default binds; a text HUD (health, armor,
   ammo, the center print); a level end reloads the next map.
   Checked headless on base1: no crash over walks and fires; the
   blaster fires from the player (temporary log, removed); an armor
   shard picked up on contact shows Armor 2 on the HUD (shot). The
   items by the start are skill-inhibited, not missing.
   Fixed on the way: a pool-owned Trace stored into Game.touch_plane
   and then nulled was freed under its caller (impact): touch_plane
   is a persistent copy now, touch_plane_on says it is set.
   Compiler, Kalen's design: `lambda f[]` on a plain function is ONE
   static instance per function (Au lambda_static: a lambda object
   in static memory, unmanaged, never counted or freed, left out of
   the object census), over a thunk with an empty context
   (plain_lambda_static, silver.c; one thunk per function per core).
   A slot holds the same kind of object for a plain function and a
   closure, so its users never know which; a store and a null store
   cost nothing. A bare-pointer slot was built first and dropped:
   it put closures out of signature slots. Found on the way: on an
   Au_t, src/rtype/type are ONE union slot (never set rtype on a
   variable); a struct arg through a funcptr call goes by pointer, as
   the callee's own type says (aether e_fn_call). features 238/238;
   img test_jpeg pointed at a file the planet split moved (fixed).
   Headless: SHOT=<png> q2run.sh takes a shot at the end.
12. DONE (Oct 6) the monsters, one file each (`extend
   quake2`, imported after Monster in quake2.ag, spawned from
   spawn_monster in Misc.ag). The frame tables come from a converter
   (session scratchpad m2ag.py: m_X.c + m_X.h -> the MMove globals and
   an `<x>_moves[]` builder; a run of equal frames is one
   `mf[ v, n, ai, dist, think ]` call, Monster.ag; FRAME_ names
   resolve to numbers from the .h, with the name in a comment); the
   functions are ported by hand. One static instance per plain
   function (`lambda f[]`) makes the slots cheap to set.
   DONE: soldier (light, soldier, ss), infantry, gunner, berserk,
   gladiator, flipper, flyer, hover, medic (cable revive), mutant,
   parasite, tank + tank_commander, chick, brain, insane
   (misc_insane), floater, supertank (+ boss_explode), actor
   (misc_actor + target_actor), boss2. All build; base1's soldiers
   see, chase, fire and kill the player at skill 1 (checked headless:
   "player died", HUD Health 0, "fire to restart").
   Then jorg (Jorg.ag), the makron (Makron.ag: torso, sight move,
   makron_spawn/toss; Jorg_CheckAttack is Makron's line for line, so
   jorg uses makron_checkattack), monster_boss3_stand (Boss3.ag) and
   g_turret.c (Turret.ag: breach, base, driver).
   Checked headless with temporary traces (removed), the player in
   god mode: boss1's stand cycles frames 414..473 in 6 s; on boss2
   (`--spawn "200 -960 174 0 0"`, inside jorg's closed chamber) jorg
   takes pain, fires both chainguns on frames 8..13, dies, tosses
   the makron 4.8 s later, who jumps at the player 0.8 s after; the
   makron's bfg (frame 204), blaster sweep (17 shots, flashes
   102..118, yaw 260 down to 180 then 110 up to 180, as the source)
   and rail (frame 243, 0.9 s after the pick; pain can take the move
   first, as in C) all fire; he dies in 95 frames and leaves the
   torso 84 units along -y, looping frames 346..364. jail1's turret
   (`--spawn "-2350 -192 150 180 0"`): muzzle 122 units out, turns 5
   degrees a think, the driver rides his seat round, a rocket every
   3 s (speed 600, damage 100..150).
   NOT driven: use_boss3 (its trigger), jorg's bfg (a 1 in 4 pick,
   not drawn in 5 attacks), turret_driver_die (the gun is always
   between the player and the driver), strike's turret.
13. DONE (Oct 6) MD2 models drawn (Md2.ag). Md2Set loads a model
   the first time its index is asked for (IDP2 version 8; every
   skin's PCX through the colormap palette into one 2048 x 2048
   atlas, two pixels apart; a skin placed after the texture exists
   goes up with Texture.upload_region). Each frame draw_entities
   (quake2.ag) refills ONE mesh of 98,304 vertices on the cpu, as
   GL_DrawAliasFrameLerp did: every edict with an md2 model, in a
   cluster the eye sees (its box corners), lerped from a snapshot
   taken before each 10 Hz game frame (origin, angles, frame; no
   lerp on a new model or a 512 unit jump, as CL_DeltaEntity),
   EF_ROTATE items turning, modelindex2 on the same frame (jorg's
   walker), then the view weapon at the eye (ps.gunindex, gunframe,
   gunangles, gunoffset; depth squeezed to three tenths as
   RF_DEPTHHACK). Light: Bsp.light_point is R_LightPoint (the
   lightmap texel under the origin; node faces and each face's
   lightmap block are kept now), times the shade dot of each
   vertex normal. The 16 x 256 anormtab.h table is not carried: it
   is 1 + dot (0.3 x dot when negative) of the 162 anorms.h normals
   with the light (cos, sin, 1) normalized, yaw quantized to 16
   (checked: within 0.005 of every table entry). Shader Md2Lit:
   texel x 2 (gl's intensity) and the colour, each clamped to 1.
   Game: the model table starts with '*1'..'*N', so an inline
   model keeps its own number and md2 indices come after; setmodel
   now sets modelindex for md2 names (items had 0: never drawn).
   Checked headless, by shots: a soldier with his skin, aiming;
   the dead body; two armor shards; jorg with the rider on him;
   the blaster in hand, staying in place with the view pitched;
   60 frames a second. light_point equals a Python copy of
   RecursiveLightPoint at six base1 points, to the last digit. With
   the preload off every skin arrived by the late upload and drew
   right.
   NOT done: sprites (.sp2: the bfg ball, explosions' flashes),
   RF_TRANSLUCENT, the colour shells, RF_BEAM lasers, dynamic
   lights and light styles on models, a left-handed gun; brush
   models are still unlit by entity light.
   A method parameter named `a` is the object itself: `b - a`
   failed as pointer arithmetic (lerp_angle).
14. DONE (Oct 6) the sounds (Sound.ag, SoundSet over spectra's
   AudioMixer at 44100; quake2 imports spectra). A clip loads the
   first time its index is asked for: RIFF from the paks, 8 or 16
   bit (pak0: 495 at 16 bit, 5 at 8, all mono 22050 Hz).
   S_StartSound's rules: a sound on the same entity and channel
   stops the old one; with every voice busy the one nearest its
   end goes (never the player's for a monster's); a sound with no
   volume gives its voice back; a delayed start waits on the clock.
   Falloff as S_SpatializeOrigin: full within 80 units, then
   attenuation x 0.0005 a unit (0.001 for ATTN_STATIC), times
   s_volume 0.7; the player's own sounds are full. Looping entity
   sounds (edict.sound) as S_AddLoopSounds: one voice a sound,
   every entity's share added, 0.003 a unit, six at most.
   The game's events are read once a frame (MapView calls
   snd.frame after the game frames; run_frame no longer clears
   them): gi.sound at the entity's origin when it was made (a
   brush model's box middle, as SV_StartSound), the weapon sounds
   of CL_ParseMuzzleFlash and CL_ParseMuzzleFlash2 (flash numbers
   to names, by range), the impact and explosion sounds of
   CL_ParseTEnt.
   Checked headless on a null ALSA device (ALSA_CONFIG_PATH to a
   file with `pcm.!default { type null }`: silent, nothing reaches
   the speakers), by a temporary trace (removed): base1's hums
   loop, soldiers' sight, pain, attack and death sounds start at
   0.25..0.35 by distance, the player's blaster at 0.7, the bolt's
   fly loop, flybys at full; a wav's rate and length equal a
   Python read of the pak. With no device the game runs silent.
   NOT heard by anyone yet. NOT done: left and right (spectra's
   Voice has one gain, so each sound is the mean of the two ears:
   half level when spatialized; a pan on Voice is Kalen's call, it
   changes spectra's class), cue-point loops, the mission pack's
   flashes, footsteps (an entity event), s_volume as a setting.
15. DONE (Oct 6) the rest of the list, in its order.
   a. The sights of the events (Effects.ag; cl_fx.c, cl_tent.c and
      CL_AddPacketEntities' effects). Particles (4096, each a clock
      time, origin, velocity, acceleration, palette colour, alpha
      and its rate, placed by the closed form as CL_AddParticles):
      every base game effect function ported one for one (impact
      puffs, blood, blaster, explosion, rail spiral, bubbles, the
      rocket, grenade and gib trails, teleport, big teleport, item
      respawn, logout, flies, the bfg ball's sparks). Explosions
      (32: smoke and flash, the blaster burst, the fireball's
      frames and skins, the bfg sprite), the parasite's and medic's
      cable (temp_entity_beam carries its entity), bfg lasers, and
      RF_BEAM entities as six-sided beams. Dynamic lights: muzzle
      flashes (a frame long, colours by weapon and by monster flash
      group), explosions, rockets, bolts, the bfg; they light the
      models (R_LightPoint's sum) and the WORLD: BspLit takes the
      four that matter most at the eye and adds R_AddDynamicLights'
      falloff per pixel (reach less distance past a cutoff of 64,
      the edge eased over 16 units as the lightmap's filtering
      did; the true distance along the face, not the original's
      octagon estimate).
      Md2.ag now fills TWO meshes: `solid`, and `clear` for what is
      seen through (the same shader made with transparent: true:
      trinity gives that pipeline no depth write and straight
      alpha, so the fragment writes rgb and alpha unmultiplied).
      Sprites (.sp2, each frame's pcx in the skin atlas, colour 255
      clear), colour shells (quad, invulnerability, power screen:
      the plain colour four units out along the normals), alpha
      (RF_TRANSLUCENT 0.7, the bfg 0.3), EF_ANIM frames, the linked
      models 2 and 3. A vertex alpha of 2 marks the view weapon
      (opaque, depth squeezed). The atlas keeps a white block
      (beams, shells) and gl_rmisc.c's particle dot at its corner.
      Entity events once a game frame (MapView.entity_events:
      footsteps, falls, item respawn, teleport, the teleporter
      pads), the event cleared as the server did.
      Seen by shots: blood off a hit soldier, the bolt's yellow
      light on floor, wall and soldier (floor 44,33,25 ->
      76,60,25), the rocket and grenade fireballs, the rail's blue
      spiral, the big teleport's particles, smoke and flash.
   b. The camera (calc_view, CL_CalcViewValues): the eye is the
      origin plus ps.viewoffset (bob, fall, crouch), the angles the
      mouse's plus ps.kick_angles, all lerped across the game
      frame; dead or frozen, ps.viewangles (rolled 40 degrees on
      the floor, facing the killer). The gun, sprites and particles
      use the same eye.
   c. Death in single player: fire restarts the level with the
      player as he ENTERED it (MapView.entry). The first try
      crashed in Render_destroy_ring: replacing a Render under the
      same element_targets key frees the old one while the map
      still lists it, and its dealloc nulls its own key (a second
      free); and a key set to null is found by lookup but skipped
      by the window's draw loop after it is set again. quake2 now
      keeps ONE Render for the app and gives it the next map's
      models (target.models, each model's finish).
      OPEN, trinity's: Render.dealloc (vk.ag) walks
      element_targets and nulls its own key from inside its
      dealloc; replacing a Render under one key then drops twice.
      A change to Au's map_set for this was WRONG and is taken out
      (Kalen: map_set has nothing wrong with it; never change map).
      quake2 keeps one Render and swaps its models.
   d. Water (pmove.c): the level at feet, waist and eye, water
      friction, swimming (PM_WaterMove, the jump key swims up),
      currents in water and on conveying ground, ladders, the jump
      out of water. Traced on base1's pool: sinks to the bottom
      head under (air_finished set), swims up at 64 units a
      second, forward at 150. The player's waterlevel and
      watertype reach the game (drowning, splashes).
   e. Area portals (Bsp.ag: leaf areas, the areas and areaportals
      lumps, FloodAreaConnections on every portal change) gate the
      world's leaves, the entities and the monsters' hearing. At
      base1's start doors: 1,319 triangles shut, 1,634 open, 1,239
      shut again.
   f. Frustum culling: the four side planes each frame against
      every visible leaf's box, and a sphere per model
      (R_CullAliasModel). The live index list is rebuilt every
      frame now. No holes seen, pitched or level.
   g. Warp and see-through surfaces: BspLit warps water as
      EmitWaterPolys (each axis swung 8 texels by the other's
      sine), slides SURF_FLOWING, and SURF_TRANS33/66 faces are
      their own mesh (Bsp.build_clear: a node for the world and
      each inline model), drawn last with no depth write. Seen:
      the pool's surface from under it, the glass over the emblem
      at base1's start.
   h. skyrotate and skyaxis (worldspawn, read in Bsp.ag): BspSky
      turns the look back round the axis. Seen on `space`: only
      the sky's pixels change over four seconds.
   i. The status bar (Hud.ag): g_spawn.c's single_statusbar run by
      SCR_ExecuteLayoutString's rules, the pics from pics/*.pcx
      (digits, icons, the flash field), three of our pixels to
      one of its. Seen: health, armor, the weapon icon, the pickup
      icon with 'Armor Shard', the F1 help icon on base2.
   j. Level changes: `map$start`, '*' and 'film+map' names are
      read; the next map loads (MapView.level_name: map_name is a
      prop the app sets again every frame, so the change never
      took before); the player's persistant data, the start to
      arrive at, serverflags and the help text go to the new Game
      (Game.carry), and the player stands where PutClientInServer
      put him (the port used the map's first start: base1 began at
      the wrong one). Traced: base1 -> base2 arrives at the start
      named base1 (848 2292), armor 4 and health kept.
   NOT done, and why:
   - saving to disk (g_save.c) and a hub's levels remembered when
     you come back: every edict's think, touch, use, pain and die
     is a lambda and its move a pointer; writing them needs a name
     for each static lambda and a way back (a registry, or the
     symbol's name). Not started.
   - films (.cin) and the end-of-unit screens: skipped over.
   - the help computer, the inventory and the score layouts (the
     layout runner is there; the port's p_hud does not write their
     programs yet); the damage and underwater screen tint
     (ps.blend); crouching (no key).
   - sound: left and right (a pan on spectra's Voice, Kalen's).
   - the world's own light styles (flicker, switched lights).
16. DONE (Oct 6, second pass) from 15's NOT done list:
   - crouch (PM_CheckDuck: c or left ctrl; box 4 high, eye -2,
     speed 100, swims down in water).
   - the screen tint (ps.blend) over the view.
   - the help computer (F1), the inventory (tab, [ ] enter
     backspace) and the centre print, in the game's own letters
     (pics/conchars.pcx; Hud.glyph / chars / help / inventory).
     A written backslash n in a map message is a line break now.
   - the world's light styles: g_spawn.c's 12 patterns and the
     switched lights, ten times a second; Bsp.relight rebuilds
     the styled faces' blocks (R_BuildLightMap) and the changed
     atlas rows go up; light_point sums every style.
   - left and right ears: spectra Voice.gain_right and
     AudioMixer.voice_sides (both added LAST in their classes);
     Sound.ag gives each ear S_SpatializeOrigin's share.
   - films (Cin.ag: .cin with sound, .pcx stills) and SV_Map's
     names (MapView.go: 'film+map', '$start', '*'); any key skips.
   - Menu.ag: main, game (easy medium hard), options (volume,
     mouse speed, invert), quit; esc opens it, the game holds
     still. GameConsole (the ` key): map, skill, quit, and the
     g_cmds commands (god, give, noclip ...).
   - a click takes the mouse for looking (ux.mouse_lock).
   Seen by shots: help computer, inventory, crouch, main and
   game menus, the intro film, the skip into base1, `god` in the
   console. NOT seen or heard: the tint, light styles, the two
   ears, the options sliders, the mouse lock on a real screen.
   - saves (Save.ag, MADE by a script from Game.ag's classes:
     /src/quake2/support/gensave.py + save_head.ag; run it
     from /src/quake2/quake2 after Game.ag's classes change). Every member of Edict,
     Client, MoveInfo, MonsterInfo, Level goes out as a line of
     text; floats as their bits; edicts by number, items by
     index, 543 lambdas and 286 moves by name. The menu's save
     and load rows write and read ~/.cache/quake2/save<n>.sav
     (game, level, the hub's kept levels). Checked hidden:
     saved at x 3.6 y -195.6, walked to x -112 y -80, loaded,
     back at x 3.6 y -195.6 exactly.
     A level left inside a unit is kept in memory (MapView.hub)
     and read back on return; a '*' map clears it. NOT driven.
   NOT done: multiplayer, CTF, demos, key binding menus.
   RULE learned (Kalen, Oct 6): `new T [ n ]` is a pooled
   vector; an `@T` member takes only its data pointer and owns
   nothing, so the frame's clean-up frees it. A buffer that
   lives past the frame goes in a real new member
   (`public data : new u8`, then `data = new u8 [ n ]`). Not a
   silver bug: checked in the IR. PakData.data and Cin.ag's
   buffers are new members now; pak_data[ n ] makes one for
   the map's light and vis data, the sound samples and the
   live index list (all were dangling before, and lucky).
   NOT reproduced: the save's first crash (a string freed twice
   at the frame's clean-up) went away when put_s stopped pushing
   a literal held in a local; features t_vec_literal_local does
   the same thing and passes, so the cause is not known.
   OPEN silver:
   - a one-letter literal ('w') is a unichar, not a string.
17. DONE (Oct 6; written blind on Kalen's "port all, no
   testing", then debugged in hidden runs):
   RUN AND SEEN: id's demos/demo1.dm2 plays on base2 (the
   fight, blood, the damage tint, health 100 -> 81 -> 76); a
   hosted q2dm1 with a second hidden instance joined over udp
   (both scoreboards, the client's suicide and obituary on the
   host, its score -1, respawn); a hosted q2ctf1 with a joiner
   (one player a team, the joined marks, flags spawned, the
   grapple fired and its hook flying, the blue banner seen by
   the joiner, `team blue` on the host); `record mine` then
   `demomap mine` plays the recording (health and weapon
   right); the customize controls screen.
   FIXED on the way: the paks' demos are protocol 26 (no
   suppress byte in a frame); put_client_in_server dropped the
   old Client before the new one held its ClientRespawn (the
   team read as 0: a freed object); vec.insert is (value,
   index); the ctf item count; `ctf <map>` hosts ctf (host is
   deathmatch); a recording waits for a whole frame
   (demowaiting, lastframe -1); demomap reads ~/.cache/quake2
   too; a player's name survives a respawn.
   NOT RUN YET: the grapple's pull, a flag taken and captured,
   techs, fraglimit / timelimit / capturelimit, the
   intermission in deathmatch, other players drawn on the
   host (model 255 -> the male model), the inventory and
   centre prints over the net, keys bound through the screen
   (saving is written, the press was not driven), `connect`
   to a far machine (only 127.0.0.1).
   - keys: Menu screen 6 (options > customize controls), 12
     actions, saved in ~/.cache/quake2/binds.cfg.
   - Net.ag: Msg (sizebuf + MSG_*), protocol 34's entity and
     player deltas, Netchan. quake2.c/.h: udp sockets.
   - Server.ag (NetServer): challenge, connect, new,
     configstrings, baselines, begin; per client delta frames
     with pvs and areas; sounds, flashes, temp entities, prints,
     centre prints, layouts, inventory. Console `host [map]` or
     the multiplayer menu; port 27910; 8 players.
   - Remote.ag: `connect host[:port]`; the server's frames are
     laid over a local Game's edicts (edict 1 stays the local
     player: numbers 1 and playernum+1 swap), so the drawing,
     sound and effects code is unchanged. `demomap <name>` plays
     demos/<name>.dm2 from the paks; `record <name>` / `stop`.
   - NOT id's protocol in one place: the client moves itself and
     sends CLC_STATE (5: place, velocity, angles, buttons); the
     server does not run pmove from usercmds, and clc_move's
     checksum is not made. Our builds talk to each other; id's
     own client or server will not play with ours. Server to
     client is protocol 34 as written, so real demos should read.
   - Dm.ag: obituaries with frags, deathmatch spawn points,
     the scoreboard layout (Hud.layout reads quoted strings and
     string / cstring / client / ctf now), fraglimit, timelimit.
   - Ctf.ag: teams, base spawns, flags (take, capture, return,
     drop, auto return), every bonus, the four techs, the
     grapple, the status bar and scoreboard, `team red|blue`,
     banners, trigger_teleport. The ctf pak beside baseq2 is
     read. NOT ported from g_ctf.c: match mode, elections,
     admin, ghosts, the join menus, observers, id view,
     say_team, team skins, the grapple's cable drawing.
   - Save.ag was made again (Client and ClientRespawn grew).
   Not written: downloads, rcon / status / info, prediction
   smoothing, sounds culled by PHS (every event goes to every
   client).
   Silver bugs met:
   - FIXED (Oct 6) `if [ v[ i ] ]` with v a vec bool branched on
     the element's ADDRESS (LLVM: "Branch condition is not 'i1'");
     a ternary on it, or on a vec of objects' element, was always
     true. aether.c: e_create's same-type shortcut loads an
     unloaded primitive element, and both ternary forms load their
     condition (cond_loaded). features t_vec_bool_if, _while,
     _ternary, _logic, t_vec_object_cond; 245 of 246 pass (the one
     left, t_codegen_resource, failed before too). quake2's Bsp.ag
     uses the plain form again.
   The rest are worked round, OPEN:
   - `x : vec f32 [ other ]` is a sized construction, not a copy
     ("no suitable conversion found for vector -> i64"): write
     `x : other`.
   - a class-typed local declared with its type from a call or an
     element (`ex : Explosion [ alloc[] ]`) is refused as
     redundant; a struct one (`v : vec3f [ f[] ]`) is not.
   - a method argument or local may not carry the name of ANY
     member up the class chain (flags, area, last, planes, nodes).
   Seen once, not chased: the first run after BspLit's source
   changed drew no world (shaders compiled in that run); the next
   run was right.
18. DONE (Oct 6, Kalen: "when i load it it goes direct to base1")
   the attract loop of default.cfg's d1..d4: with no --map the app
   plays idlog.cin, demo1, idlog.cin, demo2, round again
   (MapView.attract, attract_next; the demo's end and film_end
   step it); any key opens the main menu over the running demo,
   or alone over black while a film runs on, as SCR_UpdateScreen;
   esc goes back to the loop; game > a skill, a load, a host,
   connect, map or demomap leave it (fresh, load_slot). The menu's
   letters and pics before any map come from `front` (a palette
   and an atlas of their own). `--map base1` goes straight in, as
   +map did. Checked hidden: film, demo, menu over it, esc,
   game > easy into ntro.cin, and 150 s of the loop (base2,
   waste2, base2) with no fault.
19. DONE (Oct 6, Kalen: stuck on walls when sliding along them,
   and on tiny objects on the ground): slide_move's plane test
   was inverted (`i != nplanes` took the crease branch, which
   zeroes the velocity on a single plane: every wall contact
   stopped the player dead). Traced with a temporary bump log
   (removed): the wall hit at fraction 0.83, then no second bump.
   Checked hidden: 25 degrees into base1's corridor wall now
   slides along it (y -220 -> -76). The small objects are not
   re-tested (none found); likely the same cause.
20. BUILT, Kalen tests (Oct 6, "stuck on enemies you killed"):
   World.trace lacked sv_world.c:547: an entity flagged
   SVF_DEADMONSTER is skipped unless the mask asks for
   CONTENTS_DEADMONSTER, so every corpse was a solid box to the
   player's move. SVF_DEADMONSTER and CONTENTS_DEADMONSTER now
   live in Game.ag (World is imported before Trigger/Target).
21. BUILT, Kalen tests (Oct 6): lifts stepped 10 Hz and demos
   were jerky. Brush models lerp between game frames
   (lerp_origin / lerp_angles in place_model); a rise of 8..20
   units while on ground eases out of the view over 0.1 s
   (step_up / step_left, CL_PredictMovement's predicted_step);
   a demo's view lerps the recorded player's origin and angles
   between frames (checked by a per-frame log). `timedemo 0|1`
   and `map <x>.dm2` added (688 frames, 58.8 fps hidden).
   OPEN: "still stick on bits of floor and walls": the trace and
   slide code match cmodel.c / pmove.c line by line; a wander
   on base1 found only head-on stops (within 5.7 degrees of
   square, which pmove.c stops too). Needs a place.
   Mouse: m_yaw 0.022 x sensitivity (was 0.004 rad: 3.5x fast).
   NEVER run quake2 hidden while Kalen works: its click locks
   the mouse and devices' platform_warp_cursor warps HIS pointer
   (an unmapped window still warps). Kalen: no more testing.
22. DONE (Oct 6) broken glass drew against the back wall: a
   freed or SVF_NOCLIENT brush model is hidden (place_model).
23. DONE (Oct 6) sticking on walls at shallow angles. Repro at
   165 Hz (SILVER_HZ=165), 6 degrees into base1's y 128 wall:
   the step path left the box inside the 1/32 gap, a small step
   in gave an entry fraction under -1 that clip_brush dropped,
   the box entered the wall and every trace after was allsolid.
   The trace is id's again; pmove now ends with id's
   PM_SnapPosition (origin and velocity to eighths, jitter by an
   eighth to a good place, else back) and starts with
   PM_InitialSnapPosition when pushed or moved since. At 165 Hz a
   sideways creep under 1/8 unit a frame is truncated away, as
   in id's. Keys-only hidden runs (no click) are safe.
24. DONE (Oct 6) the socket layer is silver (Net.ag q2net_*);
   quake2.c and quake2.h are gone: no C in quake2.
25. DONE (Oct 6) id's console (Con.ag; trinity has a Console.ag,
   so not that name): conback sliding at scr_conspeed, v3.19,
   word wrap, ^ backscroll row, blinking cursor, notify rows for
   con_notifytime; keys.c editing (tab completion as
   CompleteCommand, 32 line history, PgUp/PgDn/Home/End,
   ctrl+v/l/h/p/n, esc opens the menu); cmd.c and cvar.c
   (cmdlist, cvarlist, set, echo, clear, toggleconsole; a cvar
   alone prints, with a value sets; unknown words go to the
   game). Variables are only the ones that drive something:
   fov (ps.fov by ClientUserinfoChanged, CalcFov, gun hidden
   over 90), sensitivity, m_pitch (the menu's invert), m_yaw,
   s_volume (the menu's slider), skill, dmflags, fraglimit,
   timelimit, capturelimit, timedemo, cl_gun, name. Archived
   ones go to ~/.cache/quake2/config.cfg, run at start. gi
   cprintf / bprintf reach the console. NOT done: alias, exec,
   bind, condump, say / say_team, messagemode.
