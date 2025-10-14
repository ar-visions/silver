silver is r/Futurology related, because this topic was primary when designing the language.  It is unlike anything done in build workflows now at the moment, because the compiler is being given the builder role by the user with language.  It is a build language first.  We just ask what the cool architects do, and they seem to produce SDK environments.  so we build and maintain this as users of silver.
sure, the future is about less code, but that doesn't mean you aren't the modeler.

import started with reading models from building other silver modules and importing the models. C interoperability is essential to get anywhere, so it can import <list, of, headers> so silver understands both the types and macros(with, args), type-based macros (translated into model and usable as types in silver). Also, we allow macros that merely token-replace still, and it supports a C-compatility mode in this replacement context. this is so we most of C language into silver's model scheme.

# C imports from header (C++ support in 1.0 release)
import <stdio.h>

# software dependency projects consist of url and commit-id tokens
# we dont invent a package manager when the world is one already
import <vulkan/vulkan.h> [ https://github.com/KhronosGroup/Vulkan-Headers main ]

# we call our llm agent for design-time reference:
import agent [ https://huggingface.co/meta-llama/Llama-3-8b ]
	max_new_tokens: 50
	temperature: 0.8
	top_p: 0.95
	repetition_penalty: 1.1

something: true

if something
	class hello
        state: i32[ 1 ]

        ai elaborate[ n:i32, b:i32 ] -> i32
            "write something elaborate here using i32 inputs state (on object accessible by a.state), n and b"
            "no i want it more elaborate than that"
            "i give up, just add state, n and b and return the result"
        
		fn print[ a:hello, msg: string ]
            # call elaborate and give that to printf along with the string arg
    		printf[ "hello: %s : %i", msg, elaborate[ 22, 22 ] ]

# using main indicates we're building an app, not a library
# effectively creating a new subclass of main called app
main app
    fn init[]
        if something
            s: hello
            hello.print[ s, "this is a message" ]
        else
            puts[ "if you dont want to do something lets do nothing." ]
            puts[ "test failed.  something done" ]
            exit 1


certainly, users will use llm to write methods, however users define the models the data it works with.  consulting with an ai can also help establish the models, but users does this in a proprietary fashion.  it's pretty important for the user to define this model and maintain it; this leads to better ownership and maintenance.  we have to be good for 'something'.




llm with weights accessible by url identifier.  .hugging-face.sf for hugging-face are private hidden and untracked files for you facilitate build access.  we build cmake, auto-config, meson, rust and others that use a simple makefile only. when needed, we indent our import with --build-args=x   ... or,  > shell commands with {silver} design-time accessible module properties.

in regular software import, its build arguments with optional commands provided above or below. 

using > or >>, the command is pre build or post build.  the important thing is this build fabric is woven into your actual source module.  since we are build language first, we have a  basis of reproducing the exact software result on all platforms if we confine those steps to this place, our actual software language: silver


we dictate the app, but we still have a means of coding in this context.  dictation is merely a keyword on methods giving your own llm import name; for it's content, you indent for lines of > dictation.  this content supports media references as well as text.

to use to extend upon it, and to be given context when we want to AI to build on top of it.  These AI dictations are separate translation units, and we generate them by specifying an import of the llm, along with the url to the resource and build instructions accessible by > console commands supporting $(shell)
As a build system, silver is in the right place to be a top level native application language, because it's building all of the software we need using the developers actual build system -- from git, mirrored and cached no where, it's not a language that hosts its own package manager, you are the manager however you may do it more succinctly with url and commit-id tokens.  We are a build system first, and then create modules when there is code available.
we help democratize software by way of direct exposure to git urls, and not a costly and inaccessible central authority.

so far, this is somehow relatively novel in a language.

import is used for all modules, including app, lib, silver module, or C header associated with a project-url and commit.  it is also used to import llm for code generation.  silver merely defines a protocol which may be extended upon by the user; however to use llm successfully, bf16 is recommended using models over 70b.  it is also recommended to use open-data models so the entire process can be audited by community.

llms are for design-time code generation, and one we may cache along with our dictation whacks done under the import as separate token entries separated by > ... same as import's install command script.

to compose full stack, we should write code natively, potentially using the same module for all contexts.  This is the case now for use of the trinity app framework, which produces a canvas-driven app, a 3D one, or command-line text-output.  There's no reason why we can't use the same code for all targets here, and development can be more maintainable in the process.

for module level members, one can use the if statement the same as we use in functions.  It's use in the global scope is effectively design-time.   this, so definitions can be made in different build context.

Using LLVM in C makes it easy to write operations; this is somewhat the same to rust's usage.  We go a bit further deeper in by importing C headers, and plan on importing C++ for the 1.0 release. 

we use clang 22 for 'building' code (silvers products are IR translated to executable for the target platform by default)

silver types (enum, class, struct) models are laid out using instances of the model class in Aether, this is Au's abstract for llvm.  Au types are universal-objects in C/C++/silver.  silver simply produces models based on this format.  this to the end of letting software interoperate a bit easier.

Importantly the object model here is universal, and we have ability to use it in C and C++, with rusty shims expected after.

we want to be different from rust, which is why I chose silver as a name here in Tucson in 2017 and designed the memory layout based on Au. 

It tries to blend the best software development practices in app;  this so we can start developing and maintaing software easier.  llm dictation is performed for methods with arguments and scope defined by the user.  We need not leave out our ability to write a competing function with this to replace or benchmark against.  If two are defined, it will not build with llm


so there is an alternative in open source with you controlling the packages, these are simply defined as buildable software on the internet.  we don't need to spend resources on this problem to reduce user-freedom and build support over time.  It's not a 'rebel' decision here, it's the correct decision to allow more user freedom on this.  If you publish software, you publish to all of these moronic app stores and in the future that will be built in to orbiter, the ide part of an open app imported from url, being written in silver.

silver module members are set by import parameters potentially if we are importing silver.  otherwise import can be a given a direct token output for the build args without the need to escape with quotes.  the language makes use of a raw token type for specific keywords, to ease peoples minds; we merely use +1 indent for rules.  You just follow your own indent rules and thats the definition.

These are free floating blocks similar to python but a bit more for lenient.  we try to take the best parts from all languages, with a C expression syntax without as many parenthesis

backing out of partial C++ integration for now to produce a meaningful release that integrates reflection of all of your members and their meta information (array of type identifiers)

the future is about the individual shining as bright as possible, and we do this with dictation in public and private domains.  We do this with more syntactical freedom, and less platform constraint.  We target all devices and contexts, 