#!/bin/bash
# Directories to watch (set via ENV variable)
WATCH_DIRS=${WATCH_DIRS:-""}  # Example: "A vec ai silver ether trinity hyperspace orbiter"

# Check if WATCH_DIRS is set
if [[ -z "$WATCH_DIRS" ]]; then
    echo "Error: WATCH_DIRS environment variable is not set."
    exit 1
fi

echo "watching for changes in: $WATCH_DIRS"

# Function to watch files and trigger build
watch_and_build() {
    local dir="$TAPESTRY/../$1"
    local debug_dir="$dir/debug"

    # Ensure debug directory exists
    if [[ ! -d "$debug_dir" ]]; then
        echo "Skipping: No ./debug directory in $dir"
        return
    fi

    flags="-m -r -e close_write --format %w%f --exclude '.*\.(^.*/debug/.*|.*\.(o|a|so|tmp|swp))'"

    # Start watching the directories
    if [ -d "$dir/app" ] && [ -d "$dir/lib" ]; then
        inotifywait $flags "$dir/app" "$dir/lib" 2>/dev/null
    elif [ -d "$dir/app" ]; then
        inotifywait $flags "$dir/app" 2>/dev/null
    elif [ -d "$dir/lib" ]; then
        inotifywait $flags "$dir/lib" 2>/dev/null
    else
        echo "Neither $dir/app nor $dir/lib exists, skipping..."
        return
    fi | while read file; do
        if [[ "$file" == *.c ]]; then
            echo -e "building $debug_dir..."
            (cd "$debug_dir" && make -f ../Makefile install)
        fi
    done
}

# Watch all directories in parallel
for dir in $WATCH_DIRS; do
    watch_and_build "$dir" &
done

# Keep script running
wait
