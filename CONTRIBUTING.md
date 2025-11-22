
# CONTRIBUTING.md for the silver Project

Accepting people looking to commit open modeling to the public, to facilitate open-development with source/binary-built platforms targeting all devices.

## Welcome to the silver project!

We are building a highly reflective, reduced-token programming language, an associated build system (Au), a Vulkan-based 3D UX SDK (trinity), and the Orbiter IDE that integrates AI at design time.
The projects around silver welcome volunteers who are excited about systems programming, language design, and graphics development.
Getting Started
The project is hosted on github: ar-visions/silver.

## Design patterns
To get started, contributors should be comfortable with:

    C Programming: The core foundation uses C and polymorphic macros for object modeling (no worries!  its about as usable as python).

    Systems Programming Concepts: Familiarity with memory management, reference counting, and C header imports in spirit of a open-standards approach.

    Build Systems: Understanding decentralized, git-built packages and cross-compilation (specifically LLVM) is a major asset.  

---
    The build systems we support are as many we may define.  Current support includes Autotools, Make, Meson, Google's GN, CMake and rust cargo.  The scope here is to build everything we use ourselves, and as-such we build CMake and other software from scratch.  All build systems mentioned must be included with import statement in our .g files associated to Au's open make system.  With a few lines of code you can summon it for your project, and start describing your own imports.  You will never stop.