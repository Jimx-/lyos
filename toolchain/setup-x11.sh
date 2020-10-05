#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_LIBDRM:=false}
: ${BUILD_KMSCUBE:=false}
: ${BUILD_LIBEXPAT:=false}
: ${BUILD_LIBFFI:=false}
: ${BUILD_WAYLAND:=false}
: ${BUILD_MESA:=false}

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

# Build libexpat
if $BUILD_LIBEXPAT; then
    if [ ! -d "libexpat-$SUBARCH" ]; then
        mkdir libexpat-$SUBARCH
    fi

    pushd libexpat-$SUBARCH > /dev/null
    $DIR/sources/expat-2.2.9/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT --without-xmlwf
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libffi
if $BUILD_LIBFFI; then
    if [ ! -d "libffi-$SUBARCH" ]; then
        mkdir libffi-$SUBARCH
    fi

    pushd libffi-$SUBARCH > /dev/null
    $DIR/sources/libffi-3.3/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build wayland
if $BUILD_WAYLAND; then
    if [ ! -d "wayland-$SUBARCH" ]; then
        mkdir wayland-$SUBARCH
    fi

    pushd $DIR/sources/wayland-1.18.0 > /dev/null
    # ./autogen.sh
    popd > /dev/null

    pushd wayland-$SUBARCH > /dev/null
    $DIR/sources/wayland-1.18.0/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT --with-host-scanner --disable-dtd-validation --disable-documentation
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

if $BUILD_MESA; then
    if [ ! -d "mesa-$SUBARCH" ]; then
        mkdir mesa-$SUBARCH
    fi

    pushd mesa-$SUBARCH > /dev/null
    meson --cross-file ../../meson.cross-file --prefix=/usr --libdir=lib --buildtype=debugoptimized -Dglx=disabled -Dplatforms=drm -Ddri-drivers= -Dgallium-drivers=swrast -Dvulkan-drivers= $DIR/sources/mesa-20.0.5
    ninja -v || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

# Build kmscube
if $BUILD_KMSCUBE; then
    if [ ! -d "kmscube-$SUBARCH" ]; then
        mkdir kmscube-$SUBARCH
    fi

    pushd $DIR/sources/kmscube > /dev/null
    # ./autogen.sh
    make distclean
    popd > /dev/null

    pushd kmscube-$SUBARCH > /dev/null
    $DIR/sources/kmscube/configure --host=$TARGET --prefix=/usr
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
