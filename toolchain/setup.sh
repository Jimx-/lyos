#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_EVERYTHING:=false}
: ${BUILD_BINUTILS:=false}
: ${BUILD_GCC:=false}
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
: ${BUILD_LIBDRM:=false}

if $BUILD_EVERYTHING; then
    BUILD_BINUTILS=true
    BUILD_GCC=true
    BUILD_NEWLIB=true
    BUILD_LIBSTDCPP=true
    BUILD_NATIVE_BINUTILS=true
    BUILD_NATIVE_GCC=true
    BUILD_READLINE=true
    BUILD_BASH=true
    BUILD_COREUTILS=true
    BUILD_NCURSES=true
    BUILD_VIM=true
fi

echo "Building toolchain... (sysroot: $SYSROOT, prefix: $PREFIX, crossprefix: $CROSSPREFIX, target: $TARGET)"

if [ ! -d "build" ]; then
    mkdir build
fi

pushd build > /dev/null

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

    pushd gcc-$SUBARCH
    $DIR/sources/gcc-7.1.0/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --with-newlib --enable-shared=libgcc || cmd_error
    make all-gcc all-target-libgcc -j || cmd_error
    make install-gcc install-target-libgcc || cmd_error

    popd
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
    autoconf || cmd_error
    pushd lyos > /dev/null
    autoreconf
    popd > /dev/null
    popd > /dev/null

    pushd newlib-$SUBARCH > /dev/null
    $DIR/sources/newlib-3.0.0/configure --target=$TARGET --prefix=$CROSSPREFIX || cmd_error
    sed -s "s/prefix}\/$TARGET/prefix}/" Makefile > Makefile.bak
    mv Makefile.bak Makefile
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $TARGET/newlib/libc/sys/lyos/crt*.o $SYSROOT/$CROSSPREFIX/lib/
    $TARGET-gcc -shared -o $SYSROOT/usr/lib/libc.so -Wl,--whole-archive $SYSROOT/usr/lib/libc.a -Wl,--no-whole-archive || cmd_error
    $TARGET-gcc -shared -o $SYSROOT/usr/lib/libg.so -Wl,--whole-archive $SYSROOT/usr/lib/libg.a -Wl,--no-whole-archive || cmd_error
    popd > /dev/null
fi

if $BUILD_LIBSTDCPP; then
    pushd gcc-$SUBARCH > /dev/null
    make all-target-libstdc++-v3 -j4 || cmd_error
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
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build libevdev
if $BUILD_LIBEVDEV; then
    if [ ! -d "libevdev-$SUBARCH" ]; then
        mkdir libevdev-$SUBARCH
    fi

    pushd libevdev-$SUBARCH > /dev/null
    $DIR/sources/libevdev-1.9.0/configure --host=$TARGET --prefix=/usr --with-sysroot=$SYSROOT
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
