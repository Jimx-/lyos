#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_EVERYTHING:=false}
: ${BUILD_NEWLIB:=false}
: ${BUILD_BASH:=false}
: ${BUILD_COREUTILS:=false}
: ${BUILD_DASH:=false}
: ${BUILD_VIM:=false}

if $BUILD_EVERYTHING; then
    BUILD_NEWLIB=true
    BUILD_BASH=true
    BUILD_COREUTILS=true
    BUILD_DASH=true
    BUILD_VIM=true
fi

echo "Building toolchain... (sysroot: $SYSROOT, prefix: $PREFIX, target: $TARGET)"

if [ ! -d "build" ]; then
    mkdir build
fi

pushd build > /dev/null

. $DIR/activate.sh

# Build newlib
if $BUILD_NEWLIB; then
    if [ ! -d "newlib" ]; then
        mkdir newlib
    else
        rm -rf newlib
        mkdir newlib
    fi
    
    pushd $DIR/sources/newlib-2.0.0 > /dev/null
    find -type f -exec sed 's|--cygnus||g;s|cygnus||g' -i {} + || cmd_error
    popd > /dev/null
    
    pushd $DIR/sources/newlib-2.0.0/newlib/libc/sys > /dev/null
    autoconf || cmd_error
    pushd lyos > /dev/null
    autoreconf
    popd > /dev/null
    popd > /dev/null
    
    pushd newlib > /dev/null
    $DIR/sources/newlib-2.0.0/configure --target=$TARGET --prefix=$CROSSPREFIX || cmd_error
    sed -s "s/prefix}\/$TARGET/prefix}/" Makefile > Makefile.bak
    mv Makefile.bak Makefile
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $TARGET/newlib/libc/sys/lyos/crt*.o $SYSROOT/$CROSSPREFIX/lib/
    $TARGET-gcc -shared -o $SYSROOT/usr/lib/libc.so -Wl,--whole-archive $SYSROOT/usr/lib/libc.a -Wl,--no-whole-archive || cmd_error
    $TARGET-gcc -shared -o $SYSROOT/usr/lib/libg.so -Wl,--whole-archive $SYSROOT/usr/lib/libg.a -Wl,--no-whole-archive || cmd_error
    popd > /dev/null
fi

# Build bash
if $BUILD_BASH; then
    if [ ! -d "bash" ]; then
        mkdir bash
    fi
    
    pushd bash > /dev/null
    $DIR/sources/bash-4.3/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX  --without-bash-malloc --disable-nls || cmd_error
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $SYSROOT/usr/bin/bash $SYSROOT/bin/bash
    popd > /dev/null
fi

# Build coreutils
if $BUILD_COREUTILS; then
    if [ ! -d "coreutils" ]; then
        mkdir coreutils
    fi
    
    pushd coreutils > /dev/null
    $DIR/sources/coreutils-8.13/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-nls || cmd_error
    make -j
    make DESTDIR=$SYSROOT install
    popd > /dev/null
fi 

# Build dash
if $BUILD_DASH; then
    if [ ! -d "dash" ]; then
        mkdir dash
    fi
    
    pushd dash > /dev/null
    $DIR/sources/dash-0.5.8/configure --host=$TARGET --prefix=$CROSSPREFIX || cmd_error
    sed -i '/# define _GNU_SOURCE 1/d' config.h
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    cp $SYSROOT/usr/bin/dash $SYSROOT/bin/sh
    popd > /dev/null
fi

# Build Vim
if $BUILD_VIM; then
    pushd $DIR/sources/vim74 > /dev/null
    #ac_cv_sizeof_int=4 vim_cv_getcwd_broken=no vim_cv_memmove_handles_overlap=yes vim_cv_stat_ignores_slash=no vim_cv_tgetent=zero vim_cv_terminfo=yes vim_cv_toupper_broken=no vim_cv_tty_group=world $DIR/sources/vim74/configure --host=$TARGET --target=$TARGET --prefix=$CROSSPREFIX --with-tlib=ncurses --enable-gui=no --disable-gtktest --disable-xim --with-features=normal --disable-gpm --without-x --disable-netbeans --enable-multibyte || cmd_error
    make -j || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi
    
popd > /dev/null
