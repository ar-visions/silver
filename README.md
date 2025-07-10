# **silver** lang
development in progress, with documentation to be added/changed.

# A-type runtime
A-type is a C-based object system designed for clear maintainable code that is reflective and adaptive. Use familiar OOP patterns in C with less boilerplate.  Far more leniant than most Object-Oriented languages. It's in C, and here less code can do more. Just add a few macros. Reflective members of classes, enums methods and props enable for sophisticated control in compact design. The result of that is scalable performance you can take to more platforms. It's javascript meeting C and python in self expanding fashion.

<a href="https://github.com/ar-visions/A/actions/workflows/build.yml">
  <img src="https://github.com/ar-visions/A/actions/workflows/build.yml/badge.svg" alt="A-type build" width="444">
</a>

```c
#include <A>

int main(int argc, char **argv) {
    cstr        src = getenv("SRC");
    cstr     import = getenv("IMPORT");
    map        args = A_args(argv,
        "module",  str(""),
        "install", form(path, "%s", import));
    string mkey     = str("module");
    string name     = get(args, str("module"));
    path   n        = new(path, chars, name->chars);
    path   source   = call(n, absolute);
    silver mod      = new(silver,
        source,  source,
        install, get(args, str("install")),
        name,    stem(source));
}

define_class (tokens)
define_class   (silver, ether)
```

We have named arguments in our new() macro and that means 1 ctr/init to code not arbitrary amounts.  It's a far better and more productive pattern, and even more lean than Swift.  This is a post-init call, where we have already set the properties (where holds happen on non-primitives).  A-type standard destructor will also auto-drop member delegates that are object-based.  It just means you don't have to do much memory management at all.

The flattened macro function interface table is one you keep adding to, and they work across all of the objects that support those method names.  This, so you don't have to use the generic 'call' macro.  The large number of defines is actually fine and lets you use variables that clash with the names.  

Orbiter
an IDE being built with silver (was C++)
[https://github.com/ar-visions/orbiter.git]

Hyperspace
spatial dev kit, ai module & training scripts
[https://github.com/ar-visions/hyperspace.git]


# dbg component (A-type, universal object for C or C++)
- componentized debugger using LLDB api; accessed by universal object

# example use-case
```c
#include <dbg>

object on_break(dbg debug, path source, u32 line, u32 column) {
	print("breakpoint hit on %o:%i:%i", source, line, column);
	print("arguments: %o ... locals: %o ... statics: %o ... globals: %o ... registers: %o ... this/self: %o", 
	cont(debug);
	return null;
}

int main(int argc, symbol argv[]) {
	map args  = A_arguments(argc, argv);
    	dbg debug = dbg(
		location,  f(path, "%o", get(args, string("binary")),
		arguments, f(string, "--an-arg %i", 1));
	break_line(debug, 4, on_break);
	start(debug);
	while(running(debug)) {
		usleep(1000000);
	}
}
```

# **import** keyword
**silver** starts with **import**. The **import** keyword lets you build from repositories from projects in any language.  It also uses local silver/C/C++/rust modules directly if file identifiers given. Your local checkouts are prioritized before external checkouts, so you can build externals locally with your own changes, and silver will track these changes.  The build process will recognize the various environment variables such as **CC**, **CXX**, **RUSTC**, **CPP** (type-bound pre-processor is planned for **silver** 1.0)

As a language, **silver** is all about efficiency: fewer moving parts (no direct requirement of Make, CMake for your projects), fewer tokens, and a strong stance against centralized package management. In watch mode (or development mode), changes are built immediately, with large C headers kept in memory for faster updates. **silver** is also the language target for the Orbiter IDE, which is currently in development.

# **A-type** foundation
A-type is the foundation of **silver**'s compiler and reflection system. It provides compatibility and reflection capabilities that enable dynamic behavior and runtime type inspection. With A-type, you can write classes in C and seamlessly use them in **silver**, similar to Python's extension protocol. A-type makes **silver** adaptable and extensible, integrating deeply with both the language and its C interoperability features.

see: [A-type project](https://github.com/ar-visions/A)

```python
import WGPU 'https://github.com/ar-visions/dawn@2e9297c45'
    build:      [-DDAWN_ENABLE_INSTALL=1 -DBUILD_SHARED_LIBS=0]
    includes:   ['dawn/webgpu' 'dawn/dawn_proc_table']
    links:      ['webgpu_dawn']

##
# designed for keywords import, class, struct, enum
# member keywords supported for access-level: [ intern, public ] and store: [ read-only, inlay ]
# primitives are inlay by default, but one can inlay class so long as we are ok with copying trivially or by method
# public members can be reflected by map: members [ object ]
##

# there are no commas in arguments, less we are expressing context arguments
string op + [ i:int  a:string ] -> string
    return '{ a } and { i }'

fn some-callback[ i:int, ctx:string ] -> int
    print[ '{ctx}: {i}' ]
    return i + len[ ctx ]

# methods can exist at module-level, too.
# for this, we may incorporate generic, 'this' is applicable to any object
generic[ second:int ] -> string
    return '{typeof[ this ]} generic, with arg {second}'

nice: some-callback[ 'a-nice-callback' ]

fn a-function [ string a ] -> string
    i : 2 + sz[ a ]
    r : nice[ i ]
    print[ 'called-with: %s. returning: { r }'  a ]
    return r

fn a-function [ string a ] -> int [2 + sz[ a ]]

class app
    public value     : short 1
    intern something : 2 # default is int

    mk-string [ from: int ] -> 'a string with { from }'

    cast int
        my-func  = ref run
        r:int[ my-func[ 'hi' ] ?? run ] # value or default
        s:mk-string[ r ]
        return len[ s ]
    
    run[ arg:string ] -> int
        print['{ arg } ... call own indexing method: { this[ 2 ] }']
        return 1

fn module-name[ a:app ] -> int
    is-const = int[ a ] # = denotes constant assignment, this calls the cast above
    val : is-const      # : assignment [mutable]
    val += 1
    print[ 'using app with value: { is-const } + { val - is-const }' ]
    return [a.run[string[val]] > 0] ? 1 : 0


# **meta** keyword (reserved for 1.0 release)
classes have ability to perform meta instancing.  think of it as templates but without code expansion; it simply does high-level reflection with the typed symbols.  meta is a simple idea, it's nothing more than an array of types you provide to the class when using it.  the class accepts types at meta index.  
```python
meta [ I:any ]
class list
    I type # in this context, I becomes an 'object' type, the base A-type we're ABI compatible with
