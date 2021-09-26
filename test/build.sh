TEST_SOURCES="src/load_file.c"
LIB_SOURCES="../src/**.c"
FLAGS="-lm -std=c99 -Wall -Wextra -Wpedantic"
INCLUDES="-I../include"

OPTIMIZE_FLAGS="-g -DDEBUG"

# handle arguments
if [ $# == 1 ]; then
    if [ $1 == "fast" ]; then
        OPTIMIZE_FLAGS="-O3 -DNDEBUG"
    else
        echo "unknown arg 1!"
        exit -1
    fi
elif [ $# -gt 1 ]; then
    echo "too many args! $# $1 $2"
    exit -1
fi

# compile
i=1

for SOURCE in $TEST_SOURCES; do
    echo "COMPILING $SOURCE"

    LC_ALL=C gcc $SOURCE $LIB_SOURCES $FLAGS $OPTIMIZE_FLAGS $INCLUDES -o bin/$i

    ((++i))
done
