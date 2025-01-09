# **silver** lang
development in progress, with documentation to be added/changed.

# **import** keyword
**silver** starts with **import**. The **import** keyword lets you build from repositories from projects in any language.  It also uses local silver/C/C++/rust modules directly if file identifiers given. Your local checkouts are prioritized before external checkouts, so you can build externals locally with your own changes, and silver will track these changes.  The build process will recognize the various environment variables such as **CC**, **CXX**, **RUSTC**, **CPP** (type-bound pre-processor is planned for **silver** 1.0)

As a language, **silver** is all about efficiency: fewer moving parts (no direct requirement of Make, CMake for your projects), fewer tokens, and a strong stance against centralized package management. In watch mode (or development mode), changes are built immediately, with large C headers kept in memory for faster updates. **silver** is also the language target for the Orbiter IDE, which is currently in development.

# **A-type** foundation
A-type is the foundation of **silver**'s compiler and reflection system. It provides compatibility and reflection capabilities that enable dynamic behavior and runtime type inspection. With A-type, you can write classes in C and seamlessly use them in **silver**, similar to Python's extension protocol. A-type makes **silver** adaptable and extensible, integrating deeply with both the language and its C interoperability features.

see: [A-type project](https://github.com/ar-visions/A)

```python
import WGPU
    source:     https://github.com/ar-visions/dawn@2e9297c45
    build:      [-DDAWN_ENABLE_INSTALL=1
                 -DBUILD_SHARED_LIBS=0]
    includes:   ['dawn/webgpu'
                 'dawn/dawn_proc_table']
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
