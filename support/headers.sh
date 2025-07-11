#!/bin/bash
BUILD_FILE="$PROJECT_PATH/build.sf"
SRC_DIRECTIVE="$PROJECT_PATH/$DIRECTIVE"

echo "src directive = $SRC_DIRECTIVE"

if [ ! -d "$SRC_DIRECTIVE" ]; then
    echo "skipping directive $DIRECTIVE (null-path: $SRC_DIRECTIVE)"
    exit 0
fi

GEN_DIR="$BUILD_PATH/$DIRECTIVE"
UPROJECT=$(echo "$PROJECT" | tr '[:lower:]' '[:upper:]')
SED=${SED:-sed}
IM="$IMPORTS"

if [ "$DIRECTIVE" != "app" ]; then
    for f in "$SRC_DIRECTIVE"/*; do
        if [ -f "$f" ] && [[ "$f" != *.* ]]; then
            module=$(basename "$f")
            if [[ ! " $IM " =~ (^|[[:space:]])"$module"($|[[:space:]]) ]]; then
                IM="$IM $module"
            fi
        fi
    done
fi
IM="$IM $PROJECT"

mkdir -p $GEN_DIR

# these should always be in sync -- on balance it seems easier to complete this here
# if you break the build, it means the install is broken for your dependencies, but thats fine
if [ "$DIRECTIVE" = "src" ]; then
    INSTALL_INCLUDE_DIR="${IMPORT}/include/${PROJECT}"
    rm -rf $INSTALL_INCLUDE_DIR
    ln -s $GEN_DIR $INSTALL_INCLUDE_DIR
fi

# we need to add to imports based on our local directive peers; those with (extension-less files we dupe check add to IMPORTS)
#       $SRC_DIRECTIVE scan for this

# should rebuild if last build failed, i think.
# that may be stored in a file before we make the time stamp, called import-result
# if that is 0 then we may remake it

write_import_header() {
    local DIRECTIVE="$1"
    local BUILD_FILE="$2"
    local PROJECT="$3"
    local UPROJECT="$4"
    local MODULE="$5"
    local IMPORTS="$6"

    IMPORT_HEADER="$BUILD_PATH/$DIRECTIVE/$MODULE/import"
    rm -f "$IMPORT_HEADER" # todo: improve
    mkdir -p $(dirname $IMPORT_HEADER)

    if [ ! -f "$IMPORT_HEADER" ] || [ "$BUILD_FILE" -nt "$IMPORT_HEADER" ]; then
        echo "/* generated import interface for project $PROJECT */" >> "$IMPORT_HEADER"
        echo "#ifndef _${UPROJECT}_IMPORT_${PROJECT}_" >> "$IMPORT_HEADER"
        echo "#define _${UPROJECT}_IMPORT_${PROJECT}_" >> "$IMPORT_HEADER"
        echo "" >> "$IMPORT_HEADER"
        echo "/* imports: $IM <- new imports */" >> "$IMPORT_HEADER"

        for import in $IM; do
            if [ "$import" != "$MODULE" ]; then
                echo "#include <${import}/public>" >> "$IMPORT_HEADER"
            fi
        done
        for import in $IM; do
            if [ "$import" != "$MODULE" ]; then
                echo "#include <${import}/${import}>" >> "$IMPORT_HEADER"
            fi
        done

        echo "#include <${MODULE}/intern>" >> "$IMPORT_HEADER"
        echo "#include <${MODULE}/${MODULE}>" >> "$IMPORT_HEADER"
        echo "#include <${MODULE}/methods>" >> "$IMPORT_HEADER"

        echo "#undef init" >> "$IMPORT_HEADER"
        echo "#undef dealloc" >> "$IMPORT_HEADER"

        for import in $IM; do
            if [ "$import" != "$MODULE" ]; then
                echo "#include <${import}/init>" >> "$IMPORT_HEADER"
            fi
        done

        for import in $IM; do
            if [ "$import" != "$MODULE" ]; then
                echo "#include <${import}/methods>" >> "$IMPORT_HEADER"
            fi
        done

        echo "#include <${MODULE}/init>" >> "$IMPORT_HEADER"
        echo "" >> "$IMPORT_HEADER"
        echo "#endif" >> "$IMPORT_HEADER"

        if [ -f "${SRC_DIRECTIVE}/${MODULE}" ]; then
            INSTALL_MODULE_INCLUDE="${IMPORT}/include/${MODULE}"
            rm -rf $INSTALL_MODULE_INCLUDE
            ln -s "${SRC_DIRECTIVE}/${MODULE}" "${INSTALL_MODULE_INCLUDE}"
        fi
        
    fi
}

if [ "$DIRECTIVE" = "app" ]; then
    write_import_header "$DIRECTIVE" "$BUILD_FILE" "$PROJECT" "$UPROJECT" "$PROJECT" "$IM"
fi

# Find all files with no extension in the directive folder
find "$SRC_DIRECTIVE" -type f ! -name "*.*" | while read -r MOD_HEADER; do
    MODULE=$(basename "$MOD_HEADER")
    UMODULE=$(echo "$MODULE" | tr '[:lower:]' '[:upper:]')

    METHODS_HEADER="$GEN_DIR/$MODULE/methods"
    INTERN_HEADER="$GEN_DIR/$MODULE/intern"
    PUBLIC_HEADER="$GEN_DIR/$MODULE/public"
    INIT_HEADER="$GEN_DIR/$MODULE/init"
    echo "[headers] generating for module: $MODULE"

    if [ "$DIRECTIVE" != "app" ]; then
        write_import_header "$DIRECTIVE" "$BUILD_FILE" "$PROJECT" "$UPROJECT" "$MODULE" "$IM"
    fi
    
    HEADER="$GEN_DIR/$MODULE/$MODULE"
    mkdir -p $(dirname $HEADER)

    if [ ! -f "$HEADER" ] || [ "$MOD_HEADER" -nt "$HEADER" ]; then
        mkdir -p "$GEN_DIR"
        {
            echo "#line 1 \"$MOD_HEADER\""
            cat "$MOD_HEADER"
        } > "$GEN_DIR/temp"
        mv "$GEN_DIR/temp" "$HEADER"
    fi


    # Only regenerate if necessary
    if [ ! -f "$INIT_HEADER" ] || [ "$HEADER" -nt "$INIT_HEADER" ]; then
        # Start the file with header comments and include guards
        rm -f "$INIT_HEADER"
        echo "/* generated methods interface */" > "$INIT_HEADER"
        echo "#ifndef _${MODULE}_INIT_H_" >> "$INIT_HEADER"
        echo "#define _${MODULE}_INIT_H_" >> "$INIT_HEADER"
        echo >> "$INIT_HEADER"
        
        # Process class/mod/meta/vector declarations
        grep -o 'declare_\(class\|class_2\|class_3\|vector\)[[:space:]]*([[:space:]]*[^,)]*' "$HEADER" | \
        
        $SED -E 's/declare_(class|class_2|class_3|vector)[[:space:]]*\([[:space:]]*([^,)]*)[[:space:]]*(,[[:space:]]*([^,)[:space:]]*))?/\1 \2 \4/' | \
        while read type class_name arg; do
            if [ -z "${class_name}" ]; then
                continue
            fi
            
            echo "#define TC_${class_name}(MEMBER, VALUE) ({ (*(u128*)&instance->f) |= ((u128)1) << FIELD_ID(${class_name}, MEMBER); VALUE; })" >> "$INIT_HEADER"

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
                echo "#define _N_ARGS_${class_name}_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, object) A_schema(A, GENERICS, object) const void *: (void)0)(instance, a)" >> "$INIT_HEADER"
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
            echo "    ${class_name} instance = (${class_name})A_alloc_dbg(typeid(${class_name}), 1, __FILE__, __LINE__); \\" >> "$INIT_HEADER"
            #echo "    ${class_name} instance = (${class_name})A_alloc(typeid(${class_name}), 1, true); \\" >> "$INIT_HEADER"
            echo "    _N_ARGS_${class_name}(${class_name}, ## __VA_ARGS__); \\" >> "$INIT_HEADER"
            echo "    A_initialize((object)instance); \\" >> "$INIT_HEADER"
            echo "    instance; \\" >> "$INIT_HEADER"
            echo "})" >> "$INIT_HEADER"
        done
        
        # Process struct declarations
        grep -o 'declare_struct[[:space:]]*([[:space:]]*[^,)]*' "$HEADER" | \
        $SED -E 's/declare_struct[[:space:]]*\([[:space:]]*([^,)]*)[[:space:]]*(,[[:space:]]*([^,)[:space:]]*))?/\1 \3/' | \
        while read struct_name arg; do
            if [ -z "${struct_name}" ]; then
                continue
            fi
            
            echo "#define ${struct_name}(...) structure_of(${struct_name} __VA_OPT__(,) __VA_ARGS__) _N_STRUCT_ARGS(${struct_name}, __VA_ARGS__);" >> "$INIT_HEADER"
        done
        
        # Close the include guard
        echo >> "$INIT_HEADER"
        echo "#endif /* _${MODULE}_INIT_H_ */" >> "$INIT_HEADER"
    fi

    # methods would be nicer if we could listen to their calls along with the argument and returns
    # this is far from easy, though -- it works for individual methods with names like:
    #   method(Type, obj, arg) <-- not sure if we want this throughout, though
    #   its more debuggable to be able to listen to methods, but thats actually only these types, not all
    #   further its quite a lot of conversion and boilerplate to effectively repeat the class name all over the place
    #   methods lose their edge
    if [ ! -f "$METHODS_HEADER" ] || [ "$HEADER" -nt "$METHODS_HEADER" ]; then
        rm -f "$METHODS_HEADER"
        echo "/* generated methods interface */" > "$METHODS_HEADER"
        echo "#ifndef _${MODULE}_METHODS_H_" >> "$METHODS_HEADER"
        echo "#define _${MODULE}_METHODS_H_" >> "$METHODS_HEADER"
        echo >> "$METHODS_HEADER"
        # Process method declarations (these apply to classes only, not structs; structs also cannot be poly)
        TEMP_METHODS=$(mktemp)

        grep -o 'i_guard[[:space:]]*([^,]*,[^,]*,[^,]*,[^,]*,[[:space:]]*[[:alnum:]_]*' "$HEADER" | \
        $SED 's/i_guard[[:space:]]*([^,]*,[^,]*,[^,]*,[^,]*,[[:space:]]*\([[:alnum:]_]*\).*/\1/' | \
        while read method; do
            if [ -n "$method" ] && ! grep -qx "$method" "$TEMP_METHODS"; then
                echo "$method" >> "$TEMP_METHODS"
                echo "#undef $method" >> "$METHODS_HEADER"
                echo "#define $method(I,...) ({ __typeof__(I) _i_ = I; (((A)_i_ != (A)0L) ? ftableI(_i_)->$method(_i_, ## __VA_ARGS__) : (__typeof__(ftableI(_i_)->$method(_i_, ## __VA_ARGS__)))0) ; })" >> "$METHODS_HEADER"
            fi
        done

        grep -o 'i_method[[:space:]]*([^,]*,[^,]*,[^,]*,[^,]*,[[:space:]]*[[:alnum:]_]*' "$HEADER" | \
        $SED 's/i_method[[:space:]]*([^,]*,[^,]*,[^,]*,[^,]*,[[:space:]]*\([[:alnum:]_]*\).*/\1/' | \
        while read method; do
            if [ -n "$method" ] && ! grep -qx "$method" "$TEMP_METHODS"; then
                echo "$method" >> "$TEMP_METHODS"
                echo "#ifndef $method" >> "$METHODS_HEADER"
                echo "#define $method(I,...) ({ __typeof__(I) _i_ = I; ftableI(_i_)->$method(_i_, ## __VA_ARGS__); })" >> "$METHODS_HEADER"
                echo "#endif" >> "$METHODS_HEADER"
            fi
        done

        # Close the include guard
        echo >> "$METHODS_HEADER"
        echo "#endif /* _${MODULE}_METHODS_H_ */" >> "$METHODS_HEADER"
    fi


    # Only regenerate if necessary
    if [ ! -f "$PUBLIC_HEADER" ] || [ "$HEADER" -nt "$PUBLIC_HEADER" ]; then
        # Start the file with header comments and include guards
        rm -f "$PUBLIC_HEADER"
        echo "/* generated methods interface */" > "$PUBLIC_HEADER"
        echo "#ifndef _${MODULE}_PUBLIC_"         >> "$PUBLIC_HEADER"
        echo "#define _${MODULE}_PUBLIC_"         >> "$PUBLIC_HEADER"
        echo >> "$PUBLIC_HEADER"

        # Iterate through all three types using a single loop
        for type in "class" "class_2" "class_3" "class_4" "struct"; do
            grep -o "declare_${type}[[:space:]]*([[:space:]]*[^,)]*" "$HEADER" | \
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

        #echo "#include <${PROJECT}>"                >> "$PUBLIC_HEADER" ; \
        #echo "#include <${PROJECT}-init>"           >> "$PUBLIC_HEADER" ; \
        #echo "#include <${PROJECT}-methods>"        >> "$PUBLIC_HEADER" ; \
        echo "#endif /* _${MODULE}_PUBLIC_ */"    >> "$PUBLIC_HEADER"
    fi

    # Only regenerate if necessary
    if [ ! -f "$INTERN_HEADER" ] || [ "$HEADER" -nt "$INTERN_HEADER" ]; then
        # Start the file with header comments and include guards
        rm -f "$INTERN_HEADER"
        echo "/* generated methods interface */" > "$INTERN_HEADER"
        echo "#ifndef _${MODULE}_INTERN_H_" >> "$INTERN_HEADER"
        echo "#define _${MODULE}_INTERN_H_" >> "$INTERN_HEADER"
        echo >> "$INTERN_HEADER"
        
        # Iterate through all three types using a single loop
        for type in "class" "class_2" "class_3" "class_4"; do
            grep -o "declare_${type}[[:space:]]*([[:space:]]*[^,)]*" "$HEADER" | \
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
        
        echo "#endif /* _${MODULE}_INTERN_H_ */" >> "$INTERN_HEADER"
    fi
done

# return include path
echo "-I${GEN_DIR}"