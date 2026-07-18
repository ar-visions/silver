# silver

![silver-lang](silver-icon.png "silver-lang")

silver is a native build language. One file is a module. An import line is a git
repository, silver-module or a C header. The output is a native binary with full LLDB debug
information, on Linux, macOS, and Windows.

```
import KhronosGroup:Vulkan-Headers/29184b9
import glfw:glfw/fdd14e6 <GLFW/glfw3.h>
    +GLFW_INCLUDE_VULKAN
```

An import clones the repository at that commit, detects its build system,
builds it with that system's tools, and links the result. A C header import
is read by libclang; its types, functions, and macros become available to the
module. No binding code is written or generated.

```python
class greeter
    public name : string

    func hello []
        puts 'hello, {name}'

g : greeter [ name: 'world' ]
g.hello
```

Objects run on **Au**, a C runtime: reference counting, reflection, single
inheritance. C, C++, and silver use the same object model. A method declared
in silver without a body is implemented in a `.c` or `.cc` file beside the
module; it compiles and links with the module and reads the same fields.

Method calls are direct. Dynamic dispatch is written `method*[ ]` at the call
site.

## foundry

`foundry/` is software written in silver, developed in this repository:

- **trinity** — GPU interface engine: Vulkan, an SDF compute canvas, element
  trees styled with CSS. An app is one declaration: `element myapp : trinity`.
- **orbiter** — the IDE, a trinity app. It edits, builds, and runs silver
  modules, and hosts running apps in its panes — each app is its own process,
  sharing frames over a unix socket.
- **ai** — tensors, layers, training, with Vulkan compute.
- **spectra** — audio: decoding, mixing, spectrographs.
- **asnes / knes / rgen** — game console emulator elements, runnable on their own or within orbiter by means of enumeration of extensions (**export** keyword)

Language changes are validated against these apps.

A trinity app registers its presence as a socket named after the app. Any app
can broadcast to the others. Ctrl+click on an element in a running app emits
the style rule location that produced it; orbiter receives this and opens the
file at that line.

A module can also export keyed metadata at build time — `export extensions
['.gen', '.smd']` writes `export/rgen.agi` under the silver root. Any program
can iterate that folder, one readable file per module. Orbiter uses it to
open files: a `.gen` finds rgen by its own declaration and runs it in its own
process, in a pane. Modules built years apart associate this way without ever
being compiled together.

## tests

`expect func` at module level declares a test — a function returning `bool`:

```python
expect func check_math [] -> bool
    return 2 + 2 == 4

expect func test [ vk: vk_context ] -> bool
    w : Display [ vk: vk, backbuffer: true, width: 1024, height: 1024 ]
    c : Canvas  [ vk: vk, w: w, width: 1024, height: 1024 ]
    # draw, fence, read the pixels back, average them
    ...
    return avg_a > 10
```

Arguments are default-constructed by the test runner — the second test above
receives a real, live Vulkan context. A test whose arguments cannot be
constructed is reported as skipped, not silently dropped.

`expect` is also a statement inside any function — an assertion that prints
its message and halts under the debugger when false:

```python
expect result == VK_SUCCESS, 'swapchain creation failed'
```

`silver --test myapp` runs every module-level expect across the module and
all of its imports, in each module's own initializer. Each test prints its
name, then `passed` or `failed`; a failure aborts with the test named, a full
pass prints `expect: complete` and exits 0 before the app itself ever starts.
The same run happens for any app by setting `SILVER_EXPECT` in its
environment — the flag is only silver setting that variable for the launch.

The working pattern for a graphics app: render one frame the way the app
normally does, fence, read the pixels back, and pin the average color at the
app's default size — four integers, measured once from a build you approved.
A change anywhere in the render path moves the number and the test names
itself when it breaks.

Two build rules follow from this. A release build runs the debug tests first
and refuses to build if they fail. And release binaries contain none of the
test code at all — no bodies, no runner, no `SILVER_EXPECT` check.

## build

```bash
./bootstrap.sh        # SDK: llvm, runtimes, dependencies
make                  # the silver compiler
silver orbiter        # build and run the IDE
silver --test myapp   # run a module's expect tests
```

A module is a directory with a matching source file — `myapp/myapp.ag`.
`silver myapp` builds and runs it; silver's flags come before the module
name, and everything after the module name is passed to the app.

Language reference: [LANGUAGE.md](LANGUAGE.md)

*for Audrey and Rebecca — succinct code, inspiring more to enjoy the craft.*

MIT License — Copyright (C) 2017 Kalen Novis White
