# **silver** lang
development in progress, with documentation to be added/changed.

![silver-lang](silver-icon.png "silver-lang")

Update: context addition to release 88:
### Context-Aware Members

In silver we describe members that implicitly **pull from context** without requiring explicit passing by user, unless with intent.
Inherently required arguments, context represents a second level of public member, one that is implicit and allows for syntax reduction

If the context is not available and the user does not specify it, the compiler will require them to either provide it.  Passing null for an object is following that rule.

This enables natural, readable code with less boilerplate.  When the user does describe syntax, it is with clearer intention.

```python

linux ?? import [ https://gitlab.freedesktop.org/wayland/wayland-protocols 810f1adaf33521cc55fc510566efba2a1418174f ]

import <vulkan/vulkan.h> [ https://github.com/KhronosGroup/Vulkan-Headers main ]

import [ https://github.com/KhronosGroup/Vulkan-Tools main ]
	-DVULKAN_HEADERS_INSTALL_DIR={install}
	{linux ?? -DWAYLAND_PROTOCOLS_DIR={install}/checkout/wayland-protocols}

version = '22'

# no one can override these, but :'s can be when importing; this can change what gets imported, how its built, etc.
# this demonstrates language control over more than is typical from a module interface
# silver is a build language first, and one that treats git as first class

class Vulkan
    intern instance : VkInstance
    public a-member : i32
    public major    : i32
    public minor    : i32
    
    fn init[]
        result : vkCreateInstance [
            [
                sType : VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO

                pApplicationInfo [
                    sType              : VK_STRUCTURE_TYPE_APPLICATION_INFO
                    pApplicationName   : "trinity"
                    applicationVersion : VK_MAKE_VERSION(1, 0, 0)
                    pEngineName        : 'trinity-v{version}'
 
                    # 88 exposes C macro, however we do design-time eval on the tokens, as 'feature'
                    # const is how we effectively perform silvers macro level
                    # it can go as far as calling runtime methods we import (not in our own module please, yet!)

                    engineVersion      : VK_MAKE_VERSION(const i64[ first[version] ], const i64[ last[version] ], 0)

                    apiVersion         : vk_version
                ]
            ], null, instance ]

        # methods at expr-level 0 do not invoke with [ these ] unless the function is variable argument
        verify result == VK_SUCCESS, 'could not start vulkan {VK_VERSION}'


class Window
    context vk : Vulkan
    dimensions : vec2i

main test_vulkan
    public  queue_family_index : array 2x4 [ 2 2 2 2, 4 4 4 4 ]
    intern  an_intern_member   : i64[ 4 ]
    context an-instance        : Vulkan
    intern  window             : Window

    fn init[]
        an-instance : Vulkan[ major: 1  minor: 1 ]
        window      : Window[ dimensions: [ 444 888 ] ]
        

```

![orbiter avatar](core.png "orbiter avatar")

Orbiter
an IDE being built with silver (was C++)
[https://github.com/ar-visions/orbiter.git]

Hyperspace
spatial dev kit, ai module & training scripts
[https://github.com/ar-visions/hyperspace.git]

# **import** keyword
**silver** starts with **import**. The **import** keyword lets you build and include from projects in any language, with coupled configuration parameters and <comma, separated> includes.  Local source links are prioritized before external checkouts, so you can build externals locally with your own changes.  This is a far better way to collaborate in open source with yourself and others. Silver simply gets out of the way when it comes to git for your own source; it's merely importing.  The build process will recognize the various environment variables such as **CC**, **CXX**, **RUSTC**, **CPP**

As a language, **silver** is fewer moving syntactic parts (no direct requirement of Make, CMake for your projects).  It's fewer tokens, first class methods, and a strong stance against centralized package management.  It's considered a build language first, and tries to do the works after, by facilitating native build targets through standard compilation toolchain LLVM.  In watch mode (or development mode), changes are built immediately, with large C headers kept in memory for faster updates. **silver** is the language target for the Orbiter IDE, which is currently in development.

# **Au** foundation
Au is the foundation model of **silver**'s compiler and component system. It provides compatibility and reflection capabilities that enable dynamic behavior and runtime type inspection. With Au, you can write classes in C and seamlessly use them in **silver**, similar to Python's extension protocol. Au makes **silver** adaptable and extensible, integrating deeply with both the language and its C interoperability features.

see: [Au project](https://github.com/ar-visions/Au)

```python
import <dawn/webgpu, dawn/dawn_proc_table> [https://github.com/ar-visions/dawn 2e9297c45]
    -DDAWN_ENABLE_INSTALL=1 -DBUILD_SHARED_LIBS=0
    -lwebgpu_dawn

##
# designed for keywords import, class, struct, enum
# member keywords supported for access-level: [ intern, public ] and store: [ read-only, inlay ]
# primitives are inlay by default, but one can inlay class so long as we are ok with copying trivially or by method
# public members can be reflected by map: members [ object ]
##

string op + [ i:int, a:string ] -> string
    return '{ a } and { i }'

# context is used when the function is called; as such, the user should have these variables themselves
# this reduces the payload on lambdas to zero, at expense of keeping membership explicit
fn some-callback[ i:int, context ctx:Vulkan ] -> int
    print[ '{ctx}: {i}' ]
    return i + ctx.a-member

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
    is-const : int[ a ] # : denotes constant assignment, this calls the cast above
    val = is-const      # = assignment [mutable]
    val += 1
    print[ 'using app with value: { is-const } + { val - is-const }' ]
    return [run[a, string[val]] > 0] ? 1 : 0

```


