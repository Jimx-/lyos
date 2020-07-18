#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_LIBDRM:=false}

echo "Building X11... (sysroot: $SYSROOT, prefix: $PREFIX, crossprefix: $CROSSPREFIX, target: $TARGET)"

if [ ! -d "build" ]; then
    mkdir build
fi

pushd build > /dev/null

. $DIR/activate.sh

# Build libdrm
if $BUILD_LIBDRM; then
    if [ ! -d "libdrm-$SUBARCH" ]; then
        mkdir libdrm-$SUBARCH
    fi

    pushd $DIR/sources/libdrm-2.4.89 > /dev/null
    autoreconf
    popd > /dev/null

    pushd libdrm-$SUBARCH > /dev/null
    $DIR/sources/libdrm-2.4.89/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT --disable-intel --disable-vmwgfx --disable-radeon --disable-amdgpu --disable-nouveau --disable-cairo-tests
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
