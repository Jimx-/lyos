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
: ${BUILD_NEWLIB:=false}
: ${BUILD_LIBSTDCPP:=false}
: ${BUILD_NATIVE_BINUTILS:=false}
: ${BUILD_NATIVE_GCC:=false}
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
: ${BUILD_GDBM:=false}
: ${BUILD_XZ:=false}
: ${BUILD_LIBRESSL:=false}

if $BUILD_EVERYTHING; then
    BUILD_AUTOTOOLS=true
    BUILD_BINUTILS=true
    BUILD_GCC=true
    BUILD_WAYLAND_SCANNER=true
    BUILD_NEWLIB=true
    BUILD_LIBSTDCPP=true
    BUILD_NATIVE_BINUTILS=true
    BUILD_NATIVE_GCC=true
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
             make -j8 &&
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
fi

# Build binutils
if $BUILD_BINUTILS; then
    if [ ! -d "binutils-$SUBARCH" ]; then
        mkdir binutils-$SUBARCH
    fi

    unset PKG_CONFIG_LIBDIR

    pushd binutils-$SUBARCH
    $DIR/sources/binutils-2.31/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-werror || cmd_error
    make -j8 || cmd_error
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
        cp $DIR/patches/newlib/lyos/include/limits.h $SYSROOT/usr/include/
    fi

    pushd gcc-$SUBARCH
    $DIR/sources/gcc-9.2.0/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --with-newlib --enable-shared=libgcc || cmd_error
    make all-gcc all-target-libgcc -j4 || cmd_error
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

. $DIR/activate.sh

# Build newlib
if $BUILD_NEWLIB; then
    if [ ! -d "newlib-static-$SUBARCH" ]; then
        mkdir newlib-static-$SUBARCH
    else
        rm -rf newlib-static-$SUBARCH
        mkdir newlib-static-$SUBARCH
    fi

    if [ ! -d "newlib-dynamic-$SUBARCH" ]; then
        mkdir newlib-dynamic-$SUBARCH
    else
        rm -rf newlib-dynamic-$SUBARCH
        mkdir newlib-dynamic-$SUBARCH
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

    # Build dynamic library with -fPIC
    pushd newlib-dynamic-$SUBARCH > /dev/null
    $DIR/sources/newlib-3.0.0/configure --target=$TARGET --prefix=$CROSSPREFIX || cmd_error
    sed -s "s/prefix}\/$TARGET/prefix}/" Makefile > Makefile.bak
    mv Makefile.bak Makefile

    TARGET_CFLAGS=-fPIC make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $TARGET/newlib/libc/sys/lyos/crt*.o $SYSROOT/$CROSSPREFIX/lib/
    $TARGET-gcc -nolibc -shared -o $SYSROOT/usr/lib/libc.so -Wl,--whole-archive $SYSROOT/usr/lib/libc.a -Wl,--no-whole-archive || cmd_error
    $TARGET-gcc -nolibc -shared -o $SYSROOT/usr/lib/libm.so -Wl,--whole-archive $SYSROOT/usr/lib/libm.a -Wl,--no-whole-archive || cmd_error
    popd > /dev/null

    # Build static library without -fPIC
    pushd newlib-static-$SUBARCH > /dev/null
    $DIR/sources/newlib-3.0.0/configure --target=$TARGET --prefix=$CROSSPREFIX || cmd_error
    sed -s "s/prefix}\/$TARGET/prefix}/" Makefile > Makefile.bak
    mv Makefile.bak Makefile

    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $TARGET/newlib/libc/sys/lyos/crt*.o $SYSROOT/$CROSSPREFIX/lib/
    popd > /dev/null
fi

if $BUILD_LIBSTDCPP; then
    pushd gcc-$SUBARCH > /dev/null
    make all-target-libstdc++-v3 -j || cmd_error
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
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build native gcc
if $BUILD_NATIVE_GCC; then
    if [ ! -d "gcc-native-$SUBARCH" ]; then
        mkdir gcc-native-$SUBARCH
    fi

    if [ ! -e $DIR/sources/gcc-4.7.3/mpfr ]; then
        ln -s $DIR/sources/mpfr-2.4.2 $DIR/sources/gcc-4.7.3/mpfr
    fi
    if [ ! -e $DIR/sources/gcc-4.7.3/mpc ]; then
        ln -s $DIR/sources/mpc-0.8.1 $DIR/sources/gcc-4.7.3/mpc
    fi
    if [ ! -e $DIR/sources/gcc-4.7.3/gmp ]; then
        ln -s $DIR/sources/gmp-4.3.2 $DIR/sources/gcc-4.7.3/gmp
    fi

    pushd gcc-native-$SUBARCH > /dev/null
    $DIR/sources/gcc-4.7.3/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX --with-sysroot=/ --with-build-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --with-newlib || cmd_error
    make DESTDIR=$SYSROOT all-gcc -j4 || cmd_error
    make DESTDIR=$SYSROOT install-gcc || cmd_error
    make DESTDIR=$SYSROOT all-target-libgcc -j4 || cmd_error
    make DESTDIR=$SYSROOT install-target-libgcc || cmd_error
    # touch $SYSROOT/usr/include/fenv.h
    # make DESTDIR=$SYSROOT all-target-libstdc++-v3 -j8 || cmd_error
    # make DESTDIR=$SYSROOT install-target-libstdc++-v3 -j || cmd_error
    popd > /dev/null
fi

# Build native gcc
# if $BUILD_NATIVE_GCC1; then
#     if [ ! -d "gcc1-native-$SUBARCH" ]; then
#         mkdir gcc1-native-$SUBARCH
#     fi

#     if [ ! -e $DIR/sources/gcc-9.2.0/mpfr ]; then
#         ln -s $DIR/sources/mpfr-3.1.4 $DIR/sources/gcc-9.2.0/mpfr
#     fi
#     if [ ! -e $DIR/sources/gcc-9.2.0/mpc ]; then
#         ln -s $DIR/sources/mpc-1.0.3 $DIR/sources/gcc-9.2.0/mpc
#     fi
#     if [ ! -e $DIR/sources/gcc-9.2.0/gmp ]; then
#         ln -s $DIR/sources/gmp-6.1.0 $DIR/sources/gcc-9.2.0/gmp
#     fi

#     pushd gcc1-native-$SUBARCH > /dev/null
#     $DIR/sources/gcc-9.2.0/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX --with-sysroot=/ --with-build-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib --enable-shared=libgcc CFLAGS=-O2 CXXFLAGS=-O2 || cmd_error
#     make DESTDIR=$SYSROOT all-gcc -j4 || cmd_error
#     make DESTDIR=$SYSROOT install-gcc || cmd_error
#     make DESTDIR=$SYSROOT all-target-libgcc -j4 || cmd_error
#     make DESTDIR=$SYSROOT install-target-libgcc || cmd_error
#     touch $SYSROOT/usr/include/fenv.h
#     make DESTDIR=$SYSROOT all-target-libstdc++-v3 -j8 || cmd_error
#     make DESTDIR=$SYSROOT install-target-libstdc++-v3 -j || cmd_error
#     popd > /dev/null
# fi

if $BUILD_READLINE; then
    if [ ! -d "readline-$SUBARCH" ]; then
        mkdir readline-$SUBARCH
    fi

    pushd readline-$SUBARCH > /dev/null
    $DIR/sources/readline-8.0/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-static --enable-multibyte || cmd_error
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build bash
if $BUILD_BASH; then
    if [ ! -d "bash-$SUBARCH" ]; then
        mkdir bash-$SUBARCH
    fi

    pushd bash-$SUBARCH > /dev/null
    $DIR/sources/bash-4.3/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX  --without-bash-malloc --disable-nls --with-installed-readline || cmd_error
    make -j || cmd_error
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
    touch $DIR/sources/coreutils-8.13/man/sort.1
    touch $DIR/sources/coreutils-8.13/man/stat.1
    gl_cv_func_fstatat_zero_flag=yes \
    ac_cv_func_getgroups_works=yes \
      $DIR/sources/coreutils-8.13/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-nls || cmd_error
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build ncurses
if $BUILD_NCURSES; then
    if [ ! -d "ncurses-$SUBARCH" ]; then
        mkdir ncurses-$SUBARCH
    fi

    pushd ncurses-$SUBARCH > /dev/null
    $DIR/sources/ncurses-6.2/configure --host=$TARGET --prefix=$CROSSPREFIX --with-terminfo-dirs=/usr/share/terminfo --with-default-terminfo-dir=/usr/share/terminfo --without-tests
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build Vim
if $BUILD_VIM; then
    pushd $DIR/sources/vim74 > /dev/null
    ac_cv_sizeof_int=4 vim_cv_getcwd_broken=no vim_cv_memmove_handles_overlap=yes vim_cv_stat_ignores_slash=no vim_cv_tgetent=zero vim_cv_terminfo=yes vim_cv_toupper_broken=no vim_cv_tty_group=world $DIR/sources/vim74/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX --with-tlib=ncurses --enable-gui=no --disable-gtktest --disable-xim --with-features=normal --disable-gpm --without-x --disable-netbeans --enable-multibyte || cmd_error
    make clean
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libevdev
if $BUILD_LIBEVDEV; then
    if [ ! -d "libevdev-$SUBARCH" ]; then
        mkdir libevdev-$SUBARCH
    fi

    pushd $DIR/sources/libevdev-1.9.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libevdev-$SUBARCH > /dev/null
    $DIR/sources/libevdev-1.9.0/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build PCRE
if $BUILD_PCRE; then
    if [ ! -d "pcre-$SUBARCH" ]; then
        mkdir pcre-$SUBARCH
    fi

    pushd $DIR/sources/pcre-8.44 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd pcre-$SUBARCH > /dev/null
    $DIR/sources/pcre-8.44/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --enable-unicode-properties --enable-pcre8 --enable-pcre16 --enable-pcre32
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build grep
if $BUILD_GREP; then
    if [ ! -d "grep-$SUBARCH" ]; then
        mkdir grep-$SUBARCH
    fi

    cp $DIR/tools/automake-1.15/share/automake-1.15/config.sub $DIR/sources/grep-3.4/build-aux/

    pushd grep-$SUBARCH > /dev/null
    $DIR/sources/grep-3.4/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-nls
    make -j || cmd_error
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
    make -j || cmd_error
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
    make -j || cmd_error
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
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libpng
if $BUILD_LIBPNG; then
    if [ ! -d "libpng-$SUBARCH" ]; then
        mkdir libpng-$SUBARCH
    fi

    pushd $DIR/sources/libpng-1.6.37 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fiv
    popd > /dev/null

    pushd libpng-$SUBARCH > /dev/null
    $DIR/sources/libpng-1.6.37/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j || cmd_error
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
    make CC=$TARGET-gcc CFLAGS=-fPIC -j || cmd_error
    make PREFIX=$SYSROOT/$CROSSPREFIX install || cmd_error
    popd > /dev/null
fi

# Build libxml2
if $BUILD_LIBXML2; then
    if [ ! -d "libxml2-$SUBARCH" ]; then
        mkdir libxml2-$SUBARCH
    fi

    pushd $DIR/sources/libxml2-2.9.10 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd libxml2-$SUBARCH > /dev/null
    $DIR/sources/libxml2-2.9.10/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --disable-static --with-threads --disable-ipv6 --without-python
    make -j4 || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build eudev
if $BUILD_EUDEV; then
    if [ ! -d "eudev-$SUBARCH" ]; then
        mkdir eudev-$SUBARCH
    fi

    pushd $DIR/sources/eudev-3.2.2 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.11/bin:$PATH autoreconf -fis || cmd_error
    popd > /dev/null

    pushd eudev-$SUBARCH > /dev/null
    $DIR/sources/eudev-3.2.2/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT --disable-blkid --disable-selinux --disable-kmod --disable-mtd-probe --disable-rule-generator --disable-manpages || cmd_error
    make -j4 || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build mtdev
if $BUILD_MTDEV; then
    if [ ! -d "mtdev-$SUBARCH" ]; then
        mkdir mtdev-$SUBARCH
    fi

    pushd $DIR/sources/mtdev-1.1.6 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd mtdev-$SUBARCH > /dev/null
    $DIR/sources/mtdev-1.1.6/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT
    make -j4 || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build gdbm
if $BUILD_GDBM; then
    if [ ! -d "gdbm-$SUBARCH" ]; then
        mkdir gdbm-$SUBARCH
    fi

    pushd $DIR/sources/gdbm-1.18.1 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fis
    popd > /dev/null

    pushd gdbm-$SUBARCH > /dev/null
    $DIR/sources/gdbm-1.18.1/configure --host=$TARGET --prefix=$CROSSPREFIX \
        --with-sysroot=$SYSROOT --disable-nls --disable-static --enable-libgdbm-compat \
        --without-readline
    make -j4 || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build xz
if $BUILD_XZ; then
    if [ ! -d "xz-$SUBARCH" ]; then
        mkdir xz-$SUBARCH
    fi

    pushd $DIR/sources/xz-5.2.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd xz-$SUBARCH > /dev/null
    $DIR/sources/xz-5.2.0/configure --host=$TARGET --prefix=$CROSSPREFIX \
        --with-sysroot=$SYSROOT --disable-nls --disable-static
    make -j4 || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libressl
if $BUILD_LIBRESSL; then
    if [ ! -d "libressl-$SUBARCH" ]; then
        mkdir libressl-$SUBARCH
    fi

    pushd libressl-$SUBARCH > /dev/null
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=$TARGET_CMAKE_TOOLCHAIN_FILE -DCMAKE_INSTALL_PREFIX=/usr \
          -DLIBRESSL_APPS=OFF -DBUILD_SHARED_LIBS=ON $DIR/sources/libressl-3.0.2/
    ninja || cmd_error
    DESTDIR=$SYSROOT ninja install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
