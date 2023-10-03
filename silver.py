import sys

'''
# ------------------------------------------------
# Silver Programming Language
# ------------------------------------------------
# C99 objects from module definitions; no globals, 
# no using or import keyword or any of that stuff you repeat
# ------------------------------------------------

# var is the base type as it were, its just the instance of anything.  
# if its a primtive type then we still have it virtually-defined, based on var
module attrib {
    str     name
    var     value
}

# shell script / python comments
module Object {
    xyz     pos
}

# there should be a default, null instance for each type
# zero-mem is undefined, nullable not actual null-token
# no 'return' keyword (use -> ); void an error (we dont define a void type introspectively), semi-colon an error

# this is the data that incorporates it
module Character base Object {
    str         name
    attrib[]    attrs

    # static defined as less things, static member on a module, and that is all, 
    # we dont have a concept of global space for members
    static str static_value = "1"
    
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

    # () parens in context of args are optional for 0 arg methods

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

    # if there is arguments, it is a dedicated constructor not set with args not amounting to the names given

    # what is important is the ability to have undefined restricted to unset space.  the user cannot directly effect these bits aside
    # from setting them.  they cannot unset.  null yes, but not unsetting with an undefined.
    # this is important for integrity of knowing things were unset by the constructing user

    # only 1 constructor is allowed, they need not have parenthesis; those are defined as fields in our module
    # if you have constructor specific fields then they should be only seen in the construction space

    # this is true middle-ware.  C is low level and everyone knew that since day one

    # asm blocks are important but i believe we want an additional case for languages used (so we can enumerate implementation)
    

    construct (int value, str another) {
        if not set(name) {
            # "double-quotes" are opaque to variables
            # char is a kind of run-time type that takes in 'these' and when they are 'x' or single 
            # const-u8 then you get a u8, then when you define 'a string' you get formatting 
            # facility as optional branch.
            name = "could set a default above instead.."
        } else {
            # show off a formatter syntax and change an input
            name = '{str(name.len)}'
        }

        # no breaks in switches i think.
        # also it makes sense to make asm : switch(arch)
        asm {
            case arch.x86:
                mov eax ebx # move ebx to eax
                cmp eax ecx
                mov var_name eax
            
            case arch.x86_64:
                mov eax ebx # move ebx to eax
                cmp eax ecx
                add eax ecx
                mov var_name eax
                
            default:
                int c = 1 # implicit should be the { block here because we have no fall-through }
                var_name = c + 1
        }

    }
}

'''

# open the file for reading (replace 'filename.txt' with your file's name)
src_file = sys.argv[1]

with open(src_file, 'r') as file:
    # read the entire content of silver.sv source
    src = file.read()

    # split the content based on newline character ('\n')
    lines = src.split('\n')

    # process each line or perform operations as needed
    for line in lines:
        print(line) # print each line
