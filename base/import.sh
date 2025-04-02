#!/bin/bash
(
    # built projects from environment
    AVOID_PROJECTS="${BUILT_PROJECTS:-}"
    CALLER_BUILD_DIR=""

    # Set the NPROC_CMD variable based on the operating system
    if [ "$(uname)" = "Darwin" ]; then
        NPROC="sysctl -n hw.ncpu"
        STAT="gstat"
    else
        NPROC="nproc"
        STAT="stat"
    fi

    # parse args
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --src)
                SRC_DIR="$2"
                shift 2
                ;;
            --b)
                CALLER_BUILD_DIR="$2"
                shift 2
                ;;
            --i)
                PROJECT_IMPORT="$2"
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done

    if [ -z "$PROJECT_IMPORT" ]; then
        echo '--i required argument (import file)'
        exit
    fi

    if [ -z "$CALLER_BUILD_DIR" ]; then
        echo '--b required argument (project build dir)'
        exit
    fi

    BUILD_DIR=$CALLER_BUILD_DIR

    print() {
        if [ "${VERBOSE}" = "1" ]; then
            echo "$@"
        fi
    }

    if [ ! -f "$CALLER_BUILD_DIR/.rebuild" ]; then
        touch $CALLER_BUILD_DIR/.rebuild
    fi

    projects=()  # Initialize an empty array
    found_deps=1
    current_line=""

    while IFS= read -r raw_line || [ -n "$raw_line" ]; do
        line=$(echo "$raw_line" | tr '\t' ' ' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        if [ -z "$line" ]; then
            continue
        fi
        if [[ $line =~ ^[[:alnum:]][[:alnum:]]*: ]]; then
            found_deps=0
        fi
        if [[ $found_deps -eq 1 ]]; then
            ws=$(echo "$raw_line" | tr '\t' ' ' | sed -E 's/^( *).*/\1/' | wc -c)
            if [ $ws -le 1 ]; then
                if [ ! -z "$current_line" ]; then
                    projects+=("$current_line")
                    print "import: $current_line"
                fi
                current_line="$line"
            elif [ $ws -ge 2 ]; then
                current_line="$current_line $line"
            fi
            
        fi
    done < $PROJECT_IMPORT

    # Add the last current_line if it exists
    if [ ! -z "$current_line" ]; then
        projects+=("$current_line")
    fi

    SRC_DIR="${SRC_DIR:-$SRC}"

    if [ "${VERBOSE}" = "1" ]; then
        MFLAGS=""
    else
        MFLAGS="-s"
    fi

    if [ -z "$DEB" ]; then
        DEB=","
    fi

    SCRIPT_DIR=$(dirname "$(realpath "$0")")
    cd      $SCRIPT_DIR || exit 1
    mkdir -p "$SILVER_IMPORT" || exit 1
    cd       "$SILVER_IMPORT"
    mkdir -p checkout
    cd       checkout

    OUR_PROJECTS=""
    for project in "${projects[@]}"; do
        PROJECT_NAME=$(echo "$project" | cut -d ' ' -f 1)
        OUR_PROJECTS="$OUR_PROJECTS,$PROJECT_NAME"
    done

    # iterate through projects, cloning and building
    for project in "${projects[@]}"; do
        PROJECT_NAME=$(echo "$project" | cut -d ' ' -f 1)

        # check if project already built during this command stack
        if [[ ",$AVOID_PROJECTS," == *",$PROJECT_NAME,"* ]]; then
            print "skipping $PROJECT_NAME - already built"
            continue
        fi

        REPO_URL=$(echo "$project" | cut -d ' ' -f 2)
        IS_GCLIENT=

        if [[ "$REPO_URL" == *"googlesource.com/"* ]]; then
            # checkout depot_tools [todo: only do this if we use a gclient-based project
            if [ ! -d "depot_tools" ]; then
                git clone 'https://chromium.googlesource.com/chromium/tools/depot_tools.git'
            fi
            if [[ "$PATH" != *"/depot_tools"* ]]; then
                export PATH="${PWD}/depot_tools:${PATH}"
            fi
            IS_GCLIENT=1
        fi

        COMMIT_RAW=$( echo "$project" | cut -d ' ' -f 3)
        COMMIT=$(echo "$COMMIT_RAW" | sed -e 's/^!//')
        IS_RECUR=$(echo "$COMMIT_RAW" | grep -q '^!' && echo true || echo false)
        BUILD_CONFIG_RAW=$(echo "$project" | cut -d ' ' -f 4-)
        BUILD_CONFIG=""

        (
        for item in $BUILD_CONFIG_RAW; do
            # Check if the item contains a $ENV_VAR
            res=$(eval 'echo "$item"')
            # if there is a env:var
            if [[ $res =~ ^[[:alnum:]_]+[[:space:]]*[:] ]]; then
                env_name="${res%%:*}"
                env_value="${res#*:}"
                export "$env_name=$env_value"
                echo "set environment: ${env_name} to ${env_value}"
                continue # this is not added to build config, this is an environment var we set (and stay in for this project iteration)
            fi

            # Handle `>` commands
            if [[ $res == ">"* ]]; then
                # If we have a stored command, add it to the list
                if [[ -n "$command_buffer" ]]; then
                    COMMAND_LIST+=("$command_buffer")
                fi

                # start new command, strip >
                command_buffer="${res#>}"
                command_mode=1
                continue
            fi

            # append command if we are inside one
            if [[ $command_mode -eq 1 ]]; then
                command_buffer+=" $res"
                continue
            fi

            # append the resolved item to the processed_config
            BUILD_CONFIG="$BUILD_CONFIG $res"
        done

        # Add the last command if one was being collected
        if [[ -n "$command_buffer" ]]; then
            COMMAND_LIST+=("$command_buffer")
        fi

        # Trim leading whitespace
        BUILD_CONFIG=$(echo "$BUILD_CONFIG" | sed 's/^ *//')
        TARGET_DIR="${PROJECT_NAME}"
        A_MAKE="0" # A-type projects use Make, but with a build-folder and no configuration; DEBUG=1 to enable debugging
        IS_DEBUG=
        rust=""
        cmake=""
        BUILD_TYPE=""
        silver_build=""
        IS_RESOURCE=
        
        # set build folder based on release/debug

        if [[ ",$DBG," == *",$PROJECT_NAME,"* ]]; then
            BUILD_FOLDER="debug"
            IS_DEBUG=1
        else
            BUILD_FOLDER="build"
        fi
        
        # Check if --src directory and project exist, then symlink
        if [ -n "$SRC_DIR" ] && [ -d "$SRC_DIR/$TARGET_DIR" ]; then
            SYMLINK_TARGET=$(readlink "$TARGET_DIR")
            if [ "$SYMLINK_TARGET" != "$SRC_DIR/$TARGET_DIR" ]; then
                rm -rf "$TARGET_DIR"
                echo "symlinking $TARGET_DIR to $SRC_DIR/$TARGET_DIR..."
                ln -s "$SRC_DIR/$TARGET_DIR" "$TARGET_DIR" || exit 1
            fi
            cd "$TARGET_DIR"
        else
            if [ -d "$TARGET_DIR" ]; then
                cd "$TARGET_DIR" || exit 1
                if [ "$PULL" == "1" ]; then
                    echo "pulling latest changes for $TARGET_DIR..."
                    PULL_HASH_0=$(git rev-parse HEAD)
                    git pull || { echo "git pull failed, exiting." >&2; exit 1; }
                    PULL_HASH_1=$(git rev-parse HEAD)
                    if [ "$PULL_HASH_0" != "$PULL_HASH_1" ]; then
                        rm -f "$BUILD_FOLDER/silver-token" || { echo "silver-token failed to rm" >&2; exit 1; }
                    fi
                fi
            else
                echo "cloning repository $REPO_URL into $TARGET_DIR..."
                #echo git clone "$REPO_URL" "$TARGET_DIR"
                #git clone "$REPO_URL" "$TARGET_DIR"
                #echo git clone --recursive "$REPO_URL" "$TARGET_DIR"
                if [ -n "$IS_GCLIENT" ]; then
                    echo "fetching $TARGET_DIR ... (PWD = $PWD)"
                    fetch --force $TARGET_DIR
                elif [ "$IS_RECUR" == "true" ]; then
                    git clone --recursive "$REPO_URL" "$TARGET_DIR"
                else
                    git clone "$REPO_URL" "$TARGET_DIR"
                fi
                
                if [ $? -ne 0 ]; then
                    echo "clone failed for $TARGET_DIR"
                    exit 1
                    break
                fi
                cd "$TARGET_DIR"
            fi
            if [ -f silver-init.sh ]; then
                echo 'running silver-init.sh'
                bash silver-init.sh
            fi

            # check out the specific commit, branch, or tag if provided
            if [ -n "$COMMIT" ]; then
                CURRENT_COMMIT=$(git rev-parse HEAD)
                
                # Calculate the length of the target commit hash
                COMMIT_LENGTH=${#COMMIT}
                
                # Extract the same number of characters from the current commit
                CURRENT_COMMIT_PREFIX=${CURRENT_COMMIT:0:$COMMIT_LENGTH}
                if [ "$CURRENT_COMMIT_PREFIX" != "$COMMIT" ]; then
                    echo "checking out $COMMIT for $TARGET_DIR"
                    git checkout "$COMMIT"
                    if [ $? -ne 0 ]; then
                        echo "checkout failed for $TARGET_DIR at $COMMIT"
                        exit 1
                    fi
                    if [ -n "$IS_GCLIENT" ]; then
                        gclient sync -D
                    fi
                fi
            fi
        fi

        if [ -n "$IS_DEBUG" ]; then
            build="debug"
        else
            build="release"
        fi

        # check if this is a cmake project, otherwise use autotools
        # or if there is "-S " in BUILD_CONFIG
        if [ -n "$IS_GCLIENT" ]; then
            #echo "[$PROJECT_NAME] gclient"
            IS_GCLIENT_="1"
        elif [ -f "silver-build.sh" ]; then
            silver_build="1"
            echo "[$PROJECT_NAME] build"
        elif [ -f "CMakeLists.txt" ] || [[ "$BUILD_CONFIG" == *-S* ]]; then
            cmake="1"
            if [[ ",$DEB," == *",$PROJECT_NAME,"* ]]; then
                print "selecting DEBUG for $PROJECT_NAME"
                BUILD_TYPE="-DCMAKE_BUILD_TYPE=Debug"
            else
                print "selecting RELEASE for $PROJECT_NAME"
                BUILD_TYPE="-DCMAKE_BUILD_TYPE=Release"
            fi
        elif [ -f "Cargo.toml" ]; then
            rust="1"
        else
            if [ -f "configure.ac" ] || [ -f "config" ] ||[ -f "configure.in" ] || [ -f "configure" ]; then
                if [[ ",$DEB," == *",$PROJECT_NAME,"* ]]; then
                    BUILD_TYPE="--enable-debug"
                fi
            elif [ -f "silver" ]; then
                A_MAKE="1"
                BUILD_TYPE="-g"
            elif [ -f "Makefile" ]; then
                #echo "regular Makefile for $PROJECT_NAME"
                NO_OPERATION="1"
            else
                echo "resource project: $PROJECT_NAME"
                IS_RESOURCE=1
            fi
        fi
        
        if [ -n "$IS_RESOURCE" ]; then
            COMMANDS=""
            for command in "${COMMAND_LIST[@]}"; do
                echo "$PROJECT_NAME:command > $command\n"
                COMMANDS="${COMMANDS}${command}; "
            done
            bash -c "${COMMANDS}"
            exit 0
        fi

        mkdir -p $BUILD_FOLDER
        cd $BUILD_FOLDER
        BUILT=

        if [ -n "$silver_build" ]; then
            # arbitrary build process is sometimes needed; some repos have no Make, etc
            echo 'running silver-build.sh'
            (cd .. && bash silver-build.sh)
            BUILT=1
        else
            if [ "$REBUILD" == "all" ] || [ "$REBUILD" == "$PROJECT_NAME" ]; then
                echo "rebuilding ${TARGET_DIR}"
                if [ -n "$IS_GCLIENT" ]; then
                    ninja -C . -t clean
                elif [ -n "$cmake" ]; then
                    cmake --build . --target clean
                elif [ -n "$rust" ]; then
                    cargo clean
                elif [ "$A_MAKE" = "1" ]; then
                    make $MFLAGS -f ../Makefile clean
                else
                    make clean
                fi
                rm -rf silver-token
            fi

            # we must do this every time for A-type projects
            if [ -d "../res" ] && [ -f "../silver" ]; then
                echo "$PROJECT_NAME: copying resource files $BUILD_DIR"
                rsync -av --checksum --update ../res/* "$BUILD_DIR"
            fi
            
            # proceed if there is a newer file than silver-token, or no silver-token
            if [ ! -f "silver-token" ] || find .. -type f -newer "silver-token" | grep -q . ; then
                BUILT="1"

                # we need more than this: it needs to also check if a dependency registered to this project changes
                # to do that, we should run the import on the project without the make process
                BUILD_CONFIG=$(echo "$BUILD_CONFIG" | sed "s|\$SILVER_IMPORT|$SILVER_IMPORT|g")
                BUILD_CONFIG=$(echo "$BUILD_CONFIG" | sed ':a;N;$!ba;s/\n/ /g' | sed 's/[[:space:]]*$//')

                if [ -n "$IS_GCLIENT" ]; then
                    cd ..
                    python3 tools/git-sync-deps # may need to be a command in import if this differs between gclient projects
                    BUILD_CONFIG="'$BUILD_CONFIG"
                    BUILD_CONFIG="$BUILD_CONFIG'"
                    echo "bin/gn gen $BUILD_FOLDER --args=$BUILD_CONFIG"
                    eval bin/gn gen $BUILD_FOLDER --args=$BUILD_CONFIG
                    cd $BUILD_FOLDER
                elif [ -n "$cmake" ]; then
                    if [ ! -f "CMakeCache.txt" ] || [ "$SILVER_IMPORT" -nt "silver-token" ]; then
                        BUILD_CONFIG="${BUILD_CONFIG% }"
                        if [ -z "$BUILD_CONFIG" ]; then
                            BUILD_CONFIG="-S .."
                        fi
                        cmake -B . -S .. $BUILD_TYPE $BUILD_CONFIG -DCMAKE_INSTALL_PREFIX="$SILVER_IMPORT" 
                    fi
                elif [ -n "$rust" ]; then
                    rust="$rust"
                elif [ "$A_MAKE" = "1" ]; then
                    print "$PROJECT_NAME: simple Makefile [ no configuration needed ]"
                else
                    if [ ! -f "../configure" ] || [ "$SILVER_IMPORT" -nt "silver-token" ]; then
                        if [ -f "../configure.ac" ]; then
                            if [ ! -f "../configure" ]; then
                                echo "running autoreconf -i in $PROJECT_NAME ($cmake)"
                                autoupdate ..
                                autoreconf -i ..
                            fi
                            if [ ! -f "../configure" ]; then
                                echo "configure script not available after running autoreconf -i"
                                exit 1
                            fi
                            echo "pwd = $(pwd)"
                            echo \033[34m../configure $BUILD_TYPE --prefix=$SILVER_IMPORT $BUILD_CONFIG\033[0m
                            ../configure $BUILD_TYPE --prefix=$SILVER_IMPORT $BUILD_CONFIG
                        else
                            if [ -f "../config" ]; then
                                if [ ! -f "Makefile" ]; then
                                    echo ../config $BUILD_TYPE --prefix=$SILVER_IMPORT $BUILD_CONFIG
                                    ../config $BUILD_TYPE --prefix=$SILVER_IMPORT $BUILD_CONFIG
                                fi
                            else
                                echo "no configure.ac found for $PROJECT_NAME ... will simply call make"
                                #exit 1
                            fi
                        fi
                    fi
                fi

                # works for both cmake and configure
                if [ $? -ne 0 ]; then
                    echo "generate failed for $TARGET_DIR"
                    exit 1
                fi

                # choose j size
                repo_size=$(du -sm .. | cut -f1)
                if [ "$repo_size" -gt 500 ]; then
                    j=8 # llvm takes up a lot of memory/swap when linking (70GB with debug on), as does many large projects
                else
                    j=$(($($NPROC) / 2))
                fi

                export MAKEFLAGS="-j$j --no-print-directory"

                # build
                print "building $TARGET_DIR with -j$j in $BUILD_FOLDER"
                COMPILE_ERROR="import.sh failed on compilation for $PROJECT_NAME"
                
                ts0=$(find . -type f -exec $STAT -c %Y {} + | sort -n | tail -1)
                
                if [ -n "$IS_GCLIENT" ]; then
                    cd ..
                    ninja -C $BUILD_FOLDER
                    cd $BUILD_FOLDER
                    echo "PWD = $PWD"
                elif [ -n "$cmake" ]; then
                    cmake --build . -- -j$j || { echo "$COMPILE_ERROR"; exit 1; }
                elif [ -n "$rust" ]; then
                    cargo build --$build --manifest-path ../Cargo.toml --target-dir . || { echo "$COMPILE_ERROR"; exit 1; }
                elif [ "$A_MAKE" = "1" ]; then
                    export BUILT_PROJECTS="$OUR_PROJECTS"
                    make $MFLAGS -j$j -f ../Makefile || { echo "$COMPILE_ERROR"; exit 1; }
                else
                    cd ..
                    make $MFLAGS -j$j || { echo "$COMPILE_ERROR"; exit 1; }
                    cd $BUILD_FOLDER
                fi
                ts1=$(find . -type f -exec $STAT -c %Y {} + | sort -n | tail -1)
                #echo "-> project $PROJECT_NAME = $ts0 $ts1"
                if [ $? -ne 0 ]; then
                    echo "build failure for $TARGET_DIR"
                    exit 1
                fi

                export MAKEFLAGS="-j1 --no-print-directory"

                # install to silver-import (if it does not build in build folder, let it build once, so we may write a single token file)
                if [ "$ts0" != "$ts1" ] || { [ -z "$ts0" ] && [ -z "$ts1" ]; }; then
                    touch $CALLER_BUILD_DIR/.rebuild
                    echo "$PROJECT_NAME modified: (you should see a rebuild!)"
                    print "install ${TARGET_DIR}"
                    if [ -n "$IS_GCLIENT" ]; then
                        echo "installing gclient-based project $TARGET_DIR"
                        cp *.so *.dll *.dylib $SILVER_IMPORT/lib/ 2>/dev/null
                        # better to use manual -I includes because this indeed varies per GN project
                        # this effectively was ok for skia but not others
                        #rm -rf $IMPORT/include/$TARGET_DIR
                        #mkdir -p $IMPORT/include/$TARGET_DIR
                        #rsync -a --include "*/" --include "*.h" --exclude="*" ../ $IMPORT/include/$TARGET_DIR/
                    elif [ -n "$cmake" ]; then
                        cmake --install .
                    elif [ -n "$rust" ]; then
                        cp -r ./$build/*.so $SILVER_IMPORT/lib/
                    elif [ "$A_MAKE" = "1" ]; then
                        NO_NEED_FOR_A=1
                        #make $MFLAGS -f ../Makefile install
                    else
                        cd ..
                        make $MFLAGS install
                        cd $BUILD_FOLDER
                    fi
                    if [ $? -ne 0 ]; then
                        echo "install failure for $TARGET_DIR"
                        exit 1
                    fi

                    if [ -f ../silver-post.sh ]; then
                        echo "running silver-post.sh $BUILD_FOLDER"
                        (cd .. && bash silver-post.sh $BUILD_FOLDER)
                    fi
                fi
            fi
            echo "im-a-token" >> silver-token  # only create this if all steps succeed
        fi

        if [ -n "$BUILT" ]; then
            COMMANDS=""
            for command in "${COMMAND_LIST[@]}"; do
                echo "$PROJECT_NAME:command > $command\n"
                COMMANDS="${COMMANDS}${command}; "
            done
            bash -c "${COMMANDS}"
        fi
        ) || exit 1

    done
) || exit 1
