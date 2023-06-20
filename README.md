# Silver Programming Language
Silver is to be a transpiled language to start out. 
C++17 output is likely for now; but C99 is end goal.  Investigating GNU Bison prior to doing that optimization though.

Roadmap!

## Design Features in Works

The main features we envision for Silver are:

### Function Uniformity
Functions in Silver should behave the same way everywhere, regardless of their context. This standardization of behavior will simplify the language, making it more understandable and user-friendly.

### Improved Template System
Silver aims to have an enhanced template system. We're learning from the challenges and successes of C++ templates, but our goal is to create a system that is more intuitive to use and provides better compile-time error messages.

### Powerful Macro System
In Silver, macros will have the capacity to generate code that can be inspected by the compiler. This feature is inspired by Lisp macros, but will be adapted to work with C++-like syntax, making it easier to create and modify complex code structures at compile time.

### Enhanced Generic Programming
Silver is designed to support powerful and flexible generic programming. While we draw inspiration from C++, our aim is to make generic programming simpler and safer in Silver.

### Integrated Metaprogramming
Metaprogramming capabilities will be an integral part of Silver. Instead of being added as an afterthought, these capabilities will be designed into the language from the start.

### Optimized Allocation in Functions
Silver will provide a way to define functions that can optimize their memory allocation behavior, allowing for increased performance and efficiency.

### Unique Static Inside Functions
The language will allow for the definition of unique statics inside functions that are generated per call. This will create more opportunities for optimization, making your programs run faster and use resources more efficiently.

using ux;

int    i = 2
future f = i.call_http_method() # lol, ... just showing the wonderful stupidity possible in the design-mode

# i just want more to be possible in programming, and more common ways of doing it.  just work, of course.. just read and just write.
# meaning matters and letting the user be as succinct as possible lets the meaning of code come through.

# python style comments only, no semi colon
alias int = bc;

if (typeof(bc) != typeof(
