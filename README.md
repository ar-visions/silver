# **silver** lang
![silver-lang](silver-icon.png "silver-lang")

silver is a reduced-token, reflective systems language built on a reflective polymorphic C model (Au), isolating intern data, and offering more public exposure on members into JSON, CSS, and UX transitions.

The parser and import-related build system imports C, C++, and other languages into Au design-time models -- at design-time, we have actual runtime access to types we import.  By import, we are not merely 'compiling' but also running the module for silver to use at design time.

This lets developers build native apps in very innovative and modern ways -- to run on the most amount of hardware with built in cross compilation, letting import automatically symlink to your working source -- this, so you have a direct way to use and improve open source software from any package we import by its web4-like scheme: 


**import** identity:project/version

as used in a silver app
```python


# import is the first keyword that utilizes a tokens string
# for these we want to allow you to use your $(shell) when you wish, as well as {expression} at design-time
# our silver-app-properties are exposed as module members (install:path for one)

import [ KhronosGroup:Vulkan-Tools/main ]
	-DVULKAN_HEADERS_INSTALL_DIR={install}

# the above are lines typically found in a Makefile, or two -- 
# however with silver we wanted the language to build a software install that can define in code.
# this is keeping with Au's .g technique for graphing dependencies per compile-unit.

import <stdio.h> # lets include C models, and even type/value macros

# note: macros with matching type definitions are registered as type aliases, similar to how Swift imports types from C

# main is a hard-coded abstract class which sets properties by their 
class app of main
    required string msg

	none init[]

        # constant assignment
		string however: "its args need not be"

		puts[ "calling a C function with a const string: %s", 
            ' we only allow formatter args with const-string.. {however} ']

```


Design-time AI watchers generate and cache non-deterministic outputs, keeping 
authored code separate from LLM-generated dictation while integrating naturally 
into modules and media.


```python
# this is a native component that receives an import construction, thus creating a component-decentral model for implementing keywords in silver
# in the case of chatgpt, it hooks into function body generation using a 'using' keyword


all_globals_immutable: ''

import IEEE:product/version
    --config-with={}

# native generator-based component packaged in silver runtime
# will be able to checkout open models as well
import chatgpt
	model:'gpt-5'

class our-model [based]
    string  name
    double  scale

    # these dictations are stored in a folder of the module's stem name (module.ag -> module/our-model.a-method.ai)
    # this content is used to fill in the method name; in doing so we separate waht is machine-made vs human
    # however the human always defines the models, method prototypes, and arguments
    int a-method[string append_to_name, f64 scale_increment] using chatgpt
        [ 'take a look at this image for inspiration, along with the source provided by protocol; make this update the state based on the argments', image[ 'resource-image.png' ] ]
        [ 'never allow the name to be more than 10 characters' ] # we append more and save the file in watch-mode to update the cached .ai file

```

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

import <vulkan/vulkan.h> [ KhronosGroup:Vulkan-Headers/main ]

# short-hand for git shared git repo -- a good basis for 'web4' data
# its git provided, so it costs little to host, and we have our identities as url basis. with project/version, what else could democratize user provided media better in open?

import [ KhronosGroup:Vulkan-Tools/main ]
	-DVULKAN_HEADERS_INSTALL_DIR={install}
	{linux ?? -DWAYLAND_PROTOCOLS_DIR={install}/checkout/wayland-protocols}

# : means constant, it cannot be changed by the importer or ourselves, at any member level.
version : '22'

class Vulkan
    intern instance : VkInstance
    public a-member : i32
    public major    : i32
    public minor    : i32
    context string  : shared

    none init[]
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
    context Vulkan vk
    vec2i dimensions

class test_vulkan of main
    public  i64[2x4]  queue_family_index = [ 2 2 2 2, 4 4 4 4 ]
    intern  i64[ 4 ]  an_intern_member
    context Vulkan    an-instance
    intern  Window    window
    public  shared    'a-string'

    none init[]
        an-instance : Vulkan[ major: 1  minor: 1 ]
        window      : Window[ dimensions: [ 444 888 ] ]
        

```

![orbiter avatar](core.png "orbiter avatar")

Orbiter -- IDE being built with silver (was C++)
[https://github.com/ar-visions/orbiter.git]

Hyperspace
spatial dev kit, ai module & training scripts (will be silver)
[https://github.com/ar-visions/hyperspace.git]

# **import** keyword
**silver** starts with **import**. The **import** keyword lets you build and include from projects in any language, with coupled configuration parameters and <comma, separated> includes.  Local source links are prioritized before external checkouts, so you can build externals locally with your own changes.  This is a far better way to collaborate in open source with yourself and others. silver simply gets out of the way when it comes to git for your own source; it's merely importing.  The build process will recognize the various environment variables such as **CC**, **CXX**, **RUSTC**, **CPP**

# **export** keyword
**silver**'s contains export keyword, an invocation into any component, such as 'chatgpt', we may generate and cache a variety of media for our project resources.

As a language, **silver** is fewer moving syntactic parts (no direct requirement of Make, CMake for your projects).  It's fewer tokens, first class methods, and a strong stance against centralized package management.  It's considered a build language first, and tries to do the works after, by facilitating native build targets through standard compilation toolchain LLVM.  In watch mode (or development mode), changes are built immediately, with large C headers kept in memory for faster updates. **silver** is the language target for the Orbiter IDE, which is currently in development.

# **Au** foundation
Au is the foundation model of **silver**'s compiler and component system. It provides compatibility and reflection capabilities that enable dynamic behavior and runtime type inspection. With Au, you can write classes in C and seamlessly use them in **silver**, similar to Python's extension protocol. Au makes **silver** adaptable and extensible, integrating deeply with both the language and its C interoperability features.

see: [Au project](https://github.com/ar-visions/silver/blob/master/src/Au)

```python

##
# designed for keywords import, class, struct, enum
# member keywords supported for access-level: [ intern, public ] and store: [ read-only, inlay ]
# primitives are inlay by default, but one can inlay class so long as we are ok with copying trivially or by method
# public members can be reflected by map: members [ object ]
##

import a-silver-module [ a-non-const: enum-from-module ]


string operator + [ i:int, a:string ]
    return '{ a } and { i }'

# context is used when the function is called; as such, the user should have these variables themselves
# this reduces the payload on lambdas to zero, at expense of keeping membership explicit
int some-callback[ int i; Vulkan ctx ]
    print[ '{ctx}: {i}' ]
    return i + ctx ? ctx.a-member : 0

# methods can exist at module-level, or record.
nice: some-callback[ null ]

string a-function [ string a ]
    i : 2 + sz[ a ]
    r : nice[ i ]
    print[ 'called-with: %s. returning: { r }'  a ]
    return r

int a-function [ a:string ]
    return 2 + sz[ a ]

class app
    public short value   = 1
    intern int something = 2

    string make-string [ int from ]
        return 'a string with { from }'

    cast int[]
        my-func  = ref run
        r:int[ my-func[ 'hi' ] ?? run ] # value or default
        s:make-string[ r ]
        return len[ s ]
    
    int run[ arg:string ]
        print['{ arg } ... call own indexing method: { this[ 2 ] }']
        return 1

int module-name[ a:app ]
    is-const : int[ a ]  # : denotes constant assignment, this calls the cast above
    val = is-const       # = assignment [mutable]
    val += 1             # we may change the val, because it was declared with =

    print[ 'using app with value: { is-const } + { val - is-const }' ]
    return (run[a, string[val]] > 0) ? 1 : 0

```



