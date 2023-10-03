# Silver Programming Language
C99 objects from module definitions; no globals.  No casting either, named args most certainly because we want it to look like reflective web development with type driven context.  Silver seems to be the most reflective and succinct way of getting there.  Reduce, and improve is the idea here.  Base on C99, so it compiles fast and compiles everywhere.

# object is a generic type.. may use var.
mod attrib {
    str     name
    var     value
}

# shell script / python comments
mod Object {
    xyz pos
}

# there should be a default, null instance for each type
# zero-mem is undefined, nullable not actual null-token

# no 'return' keyword; void optional

# this is the data that incorporates it
module Character depends Object {
    str         name
    Object[]    attribs

    static str  static_value = "1"          # static defined as less things, static member on a module, and that is all.
    
    # we dont have statics inside of functions! thats not allowed, and thus static is restricted to member domain (one large part of the langauge)


    # module incorporates class and namespace.  this is important to be able to get rid of both to simplify with one name!
    # oh and we can lose 30 cpp keywords.  this is opposite of problem

    # the auto is where templates come in; auto a keyword you
    # would want to use when you wnat generics of optional! [type, restrictions]
    int method(auto[int, short] a, int b) {
        if (typeof(a) == int)
            -> b + a
        else
            -> b + int(a) * 2
    }

    # consider this a text replacement with a bit more it can do
    # crucially its not enforcing a global token replacement, and 
    # REQUIRES [] invocation, and is NOTICABLE as different from function
    # supporting only expression case: (immediate return)
    # intend to support block return case with multiple return paths per temporary
    define macro(x, y) -> x + y + 2

    # convert to string (if explicit)
    explicit str -> name

    # initialization
    # there is no reason to have an args with props, as they have been set already
    # what is important is the ability to have undefined restricted to unset space.  the user cannot directly effect these bits aside
    # from setting them.  they cannot unset.  null yes, but not unsetting with an undefined.  this is important for integrity.

    this {
        if not set(name) {
            # "double-quotes" are opaque to variables
            # 'char' is now used for formatting; char("A") gives you character values; char is int32; cast to 8bit if you want ascii data
            # honor the world with character and all of its codepages
            name = "could set a default above instead.."
        } else {
            # show off a formatter syntax and change an input
            name = '{str(name.len)}'
        }
    }
}


