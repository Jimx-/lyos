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
: ${BUILD_XORG_PROTO:=false}
: ${BUILD_XKEYBOARD_CONFIG:=false}
: ${BUILD_LIBINPUT:=false}
: ${BUILD_DEJAVU:=false}
: ${BUILD_LIBXAU:=false}
: ${BUILD_LIBXDMCP:=false}
: ${BUILD_XCB_PROTO:=false}
: ${BUILD_LIBXCB:=false}
: ${BUILD_XTRANS:=false}
: ${BUILD_LIBX11:=false}
: ${BUILD_LIBXFIXES:=false}
: ${BUILD_LIBXRENDER:=false}
: ${BUILD_LIBXCURSOR:=false}
: ${BUILD_LIBXEXT:=false}
: ${BUILD_WESTON:=false}

if $BUILD_EVERYTHING; then
    BUILD_XORG_MACROS=true
    BUILD_LIBDRM=true
    BUILD_LIBEXPAT=true
    BUILD_LIBFFI=true
    BUILD_WAYLAND=true
    BUILD_WAYLAND_PROTOCOLS=true
    BUILD_MESA=true
    BUILD_FREETYPE=true
    BUILD_PIXMAN=true
    BUILD_CAIRO=true
    BUILD_LIBXKBCOMMON=true
    BUILD_LIBTSM=true
    BUILD_KMSCON=true
    BUILD_XORG_PROTO=true
    BUILD_XKEYBOARD_CONFIG=true
    BUILD_LIBINPUT=true
    BUILD_DEJAVU=true
    BUILD_LIBXAU=true
    BUILD_LIBXDMCP=true
    BUILD_XCB_PROTO=true
    BUILD_LIBXCB=true
    BUILD_XTRANS=true
    BUILD_LIBX11=true
    BUILD_LIBXFIXES=true
    BUILD_LIBXRENDER=true
    BUILD_LIBXCURSOR=true
    BUILD_LIBXEXT=true
    BUILD_WESTON=true
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

# Build host xtrans
if $BUILD_XTRANS; then
    if [ ! -d "host-xtrans-$SUBARCH" ]; then
        mkdir host-xtrans-$SUBARCH
    fi

    pushd host-xtrans-$SUBARCH > /dev/null
    $DIR/sources/xtrans-1.4.0/configure --prefix=$PREFIX || cmd_error
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

    pushd $DIR/sources/kmscon-8 > /dev/null
    mkdir -p m4
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd kmscon-$SUBARCH > /dev/null
    $DIR/sources/kmscon-8/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT \
                                    --with-video=drm2d --with-renderers= --with-fonts=unifont \
                                    --disable-multi-seat --with-sessions=dummy,terminal
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build xorg-proto
if $BUILD_XORG_PROTO; then
    if [ ! -d "xorg-proto-$SUBARCH" ]; then
        mkdir xorg-proto-$SUBARCH
    fi

    pushd $DIR/sources/xorgproto-2020.1 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd xorg-proto-$SUBARCH > /dev/null
    $DIR/sources/xorgproto-2020.1/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                                 --sysconfdir=/etc --localstatedir=/var \
                                                 --disable-static

    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build xkeyboard-config
if $BUILD_XKEYBOARD_CONFIG; then
    if [ ! -d "xkeyboard-config-$SUBARCH" ]; then
        mkdir xkeyboard-config-$SUBARCH
    fi

    pushd $DIR/sources/xkeyboard-config-2.30 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd xkeyboard-config-$SUBARCH > /dev/null
    $DIR/sources/xkeyboard-config-2.30/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                                 --sysconfdir=/etc --localstatedir=/var \
                                                 --with-xkb-rules-symlink=xorg --disable-nls \
                                                 --disable-runtime-deps

    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libinput
if $BUILD_LIBINPUT; then
    if [ ! -d "libinput-$SUBARCH" ]; then
        mkdir libinput-$SUBARCH
    fi

    pushd libinput-$SUBARCH > /dev/null
    meson --cross-file ../../meson.cross-file --prefix=$CROSSPREFIX --libdir=lib --buildtype=debugoptimized -Dlibwacom=false -Ddebug-gui=false -Dtests=false -Ddocumentation=false $DIR/sources/libinput-1.10.7
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

# Build dejavu
if $BUILD_DEJAVU; then
    mkdir -p $SYSROOT/usr/share/fonts/truetype/
    cp -rf $DIR/sources/dejavu-fonts-ttf-2.37/ttf $SYSROOT/usr/share/fonts/truetype/dejavu
fi

# Build libXau
if $BUILD_LIBXAU; then
    if [ ! -d "libXau-$SUBARCH" ]; then
        mkdir libXau-$SUBARCH
    fi

    pushd $DIR/sources/libXau-1.0.9 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libXau-$SUBARCH > /dev/null
    $DIR/sources/libXau-1.0.9/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libXdmcp
if $BUILD_LIBXDMCP; then
    if [ ! -d "libXdmcp-$SUBARCH" ]; then
        mkdir libXdmcp-$SUBARCH
    fi

    pushd $DIR/sources/libXdmcp-1.1.3 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libXdmcp-$SUBARCH > /dev/null
    $DIR/sources/libXdmcp-1.1.3/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build xcb-proto
if $BUILD_XCB_PROTO; then
    if [ ! -d "xcb-proto-$SUBARCH" ]; then
        mkdir xcb-proto-$SUBARCH
    fi

    pushd $DIR/sources/xcb-proto-1.14.1 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd xcb-proto-$SUBARCH > /dev/null
    $DIR/sources/xcb-proto-1.14.1/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libxcb
if $BUILD_LIBXCB; then
    if [ ! -d "libxcb-$SUBARCH" ]; then
        mkdir libxcb-$SUBARCH
    fi

    pushd $DIR/sources/libxcb-1.14 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    sed -i 's/pthread-stubs//' configure
    popd > /dev/null

    pushd libxcb-$SUBARCH > /dev/null
    $DIR/sources/libxcb-1.14/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --without-doxygen \
                                        --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build xtrans
if $BUILD_XTRANS; then
    if [ ! -d "xtrans-$SUBARCH" ]; then
        mkdir xtrans-$SUBARCH
    fi

    pushd $DIR/sources/xtrans-1.4.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd xtrans-$SUBARCH > /dev/null
    $DIR/sources/xtrans-1.4.0/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libX11
if $BUILD_LIBX11; then
    if [ ! -d "libX11-$SUBARCH" ]; then
        mkdir libX11-$SUBARCH
    fi

    pushd $DIR/sources/libX11-1.6.9 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libX11-$SUBARCH > /dev/null
    $DIR/sources/libX11-1.6.9/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --disable-ipv6 \
                                        --disable-malloc0returnsnull \
                                        --with-keysymdefdir=$SYSROOT/usr/include/X11 \
                                        --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libXfixes
if $BUILD_LIBXFIXES; then
    if [ ! -d "libXfixes-$SUBARCH" ]; then
        mkdir libXfixes-$SUBARCH
    fi

    pushd $DIR/sources/libXfixes-5.0.3 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libXfixes-$SUBARCH > /dev/null
    $DIR/sources/libXfixes-5.0.3/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libXrender
if $BUILD_LIBXRENDER; then
    if [ ! -d "libXrender-$SUBARCH" ]; then
        mkdir libXrender-$SUBARCH
    fi

    pushd $DIR/sources/libXrender-0.9.10 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libXrender-$SUBARCH > /dev/null
    $DIR/sources/libXrender-0.9.10/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --disable-malloc0returnsnull \
                                        --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libXcursor
if $BUILD_LIBXCURSOR; then
    if [ ! -d "libXcursor-$SUBARCH" ]; then
        mkdir libXcursor-$SUBARCH
    fi

    pushd $DIR/sources/libXcursor-1.2.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libXcursor-$SUBARCH > /dev/null
    $DIR/sources/libXcursor-1.2.0/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libXext
if $BUILD_LIBXEXT; then
    if [ ! -d "libXext-$SUBARCH" ]; then
        mkdir libXext-$SUBARCH
    fi

    pushd $DIR/sources/libXext-1.3.4 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libXext-$SUBARCH > /dev/null
    $DIR/sources/libXext-1.3.4/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --sysconfdir=/etc --localstatedir=/var \
                                        --disable-static --disable-malloc0returnsnull \
                                        --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build cairo
if $BUILD_CAIRO; then
    if [ ! -d "cairo-$SUBARCH" ]; then
        mkdir cairo-$SUBARCH
    fi

    pushd $DIR/sources/cairo-1.16.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.65/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd cairo-$SUBARCH > /dev/null
    # FIXME: configure cannot find these functions for unknown reason.
    ac_cv_func_XRenderCreateConicalGradient=yes \
    ac_cv_func_XRenderCreateLinearGradient=yes \
    ac_cv_func_XRenderCreateRadialGradient=yes \
    ac_cv_func_XRenderCreateSolidFill=yes \
      $DIR/sources/cairo-1.16.0/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build weston
if $BUILD_WESTON; then
    if [ ! -d "weston-$SUBARCH" ]; then
        mkdir weston-$SUBARCH
    fi

    pushd $DIR/sources/weston-4.0.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd weston-$SUBARCH > /dev/null
    $DIR/sources/weston-4.0.0/configure --host=$TARGET --prefix=$CROSSPREFIX \
                                        --with-sysroot=$SYSROOT --disable-x11-compositor \
                                        --disable-weston-launch --disable-fbdev-compositor \
                                        --disable-simple-dmabuf-drm-client \
                                        --disable-simple-dmabuf-v4l-client

    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
