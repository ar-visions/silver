# **silver** lang
![silver-lang](silver-icon.png "silver-lang")

silver is a reduced-token, reflective systems language built on a polymorphic C macro object model (Au) that vectorizes cleanly, isolates intern data, and reflects public members into JSON, CSS, and UX transitions.
Its compiler imports C, C++, and other languages into a unified Aether model, letting developers build native apps, Vulkan engines, and AI-assisted components with fast two-second iteration from a token stack.
Design-time AI watchers generate and cache non-deterministic outputs, keeping authored code separate from LLM-generated dictation while integrating naturally into modules and media.
silver’s founding vision is an open, decentralized build ecosystem where cross-compiled toolchains, reflective models, and human-AI co-creation converge into a unified development experience.

development in progress, with documentation to be added/changed.

Update: context addition to release 88:
### Context-Aware Members

In silver we describe members that implicitly **pull from context** without requiring explicit passing by user, unless with intent.
Inherently required arguments, context represents a second level of public member, one that is implicit and allows for syntax reduction

If the context is not available and the user does not specify it, the compiler will require them to either provide it.  Passing null for an object is following that rule.

This enables natural, readable code with less boilerplate.  When the user does describe syntax, it is with clearer intention.

```python

linux ?? import [ https://gitlab.freedesktop.org/wayland/wayland-protocols 810f1adaf33521cc55fc510566efba2a1418174f ]

import <vulkan/vulkan.h> [ https://github.com/KhronosGroup/Vulkan-Headers main ]

# short-hand for git -- i think this is a good basis for 'web4' data
# its git provided, so it costs little to host, and we have our identities as url basis. with project/version, what else could democratize user provided media better in open?

import [ KhronosGroup:Vulkan-Tools/main ]
	-DVULKAN_HEADERS_INSTALL_DIR={install}
	{linux ?? -DWAYLAND_PROTOCOLS_DIR={install}/checkout/wayland-protocols}

# = means constant, it cannot be changed by the importer or ourselves, at any member level.
version = '22'

class Vulkan
    intern instance : VkInstance
    public a-member : i32
    public major    : i32
    public minor    : i32
    context string  : shared

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
    public  shared             : 'a-string'
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
**silver** starts with **import**. The **import** keyword lets you build and include from projects in any language, with coupled configuration parameters and <comma, separated> includes.  Local source links are prioritized before external checkouts, so you can build externals locally with your own changes.  This is a far better way to collaborate in open source with yourself and others. silver simply gets out of the way when it comes to git for your own source; it's merely importing.  The build process will recognize the various environment variables such as **CC**, **CXX**, **RUSTC**, **CPP**

# **export** keyword
**silver**'s contains export keyword, an invocation into any AI resource we are listening to for resource generation.  export src is a great way to include your src code for the project, as a resource in share.  this so more verification is enabled, and we can be transparent to the user with minimal resources used.  since AI is building for you right away, the 'mode' of this is during design-stage.  as such we do have a const.

As a language, **silver** is fewer moving syntactic parts (no direct requirement of Make, CMake for your projects).  It's fewer tokens, first class methods, and a strong stance against centralized package management.  It's considered a build language first, and tries to do the works after, by facilitating native build targets through standard compilation toolchain LLVM.  In watch mode (or development mode), changes are built immediately, with large C headers kept in memory for faster updates. **silver** is the language target for the Orbiter IDE, which is currently in development.

# **Au** foundation
Au is the foundation model of **silver**'s compiler and component system. It provides compatibility and reflection capabilities that enable dynamic behavior and runtime type inspection. With Au, you can write classes in C and seamlessly use them in **silver**, similar to Python's extension protocol. Au makes **silver** adaptable and extensible, integrating deeply with both the language and its C interoperability features.

see: [Au project](https://github.com/ar-visions/silver/blob/master/src/Au)

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

import a-silver-module [ a-non-const: enum-from-module ]



string op + [ i:int, a:string ] -> string
    return '{ a } and { i }'

# context is used when the function is called; as such, the user should have these variables themselves
# this reduces the payload on lambdas to zero, at expense of keeping membership explicit
fn some-callback[ i:int, context ctx:Vulkan ] int
    print[ '{ctx}: {i}' ]
    return i + ctx.a-member

# methods can exist at module-level, or record.

nice: some-callback[ 'a-nice-callback' ]

fn a-function [ a:string ] string
    i : 2 + sz[ a ]
    r : nice[ i ]
    print[ 'called-with: %s. returning: { r }'  a ]
    return r

fn a-function [ a:string ] int [2 + sz[ a ]]

class app
    public value     : short 1
    intern something : 2 # default is int

    mk-string [ from: int ] -> 'a string with { from } .. our -> guard indicates a default instance, same as member->access .. using -> effectively defaults null member allocation in cases where you want to be lazy.  this way we make pointers great again'

    cast int
        my-func  = ref run
        r:int[ my-func[ 'hi' ] ?? run ] # value or default
        s:mk-string[ r ]
        return len[ s ]
    
    run[ arg:string ] -> int
        print['{ arg } ... call own indexing method: { this[ 2 ] }']
        return 1

fn module-name[ a:app ] -> int
    is-const : int[ a ]  # : denotes constant assignment, this calls the cast above
    val  = is-const      # = assignment [mutable]
    val += 1
    print[ 'using app with value: { is-const } + { val - is-const }' ]
    return [ run[a, string[val]] > 0 ] ? 1 : 0

```


