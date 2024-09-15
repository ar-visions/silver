#!/bin/bash
(
    # parse args
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --src)
                SRC_DIR="$2"
                shift 2
                ;;
            *)
                BUILD_ROOT="$1"
                shift
                ;;
        esac
    done

    SRC_DIR="${SRC_DIR:-$SRC}"
    BUILD_ROOT="${BUILD_ROOT:-silver-build}"
   #BUILD_ROOT="${1:-silver-build}"

    projects=(
        "A   https://github.com/ar-visions/A.git"
    )

    # we can imagine multiple projects sharing the same build root
    # thats possible if its already set, it can filter
    # down from cmake's binary elder directory
    SCRIPT_DIR=$(dirname "$(realpath "$0")")
    cd      $SCRIPT_DIR || exit 1
    mkdir -p "$BUILD_ROOT" || exit 1
    cd       "$BUILD_ROOT"
    mkdir -p checkout
    cd       checkout

    # iterate through projects, cloning and building
    for project in "${projects[@]}"; do
        name=$(echo "$project" | awk '{print $1}')
        REPO_STRING=$(echo "$project" | awk '{print $2}')
        TARGET_DIR="${name}"
        REPO_URL="${REPO_STRING%@*}"      # get the part before @ (repository URL)
        CHECKOUT_ID="${REPO_STRING##*@}"  # get the part after  @ (checkout ID)

        # if there's no @ in the string, CHECKOUT_ID will be the same as the URL, so we reset it
        if [ "$CHECKOUT_ID" = "$REPO_URL" ]; then
            CHECKOUT_ID=""
        fi

        # Check if --src directory and project exist, then symlink
        if [ -n "$SRC_DIR" ] && [ -d "$SRC_DIR/$TARGET_DIR" ]; then
            rm -rf "$TARGET_DIR"
            echo "symlinking $SRC_DIR/$TARGET_DIR to $TARGET_DIR..."
            ln -s "$SRC_DIR/$TARGET_DIR" "$TARGET_DIR" || exit 1
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
                        rm -f "silver-build/silver-token" || { echo "silver-token failed to rm" >&2; exit 1; }
                    fi
                fi
            else
                echo "cloning repository $REPO_URL into $TARGET_DIR..."
                git clone "$REPO_URL" "$TARGET_DIR"
                if [ $? -ne 0 ]; then
                    echo "clone failed for $TARGET_DIR"
                    exit 1
                fi
                cd "$TARGET_DIR"
            fi
            # check out the specific commit, branch, or tag if provided
            if [ -n "$CHECKOUT_ID" ]; then
                echo "checking out $CHECKOUT_ID for $TARGET_DIR..."
                git checkout "$CHECKOUT_ID"
                if [ $? -ne 0 ]; then
                    echo "checkout failed for $TARGET_DIR at $CHECKOUT_ID"
                    exit 1
                fi
            fi
        fi

        mkdir -p silver-build
        cd silver-build

        if [ ! -f "silver-token" ] || find .. -type f -newer "silver-token" | grep -q . ; then
            cmake -S .. -B . -DCMAKE_INSTALL_PREFIX="$BUILD_ROOT" -DCMAKE_BUILD_TYPE=Debug
            if [ $? -ne 0 ]; then
                echo "cmake gen failed for $TARGET_DIR"
                exit 1
            fi

            cmake --build .
            if [ $? -ne 0 ]; then
                echo "build failure for $TARGET_DIR"
                exit 1
            fi

            cmake --install .
            if [ $? -ne 0 ]; then
                echo "install failure for $TARGET_DIR"
                exit 1
            fi
            
            echo "im a token" >> silver-token  # only create this if all steps succeed
        fi

        # Move back to checkout directory before starting the next project
        cd ../../
    done
)
