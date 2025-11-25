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
	{linux ?? -DWAYLAND_PROTOCOLS_DIR={install}/checkout/wayland-protocols}

# the above are lines typically found in a Makefile, or two -- 
# however with silver we wanted the language to build a software install that can define in code.
# this is keeping with Au's .g technique for graphing dependencies per compile-unit.

import <stdio.h> # lets include C models, and even type/value macros

# note: macros with matching type definitions are registered as type aliases, similar to how Swift imports types from C

# main is a hard-coded abstract class which sets properties by their 
main app
    # this lambda is called as an exception printer, of sort.  otherwise public/intern members may be called to construct their own defaults
    # please notice this happens in the order of the membership, and please don't think about that too much.
    required msg:string -> fault 'you have to start with hello'

    # to keep initialization even more standard, we can do everything by overloading init in one method
	override fn init[] -> none
        # we may omit '-> none' by convention.  types are expressed by given return-type of call model, or a simple type name
        # this is not remotely complicated because we do not interface with C++ yet (will probably require type[ expr ])

		however = "its args need not be" # const-string, instanced as design-time literal with the use of = over :
		puts[ "calling a C function with a const string: %s", string[' we only allow formatter args with const-string.. {however} ']]

		print[ usage ] # an au-type registered va-arg function, registered by reflection into design-time
			###
			# once a module has reached compilable state, it may use its own models at design-time, making adaptations to code we output without macros.
			# silver is here because there is a need to express a kind of model we have a real-control of.
			  its runtime environment is all c code, and it has the exact same models and capabilities as silver. the worlds are all one.
              using silver as build tool alone, is quite useful in that it's a cross platform, CMake/Makefile/Ninja replacement.
              we're still commented because of the double # above ^
			##

			# rust is quite good but we need more open way of expressing software,
			# that is, decentralized, independent, git-domain projects as first class imports, its nothing but user:project/branch

			# silver has a tokens type, so the user can effectively take off with their own expressions that allow or disallow unix tools to be used within.
			# tokens and shell_tokens are the different types that take-in a string.
			# when your constructor is keyword level, we can express tokens directly into code, with {expressions} possible in both
			# for zero expression expansion, we use basic_tokens



```


Design-time AI watchers generate and cache non-deterministic outputs, keeping authored code separate from LLM-generated dictation while integrating naturally into modules and media.


```python
# this is a native component that receives an import construction, thus creating a component-decentral model for implementing keywords in silver
# in the case of chatgpt, it hooks into function body generation using a 'using' keyword
import chatgpt
	model:'gpt-5'

# chatgpt takes over design-time in that these requested data types are converted for you, when valid
# with silver we are still programming by taking in these members into a function.
# for this mode, any tokens are first passed to the chatgpt component.  chatgpt returns a path when valid
fn do-something[] -> image using chatgpt [ publish:image ]
	return { please draw me cuddly lion }

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


string operator + [ i:int, a:string ] -> string
    return '{ a } and { i }'

# context is used when the function is called; as such, the user should have these variables themselves
# this reduces the payload on lambdas to zero, at expense of keeping membership explicit
fn some-callback[ i:int, context ctx:Vulkan ] -> int
    print[ '{ctx}: {i}' ]
    return i + ctx ? ctx.a-member : 0

# methods can exist at module-level, or record.

nice: some-callback[ 'a-nice-callback', null ]

fn a-function [ a:string ]  -> string
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
    is-const = int[ a ]  # : denotes constant assignment, this calls the cast above
    val : is-const      # = assignment [mutable]

    val += 1 # we may change the val, because it was declared with :

    print[ 'using app with value: { is-const } + { val - is-const }' ]
    return [ run[a, string[val]] > 0 ] ? 1 : 0

```



