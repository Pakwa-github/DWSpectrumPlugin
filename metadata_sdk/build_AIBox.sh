#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

# 帮助信息（复用你的逻辑，保持官方注释风格）
if [[ $# > 0 && ($1 == "/?" || $1 == "-h" || $1 == "--help") ]]
then
    echo "Usage: $(basename "$0") [--no-tests] [--debug] [<cmake-generation-args>...]"
    echo " --debug Compile using Debug configuration (without optimizations) instead of Release."
    echo " --no-tests Skip unit tests (Although no tests for AIBox plugin now)"
    exit
fi

BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"
PLUGIN_NAME="AIBox_plugin"
SOURCE_DIR="$BASE_DIR/src/$PLUGIN_NAME"

if [[ $# > 0 && $1 == "--no-tests" ]]
then
    shift
    NO_TESTS=1
else
    NO_TESTS=1
fi

if [[ $# > 0 && $1 == "--debug" ]]
then
    shift
    BUILD_TYPE=Debug
else
    BUILD_TYPE=Release
fi

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        GEN_OPTIONS=( -GNinja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_C_COMPILER=cl.exe )
        BASE_DIR=$(cygpath -w "$BASE_DIR") #< Windows-native cmake requires Windows path.
        BUILD_OPTIONS=()
        if [[ $BUILD_TYPE == Release ]]
        then
            BUILD_OPTIONS+=( --config $BUILD_TYPE )
        fi
        ;;
    *) # Assume Linux; use Ninja if it is available on PATH.
        if which ninja >/dev/null
        then
            GEN_OPTIONS=( -GNinja ) #< Generate for Ninja and gcc; Ninja uses all CPU cores.
            BUILD_OPTIONS=()
        else
            GEN_OPTIONS=() #< Generate for GNU make and gcc.
            BUILD_OPTIONS=( -- -j ) #< Use all available CPU cores.
        fi
        if [[ $BUILD_TYPE == Release ]]
        then
            GEN_OPTIONS+=( -DCMAKE_BUILD_TYPE=$BUILD_TYPE )
        fi
        ;;
esac

(set -x #< Log each command.
    rm -rf "$BUILD_DIR/"
)

PLUGIN="$PLUGIN_NAME"
(set -x #< Log each command.
    mkdir -p "$BUILD_DIR/$PLUGIN"
    cd "$BUILD_DIR/$PLUGIN"

    cmake "$SOURCE_DIR" `# allow empty array #` ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} "$@"
    cmake --build . `# allow empty array #` ${BUILD_OPTIONS[@]+"${BUILD_OPTIONS[@]}"}
)

if [[ $PLUGIN == "unit_tests" ]]
then
    ARTIFACT=$(find "$BUILD_DIR" -name "analytics_plugin_ut.exe" -o -name "analytics_plugin_ut")
else
    ARTIFACT=$(find "$BUILD_DIR" -name "$PLUGIN.dll" -o -name "lib$PLUGIN.so")
fi
if [ ! -f "$ARTIFACT" ]
then
    echo "ERROR: Failed to build $PLUGIN."
    exit 64
fi
echo ""
echo "Built: $ARTIFACT"
echo ""

if [[ $NO_TESTS == 1 ]]
then
    echo "NOTE: Unit tests were not run (AIBox plugin has no tests)."
else
    (set -x #< Log each command.
        cd "$BUILD_DIR/unit_tests"
        ctest --output-on-failure -C $BUILD_TYPE
    )
fi
echo ""

echo "AIBox plugin built successfully!"
echo "See the binary in: $BUILD_DIR/$PLUGIN"