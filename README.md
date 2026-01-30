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
import KhronosGroup:Vulkan-Tools/main
	-DVULKAN_HEADERS_INSTALL_DIR={install}

# lets include C models, and even type/value macros
# note: macros with matching type definitions are registered as type aliases
import <stdio.h> 

class app of main
    required msg:string

	func init[] -> none
		however: "its args need not be"
		printf["calling a C function with a const string: %s",
            'we only allow formatter args with const-string.. {however} ']

```

Design-time AI watchers generate and cache non-deterministic outputs, keeping 
authored code separate from LLM-generated dictation while integrating naturally 
into modules and media.

```python

globals_not_reassignable: "simple way to define primitive constants"

import IEEE:product/version
    --config-with={globals_not_reassignable}

# native generator-based component packaged in silver runtime
# will be able to checkout open models as well
import chatgpt
	model:'gpt-5'

class our-model [ based ]
    name:  string
    scale: double

    # these dictations are stored in a folder of the module's stem name (module.ag -> module/our-model.a-method.ai)
    # this content is used to fill in the method name; in doing so we separate waht is machine-made vs human
    # however the human always defines the models, method prototypes, and arguments
    func a-method[ append_to_name:string, scale_increment:f64 ] using chatgpt -> int
        [ 'take a look at this image for inspiration; make this update the state based on the argments', image[ 'resource-image.png' ] ]
        [ 'never allow the name to be more than 10 characters' ] # we append more and save the file in watch-mode to update the cached .ai file
```

silver’s founding vision is an open, decentralized ecosystem where cross-compiled toolchains, reflective models, and human-AI co-creation converge into a unified development experience.

development in progress, with documentation to be added/changed.

Update: context addition to release 88:
### Context-Aware Members

In silver we describe members that implicitly **pull from context** without requiring explicit passing by user, unless with intent.
Inherently required arguments, context represents a second level of public member, one that is implicit and allows for syntax reduction

If the context is not available and the user does not specify it, the compiler will require them to either provide it.  Passing null for an object is following that rule.

This enables natural, readable code with less boilerplate.  When the user does describe syntax, it is with clearer intention.

```python

linux ?? import wayland-protocols from https://gitlab.freedesktop.org/wayland/wayland-protocols 810f1adaf33521cc55fc510566efba2a1418174f

import KhronosGroup:Vulkan-Headers/main
    <vulkan/vulkan.h>


# short-hand for git shared git repo -- a good basis for 'web4' data
# its git provided, so it costs little to host, and we have our identities as url basis. with project/version, what else could democratize user provided media better in open?
import KhronosGroup:Vulkan-Tools/main
	-DVULKAN_HEADERS_INSTALL_DIR={install}
	{linux ?? -DWAYLAND_PROTOCOLS_DIR={install}/checkout/wayland-protocols}

# globals assigned at design-time, and cannot be re-assigned
# globals in effect are our constants when in primitive form
# cache from lists, maps and other classes remain mutable
# the reason for class-only mutable policy is to combat 
# manual state [mis]management with primitives
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
                pApplicationInfo
                    sType              : VK_STRUCTURE_TYPE_APPLICATION_INFO
                    pApplicationName   : "trinity"
                    applicationVersion : VK_MAKE_VERSION(1, 0, 0)
                    pEngineName        : 'trinity-v{version}'
                    engineVersion      : VK_MAKE_VERSION(const i64[ first[version] ], const i64[ last[version] ], 0)
                    apiVersion         : vk_version
                
            ], null, instance ]

        # methods at expr-level 0 do not invoke with [ these ] unless the function is variable argument
        verify result == VK_SUCCESS, 'could not start vulkan {major}.{minor}'

class Window
    context vk:   Vulkan
    public  size: shape

globals-not-reassignable: 22

class test_vulkan [ app ]
    public  queue_family_index: array i64[2x4] [ 2 2 2 2, 4 4 4 4 ]
    intern  an_intern_member:   i64 4
    context an-instance:        Vulkan
    intern  window:             Window
    public  shared:             'a-string initialized with {globals-not-reassignable}'

    func init[] -> none
        an-instance = Vulkan[ major:1  minor:1 ]
        window = Window
            size: 444x888

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
