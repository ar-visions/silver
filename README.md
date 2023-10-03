# Silver Programming Language
C99 objects from module definitions; no globals.

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
mod Character inc Object {
    str         name
    Object[]    attribs

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
    define macro[x, y] {
        if (x > 1)
            -> x + 1
        else
            -> x + 2
    }

    # convert to string (if explicit)
    explicit str -> name

    # initialization
    this {
        if undef(name) {
            # "double-quotes" are opaque to variables
            # 'char' is now used for formatting; char("A") gives you character values; char is int32; cast to 8bit if you want ascii data
            # honor the world with character and all of its codepages
            name = "could set a default above instead.."
        } else {
            # doing something extra i dont need to just to show off a formatter; this is inline C99 at the expression depth we can put it
            name = '{name}'
        }
    }
}


