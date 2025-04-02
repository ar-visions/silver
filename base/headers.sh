#!/bin/bash

silver="$(cd "$(dirname "$0")" && pwd)"

IMPORT_HEADER="$1"
PROJECT_HEADER="$2"
GEN_DIR="$(dirname $1)"
PROJECT="$3"
UPROJECT="$4"

#IMPORT_HEADER="$GEN_DIR/import"
METHODS_HEADER="$GEN_DIR/$PROJECT-methods"
INTERN_HEADER="$GEN_DIR/$PROJECT-intern"
PUBLIC_HEADER="$GEN_DIR/$PROJECT-public"
INIT_HEADER="$GEN_DIR/$PROJECT-init"

SED=${SED:-sed}

shift 4
IMPORTS=("$@")

# regen check
# add silver rule to base.mk, so if silver is newer, headers are re-generated
if [ ! -f "$IMPORT_HEADER" ]; then
    rm -f "$IMPORT_HEADER"

    echo "/* generated import interface */" >> "$IMPORT_HEADER"
    echo "#ifndef _${UPROJECT}_IMPORT_${PROJECT}_" >> "$IMPORT_HEADER"
    echo "#define _${UPROJECT}_IMPORT_${PROJECT}_" >> "$IMPORT_HEADER"
    echo "" >> "$IMPORT_HEADER"

    echo "/* imports: ${IMPORTS[*]} */" >> "$IMPORT_HEADER"

    for import in "${IMPORTS[@]}"; do
        if [ "$import" != "$PROJECT" ]; then
            if [ -f "$SILVER_IMPORT/include/${import}-methods" ]; then
                echo "#include <${import}-public> // this was from {import}" >> "$IMPORT_HEADER"
            else
                echo "// #include <${import}-public> // has no $SILVER_IMPORT/include/${import}-methods" >> "$IMPORT_HEADER"
            fi
        fi
    done

    echo "#include <${PROJECT}-intern> // line 52 uses {PROJECT}" >> "$IMPORT_HEADER"
    echo "#include <${PROJECT}>" >> "$IMPORT_HEADER"

    #for name in $LIB_MODULES; do
    #    base=$(basename "$name")
    #    echo "#include <${base%.*}>" >> "$IMPORT_HEADER"
    #done

    echo "#include <${PROJECT}-methods>" >> "$IMPORT_HEADER"
    echo "#include <A-reserve>" >> "$IMPORT_HEADER"

    # must have PROJECT-init's last! ... these define macros that have the same name as the type, so we cannot process our types through macro expansion with these in place
    # it also makes it not possible to define macros for types within our modules without manual header sequencing between them
    for import in "${IMPORTS[@]}"; do
        if [ "$import" != "$PROJECT" ]; then
            if [ -f "$SILVER_IMPORT/include/${import}-methods" ]; then
                echo "#include <${import}-init>" >> "$IMPORT_HEADER"
            fi
        fi
    done

    echo "#include <${PROJECT}-init>" >> "$IMPORT_HEADER"
    echo "" >> "$IMPORT_HEADER"
    echo "#endif" >> "$IMPORT_HEADER"
fi


# Only regenerate if necessary
if [ ! -f "$INIT_HEADER" ] || [ "$PROJECT_HEADER" -nt "$INIT_HEADER" ]; then
    # Start the file with header comments and include guards
    rm -f "$INIT_HEADER"
    echo "/* generated methods interface */" > "$INIT_HEADER"
    echo "#ifndef _${UPROJECT}_INIT_H_" >> "$INIT_HEADER"
    echo "#define _${UPROJECT}_INIT_H_" >> "$INIT_HEADER"
    echo >> "$INIT_HEADER"
    
    # Process class/mod/meta/vector declarations
    grep -o 'declare_\(class\|mod\|meta\|vector\)[[:space:]]*([[:space:]]*[^,)]*' "$PROJECT_HEADER" | \
    $SED -E 's/declare_(class|mod|meta|vector)[[:space:]]*\([[:space:]]*([^,)]*)[[:space:]]*(,[[:space:]]*([^,)[:space:]]*))?/\1 \2 \4/' | \
    while read type class_name arg; do
        if [ -z "${class_name}" ]; then
            continue
        fi
        
        # Debug definitions
        echo "#ifndef NDEBUG" >> "$INIT_HEADER"
        echo "    //#define TC_${class_name}(MEMBER, VALUE) A_validate_type(VALUE, A_member(isa(instance), A_TYPE_PROP|A_TYPE_INTERN|A_TYPE_PRIV, #MEMBER)->type)" >> "$INIT_HEADER"
        echo "    #define TC_${class_name}(MEMBER, VALUE) VALUE" >> "$INIT_HEADER"
        echo "#else" >> "$INIT_HEADER"
        echo "    #define TC_${class_name}(MEMBER, VALUE) VALUE" >> "$INIT_HEADER"
        echo "#endif" >> "$INIT_HEADER"
        
        # Variadic argument counting macros
        echo "#define _ARG_COUNT_IMPL_${class_name}(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N" >> "$INIT_HEADER"
        echo "#define _ARG_COUNT_I_${class_name}(...) _ARG_COUNT_IMPL_${class_name}(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)" >> "$INIT_HEADER"
        echo "#define _ARG_COUNT_${class_name}(...)   _ARG_COUNT_I_${class_name}(\"A-type\", ## __VA_ARGS__)" >> "$INIT_HEADER"
        
        # Combination macros
        echo "#define _COMBINE_${class_name}_(A, B)   A##B" >> "$INIT_HEADER"
        echo "#define _COMBINE_${class_name}(A, B)    _COMBINE_${class_name}_(A, B)" >> "$INIT_HEADER"
        
        # Argument handling macros
        echo "#define _N_ARGS_${class_name}_0( TYPE)" >> "$INIT_HEADER"
        
        if [ "$type" = "meta" ] || [ "$type" = "vector" ]; then
            echo "#define _N_ARGS_${class_name}_1( TYPE, a)" >> "$INIT_HEADER"
        else
            echo "#define _N_ARGS_${class_name}_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, object) const void *: (void)0)(instance, a)" >> "$INIT_HEADER"
        fi
        
        # Property assignment macros for various argument counts
        echo "#define _N_ARGS_${class_name}_2( TYPE, a,b) instance->a = TC_${class_name}(a,b);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_4( TYPE, a,b, c,d) _N_ARGS_${class_name}_2(TYPE, a,b) instance->c = TC_${class_name}(c,d);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_6( TYPE, a,b, c,d, e,f) _N_ARGS_${class_name}_4(TYPE, a,b, c,d) instance->e = TC_${class_name}(e,f);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_8( TYPE, a,b, c,d, e,f, g,h) _N_ARGS_${class_name}_6(TYPE, a,b, c,d, e,f) instance->g = TC_${class_name}(g,h);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_10(TYPE, a,b, c,d, e,f, g,h, i,j) _N_ARGS_${class_name}_8(TYPE, a,b, c,d, e,f, g,h) instance->i = TC_${class_name}(i,j);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_12(TYPE, a,b, c,d, e,f, g,h, i,j, l,m) _N_ARGS_${class_name}_10(TYPE, a,b, c,d, e,f, g,h, i,j) instance->l = TC_${class_name}(l,m);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_14(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o) _N_ARGS_${class_name}_12(TYPE, a,b, c,d, e,f, g,h, i,j, l,m) instance->n = TC_${class_name}(n,o);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_16(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q) _N_ARGS_${class_name}_14(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o) instance->p = TC_${class_name}(p,q);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_18(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s) _N_ARGS_${class_name}_16(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q) instance->r = TC_${class_name}(r,s);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_20(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s, t,u) _N_ARGS_${class_name}_18(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s) instance->t = TC_${class_name}(t,u);" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}_22(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s, t,u, v,w) _N_ARGS_${class_name}_20(TYPE, a,b, c,d, e,f, g,h, i,j, l,m, n,o, p,q, r,s, t,u) instance->v = TC_${class_name}(v,w);" >> "$INIT_HEADER"
        
        # Helper macros
        echo "#define _N_ARGS_HELPER2_${class_name}(TYPE, N, ...)  _COMBINE_${class_name}(_N_ARGS_${class_name}_, N)(TYPE, ## __VA_ARGS__)" >> "$INIT_HEADER"
        echo "#define _N_ARGS_${class_name}(TYPE,...)    _N_ARGS_HELPER2_${class_name}(TYPE, _ARG_COUNT_${class_name}(__VA_ARGS__), ## __VA_ARGS__)" >> "$INIT_HEADER"
        
        # Main constructor macro
        echo "#define ${class_name}(...) ({ \\" >> "$INIT_HEADER"
        echo "    ${class_name} instance = (${class_name})A_alloc(typeid(${class_name}), 1, true); \\" >> "$INIT_HEADER"
        echo "    _N_ARGS_${class_name}(${class_name}, ## __VA_ARGS__); \\" >> "$INIT_HEADER"
        echo "    A_initialize((object)instance); \\" >> "$INIT_HEADER"
        echo "    instance; \\" >> "$INIT_HEADER"
        echo "})" >> "$INIT_HEADER"
    done
    
    # Process struct declarations
    grep -o 'declare_struct[[:space:]]*([[:space:]]*[^,)]*' "$PROJECT_HEADER" | \
    $SED -E 's/declare_struct[[:space:]]*\([[:space:]]*([^,)]*)[[:space:]]*(,[[:space:]]*([^,)[:space:]]*))?/\1 \3/' | \
    while read struct_name arg; do
        if [ -z "${struct_name}" ]; then
            continue
        fi
        
        echo "#define ${struct_name}(...) structure(${struct_name} __VA_OPT__(,) __VA_ARGS__) _N_STRUCT_ARGS(${struct_name}, __VA_ARGS__);" >> "$INIT_HEADER"
    done
    
    # Close the include guard
    echo >> "$INIT_HEADER"
    echo "#endif /* _${UPROJECT}_INIT_H_ */" >> "$INIT_HEADER"
    
    echo "Successfully generated $INIT_HEADER"
fi


# Only regenerate if necessary
if [ ! -f "$METHODS_HEADER" ] || [ "$PROJECT_HEADER" -nt "$METHODS_HEADER" ]; then
    rm -f "$METHODS_HEADER"
    echo "/* generated methods interface */" > "$METHODS_HEADER"
    echo "#ifndef _${UPROJECT}_METHODS_H_" >> "$METHODS_HEADER"
    echo "#define _${UPROJECT}_METHODS_H_" >> "$METHODS_HEADER"
    echo >> "$METHODS_HEADER"
    # Process method declarations (these apply to classes only, not structs; structs also cannot be poly)
    grep -o 'i_method[[:space:]]*([^,]*,[^,]*,[^,]*,[^,]*,[[:space:]]*[[:alnum:]_]*' "$PROJECT_HEADER" | \
    $SED 's/i_method[[:space:]]*([^,]*,[^,]*,[^,]*,[^,]*,[[:space:]]*\([[:alnum:]_]*\).*/\1/' | \
    while read method; do
        if [ -n "$method" ]; then
            echo "#undef $method" >> "$METHODS_HEADER"
            echo "#define $method(I,...) ({ __typeof__(I) _i_ = I; ftableI(_i_)->$method(_i_, ## __VA_ARGS__); })" >> "$METHODS_HEADER"
        fi
    done
    # Close the include guard
    echo >> "$METHODS_HEADER"
    echo "#endif /* _${UPROJECT}_METHODS_H_ */" >> "$METHODS_HEADER"
    
    echo "Successfully generated $METHODS_HEADER"
fi


# Only regenerate if necessary
if [ ! -f "$PUBLIC_HEADER" ] || [ "$PROJECT_HEADER" -nt "$PUBLIC_HEADER" ]; then
    # Start the file with header comments and include guards
    rm -f "$PUBLIC_HEADER"
    echo "/* generated methods interface */" > "$PUBLIC_HEADER"
    echo "#ifndef _${UPROJECT}_PUBLIC_"         >> "$PUBLIC_HEADER"
    echo "#define _${UPROJECT}_PUBLIC_"         >> "$PUBLIC_HEADER"
    echo >> "$PUBLIC_HEADER"

    # Iterate through all three types using a single loop
    for type in "class" "mod" "meta" "struct"; do
        grep -o "declare_${type}[[:space:]]*([[:space:]]*[^,)]*" "$PROJECT_HEADER" | \
        $SED "s/declare_${type}[[:space:]]*([[:space:]]*\([^,)]*\).*/\1/" | \
        while read class_name; do
            if [ -n "$class_name" ]; then
                echo "#ifndef ${class_name}_intern" >> "$PUBLIC_HEADER"
                echo "#define ${class_name}_intern(AA,YY,...) AA##_schema(AA,YY##_EXTERN, __VA_ARGS__)" >> "$PUBLIC_HEADER"
                echo "#endif" >> "$PUBLIC_HEADER"
            fi
        done
        echo >> "$PUBLIC_HEADER"
    done

    echo "#include <${PROJECT}>"                >> "$PUBLIC_HEADER" ; \
    #echo "#include <${PROJECT}-init>"           >> "$PUBLIC_HEADER" ; \
    echo "#include <${PROJECT}-methods>"        >> "$PUBLIC_HEADER" ; \
    echo "#endif /* _${UPROJECT}_PUBLIC_ */"    >> "$PUBLIC_HEADER"
fi


# Only regenerate if necessary
if [ ! -f "$INTERN_HEADER" ] || [ "$PROJECT_HEADER" -nt "$INTERN_HEADER" ]; then
    # Start the file with header comments and include guards
    rm -f "$INTERN_HEADER"
    echo "/* generated methods interface */" > "$INTERN_HEADER"
    echo "#ifndef _${UPROJECT}_INTERN_H_" >> "$INTERN_HEADER"
    echo "#define _${UPROJECT}_INTERN_H_" >> "$INTERN_HEADER"
    echo >> "$INTERN_HEADER"
    
    # Iterate through all three types using a single loop
    for type in "class" "mod" "meta"; do
        grep -o "declare_${type}[[:space:]]*([[:space:]]*[^,)]*" "$PROJECT_HEADER" | \
        $SED "s/declare_${type}[[:space:]]*([[:space:]]*\([^,)]*\).*/\1/" | \
        while read class_name; do
            if [ -n "$class_name" ]; then
                echo "#undef ${class_name}_intern" >> "$INTERN_HEADER"
                echo "#define ${class_name}_intern(AA,YY,...) AA##_schema(AA,YY, __VA_ARGS__)" >> "$INTERN_HEADER"
            fi
        done
        
        # Add a newline after each type section
        echo >> "$INTERN_HEADER"
    done
    
    echo "#endif /* _${UPROJECT}_INTERN_H_ */" >> "$INTERN_HEADER"
fi