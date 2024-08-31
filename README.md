# **silver** Language
emit A-type library from a very succinct and modern language.  silver is designed to be the most reflective language possible, lightest weight C-based object model with almost no initialization required, yet polymorphic casts, methods, indexing and operator methods.  The actual output of silver can be debugged and mapped to our original source very easily.  A VScode gdb/llvm adapter is needed for the mapping, but Orbiter is essentially inline or development of this component first.

# **import** keyword
it starts with **import**.  import allows you to build from repositories in any language.  that makes **silver** a first class build system.  it's in-module so you only need 1 source-file for a production app.  its made to import anything, including images.  silver is project aware, and designed to get things done with less tokens, and less resources.  it's also a watcher, so changes to source are built immediately.  it makes very little sense not to keep C99 headers in memory and recompile with updates.  when it's established, a LLVM-frontend is future  architecture, with implementation of such in C++ (...one last time, C++).  For now, the C99 should compile roughly optimal to a proper LLVM frontend.

```python
# public will expose it's API, so you may just develop in C and use silver as build system
public import WGPU [
    source:     'https://github.com/ar-visions/dawn@2e9297c45f48df8be17b4f3d2595063504dac16c',
    build:      ['-DDAWN_ENABLE_INSTALL=1', '-DBUILD_SHARED_LIBS=0'],
    includes:   ['dawn/webgpu', 'dawn/dawn_proc_table'],
    links:      ['webgpu_dawn']
]

# ...or you can dig deeper and use its module member types, class, mod, struct, enum, union
class app [
    public int value : 1 # assign operator is ':' constant is '='  [ no const decorator ]
    void run[] [
        print 'hi -- you have given me {value}'
    ]
]

# this is a reduced language, but we do have cast, index, and named operators


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
classes have ability to perform meta instancing.  think of it as templates but without code expansion; it simply does introspection automatically with the typed symbols.  meta is a simple idea, it's nothing more than an array of types you provide to the class when using it.  the class accepts a fixed amount of types at meta index.  
```python
meta [ I:any ]
class list [
    I type # in this context, I becomes an 'object' type, the base A-type
]
