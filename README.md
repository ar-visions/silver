# **silver** Language
development in progress, with documentation to be added/changed

# **import** keyword
it starts with **import**.  import allows you to build from repositories in any language.  that makes **silver** a first class build system.  it's in-module so you only need 1 source-file for a production app.  its made to import anything, including images.  silver is project aware, and designed to get things done with less tokens, and less resources.  it's also a watcher, so changes to source are built immediately.  it makes very little sense not to keep C99 headers in memory and recompile with updates.  its meant to be the language the Orbiter IDE is going to be written in.  after-all you can't make a new language without an IDE

we are about 4 months into silver.  it started as a python-based silver to C transpiler and shifted into C++, then C.  then we dropped the transpiler idea to go with direct LLVM-IR API.  why output text to compiler later when you can compile as a front-end on LLVM.  technically we're using the IR APIs in LLVM.

built on an object model in C called A-type.

```python
# public will expose it's API, so you may just develop in C and use silver as build system
public import WGPU [
    source:     'https://github.com/ar-visions/dawn@2e9297c45f48df8be17b4f3d2595063504dac16c',
    build:      ['-DDAWN_ENABLE_INSTALL=1', '-DBUILD_SHARED_LIBS=0'],
    includes:   ['dawn/webgpu', 'dawn/dawn_proc_table'],
    links:      ['webgpu_dawn']
]

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

int a-function [ string a ] [
    int i: 2 + cast sz a
    print 'you just called me with %s, and ill respond with { i }', a
    return i
]

class app [
    public int value : 1 # assign operator is ':' constant is '='  [ no const decorator ]

    intern inlay struct [
        int    a: 2
        string b
    ] i-am-embedded

    public object[] all

    cast int [
        ref [int::string] my-func = ref run # function pointers work with or without object targets
        int    r: my-func ?? run
        return r
    ]

    int run[] print 'hi -- you have given me {value}' -> 2
]

int main[ app a ] [
    int this-is-const = a.value
    int val: this-is-const
    val += 1
    print[ 'using app with value: { this-is-const }, then added { val - this-is-const } ... our int is fixed at 64bit' ]
    -> a.run > 0
]

# this is a reduced language, but we do have cast, index, and named operators
# with reflection its in effect more features than you use and none that you avoid anyway.  effectively more there, more with silver.


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



# **import** keyword


# **meta** keyword
classes have ability to perform meta instancing.  think of it as templates but without code expansion; it simply does high-level reflection with the typed symbols.  meta is a simple idea, it's nothing more than an array of types you provide to the class when using it.  the class accepts a fixed amount of types at meta index.  
```python
meta [ I:any ]
class list [
    I type # in this context, I becomes an 'object' type, the base A-type we're ABI compatible with
]
