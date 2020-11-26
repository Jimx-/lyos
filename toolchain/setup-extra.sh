#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_EVERYTHING:=false}
: ${BUILD_GDBM:=false}
: ${BUILD_XZ:=false}
: ${BUILD_LIBRESSL:=false}
: ${BUILD_PYTHON:=false}

if $BUILD_EVERYTHING; then
    BUILD_GDBM=true
    BUILD_XZ=true
    BUILD_LIBRESSL=true
    BUILD_PYTHON=true
fi

echo "Building extra packages... (sysroot: $SYSROOT, prefix: $PREFIX, crossprefix: $CROSSPREFIX, target: $TARGET)"

if [ ! -d "build" ]; then
    mkdir build
fi

pushd build > /dev/null

. $DIR/activate.sh

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

# Build python
if $BUILD_PYTHON; then
    if [ ! -d "python-$SUBARCH" ]; then
        mkdir python-$SUBARCH
    fi

    pushd $DIR/sources/Python-3.8.2 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fi
    popd > /dev/null

    pushd python-$SUBARCH > /dev/null

    echo "ac_cv_file__dev_ptmx=yes
          ac_cv_file__dev_ptc=no" > python-config-site

    CONFIG_SITE=python-config-site \
      $DIR/sources/Python-3.8.2/configure --host=$TARGET --build=x86_64-linux-gnu \
        --prefix=$CROSSPREFIX \
        --with-sysroot=$SYSROOT --enable-shared --with-system-ffi --with-system-expat \
        --disable-ipv6 --without-ensurepip
    make -j4 || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

popd > /dev/null
