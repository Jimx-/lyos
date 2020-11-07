#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_EVERYTHING:=false}
: ${BUILD_XORG_MACROS:=false}
: ${BUILD_LIBDRM:=false}
: ${BUILD_KMSCUBE:=false}
: ${BUILD_LIBEXPAT:=false}
: ${BUILD_LIBFFI:=false}
: ${BUILD_WAYLAND:=false}
: ${BUILD_WAYLAND_PROTOCOLS:=false}
: ${BUILD_MESA:=false}
: ${BUILD_FREETYPE:=false}
: ${BUILD_PIXMAN:=false}
: ${BUILD_CAIRO:=false}
: ${BUILD_LIBXKBCOMMON:=false}
: ${BUILD_LIBTSM:=false}
: ${BUILD_KMSCON:=false}

if $BUILD_EVERYTHING; then
    BUILD_XORG_MACROS=true
    BUILD_LIBDRM=true
fi

echo "Building X11... (sysroot: $SYSROOT, prefix: $PREFIX, crossprefix: $CROSSPREFIX, target: $TARGET)"

if [ ! -d "build" ]; then
    mkdir build
fi

pushd build > /dev/null

. $DIR/activate.sh

# Build xorg-util-macros
if $BUILD_XORG_MACROS; then
    if [ ! -d "host-util-macros-$SUBARCH" ]; then
        mkdir host-util-macros-$SUBARCH
    fi

    pushd host-util-macros-$SUBARCH > /dev/null
    $DIR/sources/util-macros-1.19.1/configure --prefix=$PREFIX || cmd_error
    make || cmd_error
    make install || cmd_error
    popd > /dev/null

    ln -sf $DIR/local/share/aclocal/* $DIR/tools/automake-1.11/share/aclocal-1.11/
    ln -sf $DIR/local/share/aclocal/* $DIR/tools/automake-1.15/share/aclocal-1.15/
fi

# Build libdrm
if $BUILD_LIBDRM; then
    if [ ! -d "libdrm-$SUBARCH" ]; then
        mkdir libdrm-$SUBARCH
    fi

    pushd $DIR/sources/libdrm-2.4.89 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libdrm-$SUBARCH > /dev/null
    $DIR/sources/libdrm-2.4.89/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --disable-intel --disable-vmwgfx --disable-radeon --disable-amdgpu --disable-nouveau --disable-cairo-tests
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

    pushd wayland-$SUBARCH > /dev/null
    meson --cross-file ../../meson.cross-file --prefix=/usr --libdir=lib --buildtype=debugoptimized -Ddocumentation=false -Ddtd_validation=false $DIR/sources/wayland-1.18.0
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

# Build wayland-protocols
if $BUILD_WAYLAND_PROTOCOLS; then
    if [ ! -d "wayland-protocols-$SUBARCH" ]; then
        mkdir wayland-protocols-$SUBARCH
    fi

    pushd $DIR/sources/wayland-protocols-1.20 > /dev/null
    # ./autogen.sh
    popd > /dev/null

    pushd wayland-protocols-$SUBARCH > /dev/null
    $DIR/sources/wayland-protocols-1.20/configure --host=$TARGET --prefix=/usr
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
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

# Build kmscube
if $BUILD_KMSCUBE; then
    if [ ! -d "kmscube-$SUBARCH" ]; then
        mkdir kmscube-$SUBARCH
    fi

    pushd $DIR/sources/kmscube > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH ./autogen.sh
    make distclean
    popd > /dev/null

    pushd kmscube-$SUBARCH > /dev/null
    $DIR/sources/kmscube/configure --host=$TARGET --prefix=/usr
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build freetype
if $BUILD_FREETYPE; then
    if [ ! -d "freetype-$SUBARCH" ]; then
        mkdir freetype-$SUBARCH
    fi

    pushd freetype-$SUBARCH > /dev/null
    $DIR/sources/freetype-2.10.2/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT --disable-static
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build pixman
if $BUILD_PIXMAN; then
    if [ ! -d "pixman-$SUBARCH" ]; then
        mkdir pixman-$SUBARCH
    fi

    pushd pixman-$SUBARCH > /dev/null
    $DIR/sources/pixman-0.40.0/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build cairo
if $BUILD_CAIRO; then
    if [ ! -d "cairo-$SUBARCH" ]; then
        mkdir cairo-$SUBARCH
    fi

    pushd cairo-$SUBARCH > /dev/null
    $DIR/sources/cairo-1.16.0/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT --disable-xlib
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

if $BUILD_LIBXKBCOMMON; then
    if [ ! -d "libxkbcommon-$SUBARCH" ]; then
        mkdir libxkbcommon-$SUBARCH
    fi

    pushd libxkbcommon-$SUBARCH > /dev/null
    meson --cross-file ../../meson.cross-file --prefix=/usr --libdir=lib --buildtype=debugoptimized -Denable-x11=false -Denable-docs=false $DIR/sources/libxkbcommon-1.0.1
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

# Build libtsm
if $BUILD_LIBTSM; then
    if [ ! -d "libtsm-$SUBARCH" ]; then
        mkdir libtsm-$SUBARCH
    fi

    pushd $DIR/sources/libtsm-3 > /dev/null
    mkdir -p m4
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fis
    popd > /dev/null

    pushd libtsm-$SUBARCH > /dev/null
    $DIR/sources/libtsm-3/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build kmscon
if $BUILD_KMSCON; then
    if [ ! -d "kmscon-$SUBARCH" ]; then
        mkdir kmscon-$SUBARCH
    fi

    pushd kmscon-$SUBARCH > /dev/null
    $DIR/sources/kmscon-8/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT \
                                    --with-video=drm2d --with-renderers= --with-fonts= \
                                    --disable-multi-seat --with-sessions=dummy
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
