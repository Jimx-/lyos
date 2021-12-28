#!/usr/bin/env bash
#
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

export PATH=$DIR/local/bin:$PATH
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

export PKG_CONFIG_PATH_FOR_BUILD="$PREFIX/lib/pkgconfig"

# Generate pkgconfig wrapper
echo "#!/bin/sh

export PKG_CONFIG_PATH=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}

exec pkg-config \"\$@\"" > $DIR/local/bin/$TARGET-pkg-config
chmod +x $DIR/local/bin/$TARGET-pkg-config

if [ ! -d $DIR/cross-file ]; then
    mkdir -p $DIR/cross-files
fi

# Generate CMake toolchain file
echo "set(CMAKE_SYSTEM_NAME lyos)

set(CMAKE_FIND_ROOT_PATH $SYSROOT)

set(CMAKE_C_COMPILER $TARGET-gcc)
set(CMAKE_CXX_COMPILER $TARGET-g++)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# flags for shared libraries
set(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG \"-Wl,-soname,\")
set(CMAKE_PLATFORM_USES_PATH_WHEN_NO_SONAME 1)" > $DIR/cross-files/$TARGET-CMakeToolchain.txt
export TARGET_CMAKE_TOOLCHAIN_FILE=$DIR/cross-files/$TARGET-CMakeToolchain.txt

export MESON_CROSS_FILE=$DIR/meson-$SUBARCH.cross-file
