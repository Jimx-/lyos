#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_EVERYTHING:=false}
: ${BUILD_AUTOTOOLS:=false}
: ${BUILD_LIBTOOL:=false}
: ${BUILD_BINUTILS:=false}
: ${BUILD_GCC:=false}
: ${BUILD_WAYLAND_SCANNER:=false}
: ${BUILD_HOST_CMAKE:=false}
: ${BUILD_HOST_PYTHON:=false}
: ${BUILD_NEWLIB:=false}
: ${BUILD_LIBSTDCPP:=false}
: ${BUILD_NATIVE_BINUTILS:=false}
: ${BUILD_NATIVE_GCC:=false}
: ${BUILD_CJSON:=false}
: ${BUILD_READLINE:=false}
: ${BUILD_BASH:=false}
: ${BUILD_COREUTILS:=false}
: ${BUILD_NCURSES:=false}
: ${BUILD_VIM:=false}
: ${BUILD_LIBEVDEV:=false}
: ${BUILD_PCRE:=false}
: ${BUILD_GREP:=false}
: ${BUILD_LESS:=false}
: ${BUILD_ZLIB:=false}
: ${BUILD_GLIB:=false}
: ${BUILD_PKGCONFIG:=false}
: ${BUILD_LIBPNG:=false}
: ${BUILD_BZIP2:=false}
: ${BUILD_LIBXML2:=false}
: ${BUILD_EUDEV:=false}
: ${BUILD_MTDEV:=false}
: ${BUILD_GETTEXT:=false}
: ${BUILD_GUILE:=false}
: ${BUILD_LWIP:=false}

if $BUILD_EVERYTHING; then
    BUILD_AUTOTOOLS=true
    BUILD_BINUTILS=true
    BUILD_GCC=true
    BUILD_WAYLAND_SCANNER=true
    BUILD_HOST_CMAKE=true
    BUILD_HOST_PYTHON=true
    BUILD_NEWLIB=true
    BUILD_LIBSTDCPP=true
    BUILD_NATIVE_BINUTILS=true
    BUILD_NATIVE_GCC=true
    BUILD_CJSON=true
    BUILD_READLINE=true
    BUILD_BASH=true
    BUILD_COREUTILS=true
    BUILD_NCURSES=true
    BUILD_VIM=true
    BUILD_LIBEVDEV=true
    BUILD_PCRE=true
    BUILD_GREP=true
    BUILD_LESS=true
    BUILD_ZLIB=true
    BUILD_LIBPNG=true
    BUILD_BZIP2=true
    BUILD_LIBXML2=true
    BUILD_EUDEV=true
    BUILD_MTDEV=true
    BUILD_LWIP=true
fi

echo "Building toolchain... (sysroot: $SYSROOT, prefix: $PREFIX, crossprefix: $CROSSPREFIX, target: $TARGET)"

if [ ! -d "build" ]; then
    mkdir build
fi

pushd build > /dev/null

# Build autools
if $BUILD_AUTOTOOLS; then
    if [ ! -d "autoconf-2.65-$SUBARCH" ]; then
        mkdir autoconf-2.65-$SUBARCH
    fi
    if [ ! -d "autoconf-2.69-$SUBARCH" ]; then
        mkdir autoconf-2.69-$SUBARCH
    fi
    if [ ! -d "automake-1.11-$SUBARCH" ]; then
        mkdir automake-1.11-$SUBARCH
    fi
    if [ ! -d "automake-1.15-$SUBARCH" ]; then
        mkdir automake-1.15-$SUBARCH
    fi
    if [ ! -d "automake-1.16.4-$SUBARCH" ]; then
        mkdir automake-1.16.4-$SUBARCH
    fi
    if [ ! -d "libtool-$SUBARCH" ]; then
        mkdir libtool-$SUBARCH
    fi
    if [ ! -d "host-pkg-config-$SUBARCH" ]; then
        mkdir host-pkg-config-$SUBARCH
    fi

    # Build autoconf
    pushd autoconf-2.65-$SUBARCH > /dev/null
    $DIR/sources/autoconf-2.65/configure --prefix=$DIR/tools/autoconf-2.65 || cmd_error
    make || cmd_error
    make install || cmd_error
    popd > /dev/null

    pushd autoconf-2.69-$SUBARCH > /dev/null
    $DIR/sources/autoconf-2.69/configure --prefix=$DIR/tools/autoconf-2.69 || cmd_error
    make || cmd_error
    make install || cmd_error
    popd > /dev/null

    # Build automake
    pushd automake-1.11-$SUBARCH > /dev/null
    $DIR/sources/automake-1.11/configure --prefix=$DIR/tools/automake-1.11 || cmd_error
    make || cmd_error
    make install || cmd_error
    ln -sf $DIR/tools/automake-1.11/share/aclocal-1.11 $DIR/tools/automake-1.11/share/aclocal
    popd > /dev/null

    pushd automake-1.15-$SUBARCH > /dev/null
    $DIR/sources/automake-1.15/configure --prefix=$DIR/tools/automake-1.15 || cmd_error
    make || cmd_error
    make install || cmd_error
    rm -rf $DIR/tools/automake-1.15/share/aclocal
    ln -sf $DIR/tools/automake-1.15/share/aclocal-1.15 $DIR/tools/automake-1.15/share/aclocal
    popd > /dev/null

    pushd automake-1.16.4-$SUBARCH > /dev/null
    $DIR/sources/automake-1.16.4/configure --prefix=$DIR/tools/automake-1.16.4 || cmd_error
    make || cmd_error
    make install || cmd_error
    rm -rf $DIR/tools/automake-1.16.4/share/aclocal
    ln -sf $DIR/tools/automake-1.16.4/share/aclocal-1.16 $DIR/tools/automake-1.16.4/share/aclocal
    popd > /dev/null

    # Build libtool
    pushd $DIR/sources/libtool-2.4.5 > /dev/null
    chmod +w bootstrap
    sed -i 's/git:\/\/git\.sv\.gnu\.org\/gnulib/https:\/\/git.savannah.gnu.org\/git\/gnulib.git/g
' bootstrap
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH \
        ./bootstrap --force
    popd > /dev/null

    pushd libtool-$SUBARCH > /dev/null
    (export PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH;
     $DIR/sources/libtool-2.4.5/configure --prefix=$PREFIX &&
             make -j$PARALLELISM &&
             make install
    ) || cmd_error
    popd > /dev/null

    # Build pkg-config
    pushd host-pkg-config-$SUBARCH > /dev/null
    $DIR/sources/pkg-config-0.29.2/configure --prefix=$PREFIX --with-internal-glib || cmd_error
    make || cmd_error
    make install || cmd_error
    popd > /dev/null

    # Build autoconf-archive
    mkdir -p $DIR/tools/autoconf-archive/share/
    cp -r $DIR/sources/autoconf-archive-2019.01.06/m4 $DIR/tools/autoconf-archive/share/aclocal
    ln -sf $DIR/tools/autoconf-archive/share/aclocal/*.m4 $DIR/tools/automake-1.11/share/aclocal-1.11/
    ln -sf $DIR/tools/autoconf-archive/share/aclocal/*.m4 $DIR/tools/automake-1.15/share/aclocal-1.15/

    ln -sf $DIR/local/share/aclocal/* $DIR/tools/automake-1.11/share/aclocal-1.11/
    ln -sf $DIR/local/share/aclocal/* $DIR/tools/automake-1.15/share/aclocal-1.15/
    ln -sf $DIR/local/share/aclocal/* $DIR/tools/automake-1.16.4/share/aclocal-1.16/
fi

# Build binutils
if $BUILD_BINUTILS; then
    if [ ! -d "binutils-$SUBARCH" ]; then
        mkdir binutils-$SUBARCH
    fi

    unset PKG_CONFIG_LIBDIR

    pushd $DIR/sources/binutils-2.31/ld > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf || cmd_error
    popd > /dev/null

    pushd binutils-$SUBARCH
    $DIR/sources/binutils-2.31/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-werror --enable-separate-code || cmd_error
    make -j$PARALLELISM || cmd_error
    make install || cmd_error
    popd
fi

# Build gcc
if $BUILD_GCC; then
    if [ ! -d "gcc-$SUBARCH" ]; then
        mkdir gcc-$SUBARCH
    fi

    unset PKG_CONFIG_LIBDIR

    # For GCC's LIMITS_H_TEST
    if [ ! -f $SYSROOT/usr/include/limits.h ]; then
        mkdir -p $SYSROOT/usr/include/
        echo "/* limits.h - Do not edit. Overwritten after newlib is built. */" > $SYSROOT/usr/include/limits.h
    fi

    pushd gcc-$SUBARCH
    $DIR/sources/gcc-9.2.0/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --with-newlib --enable-shared --with-pic || cmd_error
    make -j$PARALLELISM all-gcc all-target-libgcc || cmd_error
    make install-gcc install-target-libgcc || cmd_error

    popd
fi

# Build host wayland-scanner
if $BUILD_WAYLAND_SCANNER; then
    if [ ! -d "host-wayland-scanner" ]; then
        mkdir host-wayland-scanner
    fi

    pushd host-wayland-scanner > /dev/null
    meson --prefix=$PREFIX --libdir=$PREFIX/lib -Ddocumentation=false -Ddtd_validation=false \
          -Dlibraries=false $DIR/sources/wayland-1.18.0
    ninja || cmd_error
    ninja install || cmd_error
    popd > /dev/null
fi

# Build host python
if $BUILD_HOST_PYTHON; then
    if [ ! -d "host-python" ]; then
        mkdir host-python
    fi

    pushd host-python > /dev/null
    $DIR/sources/Python-3.8.2/configure --prefix=$DIR/tools/python-3.8
    make -j$PARALLELISM || cmd_error
    make install || cmd_error
    popd > /dev/null
fi

# Build host cmake
if $BUILD_HOST_CMAKE; then
    if [ ! -d "host-cmake-$SUBARCH" ]; then
        mkdir host-cmake-$SUBARCH
    fi

    pushd host-cmake-$SUBARCH > /dev/null
    $DIR/sources/cmake-3.22.1/bootstrap --prefix=$DIR/tools/cmake-3.22.1 --parallel=$PARALLELISM
    make -j$PARALLELISM || cmd_error
    make install || cmd_error
    ln -sf $DIR/lyos.cmake $DIR/tools/cmake-3.22.1/share/cmake-3.22/Modules/Platform
    popd > /dev/null
fi

. $DIR/activate.sh

# Build newlib
if $BUILD_NEWLIB; then
    if [ ! -d "newlib-$SUBARCH" ]; then
        mkdir newlib-$SUBARCH
    else
        rm -rf newlib-$SUBARCH
        mkdir newlib-$SUBARCH
    fi

    pushd $DIR/sources/newlib-3.0.0 > /dev/null
    # find -type f -exec sed 's|--cygnus||g;s|cygnus||g' -i {} + || cmd_error
    popd > /dev/null

    pushd $DIR/sources/newlib-3.0.0/newlib/libc/sys > /dev/null
    PATH=$DIR/tools/autoconf-2.65/bin:$DIR/tools/automake-1.11/bin:$PATH autoconf || cmd_error
    pushd lyos > /dev/null
    PATH=$DIR/tools/autoconf-2.65/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf
    popd > /dev/null
    popd > /dev/null

    pushd $DIR/sources/newlib-3.0.0/libgloss/aarch64 > /dev/null
    PATH=$DIR/tools/autoconf-2.65/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf || cmd_error
    popd > /dev/null

    pushd newlib-$SUBARCH > /dev/null
    $DIR/sources/newlib-3.0.0/configure --target=$TARGET --prefix=$CROSSPREFIX --disable-multilib || cmd_error
    sed -s "s/prefix}\/$TARGET/prefix}/" Makefile > Makefile.bak
    mv Makefile.bak Makefile

    TARGET_CFLAGS=-fPIC make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $TARGET/newlib/libc/sys/lyos/crt*.o $SYSROOT/$CROSSPREFIX/lib/
    $TARGET-gcc -nolibc -shared -o $SYSROOT/usr/lib/libc.so -Wl,--whole-archive $SYSROOT/usr/lib/libc.a -Wl,--no-whole-archive || cmd_error
    $TARGET-gcc -nolibc -shared -o $SYSROOT/usr/lib/libm.so -Wl,--whole-archive $SYSROOT/usr/lib/libm.a -Wl,--no-whole-archive || cmd_error

    popd > /dev/null
fi

if $BUILD_LIBSTDCPP; then
    pushd gcc-$SUBARCH > /dev/null
    make all-target-libstdc++-v3 -j$PARALLELISM || cmd_error
    make install-target-libstdc++-v3 || cmd_error
    popd > /dev/null
fi

# Build native binutils
if $BUILD_NATIVE_BINUTILS; then
    if [ ! -d "binutils-native-$SUBARCH" ]; then
        mkdir binutils-native-$SUBARCH
    fi

    pushd binutils-native-$SUBARCH > /dev/null
    $DIR/sources/binutils-2.31/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-werror || cmd_error
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build native gcc
if $BUILD_NATIVE_GCC; then
    if [ ! -d "gcc-native-$SUBARCH" ]; then
        mkdir gcc-native-$SUBARCH
    fi

    if [ ! -e $DIR/sources/gcc-9.2.0/mpfr ]; then
        ln -s $DIR/sources/mpfr-4.1.0 $DIR/sources/gcc-9.2.0/mpfr
    fi
    if [ ! -e $DIR/sources/gcc-9.2.0/mpc ]; then
        ln -s $DIR/sources/mpc-1.2.1 $DIR/sources/gcc-9.2.0/mpc
    fi
    if [ ! -e $DIR/sources/gcc-9.2.0/gmp ]; then
        ln -s $DIR/sources/gmp-6.1.0 $DIR/sources/gcc-9.2.0/gmp
    fi

    pushd gcc-native-$SUBARCH > /dev/null
    $DIR/sources/gcc-9.2.0/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX --with-sysroot=/ --with-build-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib --enable-shared=libgcc CFLAGS=-O2 CXXFLAGS=-O2 || cmd_error
    make DESTDIR=$SYSROOT all-gcc -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install-gcc || cmd_error
    make DESTDIR=$SYSROOT all-target-libgcc -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install-target-libgcc || cmd_error
    touch $SYSROOT/usr/include/fenv.h
    make DESTDIR=$SYSROOT all-target-libstdc++-v3 -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install-target-libstdc++-v3 -j$PARALLELISM || cmd_error
    popd > /dev/null
fi

# Build cJSON
if $BUILD_CJSON; then
    if [ ! -d "cJSON-$SUBARCH" ]; then
        mkdir cJSON-$SUBARCH
    fi

    pushd cJSON-$SUBARCH > /dev/null
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=$TARGET_CMAKE_TOOLCHAIN_FILE \
          -DCMAKE_INSTALL_PREFIX=$CROSSPREFIX $DIR/sources/cJSON-1.7.14/
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

if $BUILD_READLINE; then
    if [ ! -d "readline-$SUBARCH" ]; then
        mkdir readline-$SUBARCH
    fi

    pushd readline-$SUBARCH > /dev/null
    $DIR/sources/readline-8.0/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-static --enable-multibyte || cmd_error
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build bash
if $BUILD_BASH; then
    if [ ! -d "bash-$SUBARCH" ]; then
        mkdir bash-$SUBARCH
    fi

    pushd bash-$SUBARCH > /dev/null
    $DIR/sources/bash-5.1.8/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX  --without-bash-malloc --disable-nls || cmd_error
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $SYSROOT/usr/bin/bash $SYSROOT/bin/sh
    cp $SYSROOT/usr/bin/bash $SYSROOT/bin/bash
    popd > /dev/null
fi

# Build coreutils
if $BUILD_COREUTILS; then
    if [ ! -d "coreutils-$SUBARCH" ]; then
        mkdir coreutils-$SUBARCH
    fi

    pushd coreutils-$SUBARCH > /dev/null
    gl_cv_func_fstatat_zero_flag=yes \
    ac_cv_func_getgroups_works=yes \
      $DIR/sources/coreutils-8.32/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-nls || cmd_error
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build ncurses
if $BUILD_NCURSES; then
    if [ ! -d "ncurses-$SUBARCH" ]; then
        mkdir ncurses-$SUBARCH
    fi

    pushd ncurses-$SUBARCH > /dev/null
    STRIP=$TARGET-strip $DIR/sources/ncurses-6.2/configure --host=$TARGET --prefix=$CROSSPREFIX --with-terminfo-dirs=/usr/share/terminfo --with-default-terminfo-dir=/usr/share/terminfo --without-tests
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build Vim
if $BUILD_VIM; then
    pushd $DIR/sources/vim74 > /dev/null
    make distclean
    ac_cv_sizeof_int=4 vim_cv_getcwd_broken=no vim_cv_memmove_handles_overlap=yes vim_cv_stat_ignores_slash=no vim_cv_tgetent=zero vim_cv_terminfo=yes vim_cv_toupper_broken=no vim_cv_tty_group=world STRIP=$TARGET-strip $DIR/sources/vim74/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX --with-tlib=ncurses --enable-gui=no --disable-gtktest --disable-xim --with-features=normal --disable-gpm --without-x --disable-netbeans --enable-multibyte || cmd_error
    make clean
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libevdev
if $BUILD_LIBEVDEV; then
    if [ ! -d "libevdev-$SUBARCH" ]; then
        mkdir libevdev-$SUBARCH
    fi

    pushd $DIR/sources/libevdev-1.9.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.16.4/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libevdev-$SUBARCH > /dev/null
    $DIR/sources/libevdev-1.9.0/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build PCRE
if $BUILD_PCRE; then
    if [ ! -d "pcre-$SUBARCH" ]; then
        mkdir pcre-$SUBARCH
    fi

    pushd $DIR/sources/pcre-8.44 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.16.4/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd pcre-$SUBARCH > /dev/null
    $DIR/sources/pcre-8.44/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --enable-unicode-properties --enable-pcre8 --enable-pcre16 --enable-pcre32
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build grep
if $BUILD_GREP; then
    if [ ! -d "grep-$SUBARCH" ]; then
        mkdir grep-$SUBARCH
    fi

    cp $DIR/tools/automake-1.16.4/share/automake-1.16/config.sub $DIR/sources/grep-3.4/build-aux/

    pushd grep-$SUBARCH > /dev/null
    $DIR/sources/grep-3.4/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-nls
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build less
if $BUILD_LESS; then
    if [ ! -d "less-$SUBARCH" ]; then
        mkdir less-$SUBARCH
    fi

    pushd less-$SUBARCH > /dev/null
    $DIR/sources/less-551/configure --host=$TARGET --prefix=$CROSSPREFIX --sysconfdir=/etc
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build zlib
if $BUILD_ZLIB; then
    if [ ! -d "zlib-$SUBARCH" ]; then
        mkdir zlib-$SUBARCH
    fi

    pushd zlib-$SUBARCH > /dev/null
    CHOST=$TARGET prefix=$CROSSPREFIX $DIR/sources/zlib-1.2.11/configure
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build glib
if $BUILD_GLIB; then
    if [ ! -d "glib-$SUBARCH" ]; then
        mkdir glib-$SUBARCH
    fi

    pushd glib-$SUBARCH > /dev/null
    PKG_CONFIG_SYSROOT_DIR=$SYSROOT PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig meson --cross-file ../../meson.cross-file --prefix=$CROSSPREFIX --libdir=lib --buildtype=debugoptimized -Dxattr=false $DIR/sources/glib-2.59.2
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja intstall || cmd_error
    popd > /dev/null
fi

# Build pkg-config
if $BUILD_PKGCONFIG; then
    if [ ! -d "pkg-config-$SUBARCH" ]; then
        mkdir pkg-config-$SUBARCH
    fi

    pushd pkg-config-$SUBARCH > /dev/null
    $DIR/sources/pkg-config-0.29.2/configure --host=$TARGET --prefix=$CROSSPREFIX --with-internal-glib --disable-static
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libpng
if $BUILD_LIBPNG; then
    if [ ! -d "libpng-$SUBARCH" ]; then
        mkdir libpng-$SUBARCH
    fi

    pushd $DIR/sources/libpng-1.6.37 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.16.4/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libpng-$SUBARCH > /dev/null
    $DIR/sources/libpng-1.6.37/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build bzip2
if $BUILD_BZIP2; then
    if [ -d "bzip2-$SUBARCH" ]; then
        rm -rf bzip2-$SUBARCH
    fi

    cp -rf $DIR/sources/bzip2-1.0.8 bzip2-$SUBARCH

    pushd bzip2-$SUBARCH > /dev/null
    sed -i s/"all: libbz2.a bzip2 bzip2recover test"/"all: libbz2.a bzip2 bzip2recover"/ Makefile

    make CC=$TARGET-gcc CFLAGS=-fPIC -f Makefile-libbz2_so || cmd_error
    make clean || cmd_error
    make CC=$TARGET-gcc CFLAGS=-fPIC -j$PARALLELISM || cmd_error
    make PREFIX=$SYSROOT/$CROSSPREFIX install || cmd_error
    popd > /dev/null
fi

# Build libxml2
if $BUILD_LIBXML2; then
    if [ ! -d "libxml2-$SUBARCH" ]; then
        mkdir libxml2-$SUBARCH
    fi

    pushd $DIR/sources/libxml2-2.9.10 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.16.4/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd libxml2-$SUBARCH > /dev/null
    $DIR/sources/libxml2-2.9.10/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --disable-static --with-threads --disable-ipv6 --without-python
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build eudev
if $BUILD_EUDEV; then
    if [ ! -d "eudev-$SUBARCH" ]; then
        mkdir eudev-$SUBARCH
    fi

    pushd $DIR/sources/eudev-3.2.2 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.16.4/bin:$PATH autoreconf -fis || cmd_error
    popd > /dev/null

    pushd eudev-$SUBARCH > /dev/null
    $DIR/sources/eudev-3.2.2/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --disable-blkid --disable-selinux --disable-kmod --disable-mtd-probe --disable-rule-generator --disable-manpages || cmd_error
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build mtdev
if $BUILD_MTDEV; then
    if [ ! -d "mtdev-$SUBARCH" ]; then
        mkdir mtdev-$SUBARCH
    fi

    pushd $DIR/sources/mtdev-1.1.6 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.16.4/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd mtdev-$SUBARCH > /dev/null
    $DIR/sources/mtdev-1.1.6/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build gettext
if $BUILD_GETTEXT; then
    if [ ! -d "gettext-$SUBARCH" ]; then
        mkdir gettext-$SUBARCH
    fi

    pushd $DIR/sources/gettext-0.21 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd gettext-$SUBARCH > /dev/null
    $DIR/sources/gettext-0.21/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build guile
if $BUILD_GUILE; then
    if [ ! -d "guile-$SUBARCH" ]; then
        mkdir guile-$SUBARCH
    fi

    pushd $DIR/sources/guile-3.0.4 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fis
    popd > /dev/null

    pushd guile-$SUBARCH > /dev/null
    $DIR/sources/guile-3.0.4/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build lwip
if $BUILD_LWIP; then
    if [ ! -d "lwip-$SUBARCH" ]; then
        mkdir lwip-$SUBARCH
    fi

    pushd lwip-$SUBARCH > /dev/null
    PATH=$DIR/tools/cmake-3.22.1/bin:$PATH cmake -GNinja \
          -DCMAKE_TOOLCHAIN_FILE=$TARGET_CMAKE_TOOLCHAIN_FILE \
          -DCMAKE_INSTALL_PREFIX=$CROSSPREFIX \
          -DLWIP_DIR=$DIR/sources/lwip-STABLE-2_1_3_RELEASE \
          -DLWIP_INCLUDE_DIRS="$DIR/sources/lwip-STABLE-2_1_3_RELEASE/src/include;$DIR/patches/lwip" \
          -DCMAKE_C_FLAGS=-DLWIP_DEBUG \
          $DIR/sources/lwip-STABLE-2_1_3_RELEASE/
    ninja lwipcore || cmd_error
    cp liblwipcore.a $SYSROOT/lib
    cp -rf $DIR/sources/lwip-STABLE-2_1_3_RELEASE/src/include/lwip $SYSROOT/usr/include/
    cp -rf $DIR/patches/lwip/* $SYSROOT/usr/include/lwip
    popd > /dev/null
fi

popd > /dev/null
