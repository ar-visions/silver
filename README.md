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

this is still a comment...

# every object is alloc'd and ref-counted except for non-inlay (bool ... i8 -> i64, f32 -> f64, inlaid structs etc)
#
# we assign with : and assign-const with =
# silver in data form may be thought of as a type-based json, and we support this form of serialization
#
# the ethos of silver is we are open source developers and we do not rely on specific package managers
# it should be simpler than using a package manager, too.  easier because you know the entire world
# is accessible.  thats something people actually demand.
#
# its also best for there to be 1 file that expresses import and logic, so we may build the entire
# stack from one modules expression.
# we can end the comment now with two #'s
#
# constant members of class arent read-only; constant applies to membership alone
# for data, it should use a membership keyword: read-only
#
# methods do not require [ bracket-parens ] if we are at the first level of expression
# ... no parens if we have no args, too.
#
# heres a program:
##

string operator add [ int i, string a ] [
    return '{ a } and { i }'
]

fn a-function [ string a ] -> int
    int i: 2 + sz[ a ]
    print 'you just called me with %s, and ill respond with { i }', a
    return i

fn a-function [ string a ] -> int [2 + sz[ a ]]

class app
    public value : short 1

    mk-string [ from: int ] -> 'a string with { from }'

    cast int
        my-func  = ref run
        int    r = my-func[ 'hi' ] ?? run # use default-state, otherwise
        string s = mk-string[ r ]
        return len[ s ] 

    run[ string arg ] -> int
        print '{ arg } ... call own indexing method: { this[ 2 ] }'
        return 1


fn module-name[ app a ] -> int
    is-const = int[ a ] # = denotes constant assignment, this calls the cast above
    val : is-const      # store by assignment [mutable]
    val += 1
    print[ 'using app with value: { this-is-const }, then added { val - this-is-const } ... our int is signed 64bit' ]
    return a.run > 0


```
| Operator | Function Name  | Description                                                   | Method Signature                              |
|----------|----------------|---------------------------------------------------------------|-----------------------------------------------|
| `+`      | add            | adds two numbers                                              | `T add [ T right-hand ]`                      |
| `-`      | sub            | subtracts the second number from the first                    | `T sub [ T right-hand ]`                      |
| `*`      | mul            | multiplies two numbers                                        | `T mul [ T right-hand ]`                      |
| `/`      | div            | divides the first number by the second                        | `T div [ T right-hand ]`                      |
| `\|\|`     | or             | logical or between two conditions                             | `T or [ T right-hand ]`                       |
| `&&`     | and            | logical and between two conditions                            | `T and [ T right-hand ]`                      |
| `^`      | xor            | bitwise xor between two numbers                               | `T xor [ T right-hand ]`                      |
| `>>`     | right          | bitwise right shift                                           | `T right [ T right-hand ]`                    |
| `<<`     | left           | bitwise left shift                                            | `T left [ T right-hand ]`                     |
| `:`      | assign         | assigns a value to a variable                                 | `T assign [ T right-hand ]`                   |
| `=`      | assign         | assigns a value to a variable (alias for `:`)                 | `T assign [ T right-hand ]`                   |
| `+=`     | assign-add     | adds and assigns the result to the variable                   | `T assign-add [ T right-hand ]`               |
| `-=`     | assign-sub     | subtracts and assigns the result to the variable              | `T assign-sub [ T right-hand ]`               |
| `*=`     | assign-mul     | multiplies and assigns the result to the variable             | `T assign-mul [ T right-hand ]`               |
| `/=`     | assign-div     | divides and assigns the result to the variable                | `T assign-div [ T right-hand ]`               |
| `\|=`     | assign-or      | bitwise or and assigns the result to the variable             | `T assign-or [ T right-hand ]`                |
| `&=`     | assign-and     | bitwise and and assigns the result to the variable            | `T assign-and [ T right-hand ]`               |
| `^=`     | assign-xor     | bitwise xor and assigns the result to the variable            | `T assign-xor [ T right-hand ]`               |
| `>>=`    | assign-right   | bitwise right shift and assigns the result to the variable    | `T assign-right [ T right-hand ]`             |
| `<<=`    | assign-left    | bitwise left shift and assigns the result to the variable     | `T assign-left [ T right-hand ]`              |
| `==`     | compare-equal  | checks if two values are equal                                | `bool compare-equal [ T right-hand ]`         |
| `!=`     | compare-not    | checks if two values are not equal                            | `bool compare-not [ T right-hand ]`           |
| `%=`     | mod-assign     | modulus operation and assigns the result to the variable      | `T mod-assign [ T right-hand ]`               |
| `is`     | is             | checks if two references refer to the same object (keyword)   | `bool is [ T right-hand ]`                    |
| `inherits`| inherits      | checks if a class inherits from another (keyword)             | `bool inherits [ T right-hand ]`              |



# **meta** keyword (reserved for 1.0 release)
classes have ability to perform meta instancing.  think of it as templates but without code expansion; it simply does high-level reflection with the typed symbols.  meta is a simple idea, it's nothing more than an array of types you provide to the class when using it.  the class accepts types at meta index.  
```python
meta [ I:any ]
class list [
    I type # in this context, I becomes an 'object' type, the base A-type we're ABI compatible with
]
