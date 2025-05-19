DIR_ROOT="${PWD}"
DIR_SCRIPTS="${DIR_ROOT}/docker/sanitizers"
DIR_EXTERNAL="${DIR_ROOT}/deps"
DIR_SRCS_EXT="${DIR_EXTERNAL}/srcs"
DIR_BUILD_EXT="${DIR_EXTERNAL}/cmake.bld"

DIR_SRC_BMQ="${DIR_ROOT}"
DIR_BUILD_BMQ="${DIR_SRC_BMQ}/cmake.bld/Linux"


# Set some initial constants
PARALLELISM=8

# Parse sanitizers config
cfgquery() {
    jq "${1}" "${DIR_SCRIPTS}/sanitizers.json" --raw-output
}

TOOLCHAIN_PATH="${DIR_SCRIPTS}/clang-libcxx-sanitizer.cmake"

# Setup CMake options for all remaining builds
CMAKE_OPTIONS=( \
    -D BUILD_BITNESS=64 \
    -D CMAKE_BUILD_TYPE=Debug \
    -D CMAKE_INSTALL_INCLUDEDIR=include \
    -D CMAKE_INSTALL_LIBDIR=lib64 \
    -D CMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_PATH}")
