#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh
. $DIR/utils.sh

: ${BUILD_EVERYTHING:=false}
: ${BUILD_GDBM:=false}
: ${BUILD_XZ:=false}
: ${BUILD_LIBRESSL:=false}
: ${BUILD_PYTHON:=false}
: ${BUILD_NET_TOOLS:=false}
: ${BUILD_LIBTASN1:=false}
: ${BUILD_P11_KIT:=false}
: ${BUILD_OPENSSL:=false}
: ${BUILD_WGET:=false}
: ${BUILD_CA_CERTIFICATES:=false}

if $BUILD_EVERYTHING; then
    BUILD_GDBM=true
    BUILD_XZ=true
    BUILD_LIBRESSL=true
    BUILD_PYTHON=true
    BUILD_NET_TOOLS=true
    BUILD_LIBTASN1=true
    BUILD_P11_KIT=true
    BUILD_OPENSSL=true
    BUILD_WGET=true
    BUILD_CA_CERTIFICATES=true
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
    make -j$PARALLELISM || cmd_error
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
    make -j$PARALLELISM || cmd_error
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

    CONFIG_SITE=python-config-site PATH=$DIR/tools/python-3.8/bin:$PATH \
      $DIR/sources/Python-3.8.2/configure --host=$TARGET --build=x86_64-linux-gnu \
        --prefix=$CROSSPREFIX \
        --with-sysroot=$SYSROOT --enable-shared --with-system-ffi --with-system-expat \
        --disable-ipv6 --without-ensurepip
    PATH=$DIR/tools/python-3.8/bin:$PATH make -j$PARALLELISM || cmd_error
    PATH=$DIR/tools/python-3.8/bin:$PATH make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build net-tools
if $BUILD_NET_TOOLS; then
    pushd $DIR/sources/net-tools-2.10 > /dev/null
    make clean
    CC=$TARGET-gcc make
    DESTDIR=$SYSROOT/usr make install
    popd > /dev/null
fi

# Build libtasn1
if $BUILD_LIBTASN1; then
    if [ ! -d "libtasn1-$SUBARCH" ]; then
        mkdir libtasn1-$SUBARCH
    fi

    pushd $DIR/sources/libtasn1-4.18.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fis
    popd > /dev/null

    pushd libtasn1-$SUBARCH > /dev/null
    $DIR/sources/libtasn1-4.18.0/configure --host=$TARGET --prefix=$CROSSPREFIX --disable-static --disable-doc
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build p11-kit
if $BUILD_P11_KIT; then
    if [ ! -d "p11-kit-$SUBARCH" ]; then
        mkdir p11-kit-$SUBARCH
    fi

    pushd $DIR/sources/p11-kit-0.24.0 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fis
    popd > /dev/null

    pushd p11-kit-$SUBARCH > /dev/null
    $DIR/sources/p11-kit-0.24.0/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT \
                                          --sysconfdir=/etc --with-trust-paths=/etc/pki/anchors --without-systemd \
                                          --disable-doc-html --disable-nls
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    ln -sfv /usr/libexec/p11-kit/trust-extract-compact $SYSROOT/usr/bin/update-ca-certificates
    popd > /dev/null
fi

# Build OpenSSL
if $BUILD_OPENSSL; then
    if [ ! -d "openssl-$SUBARCH" ]; then
        mkdir openssl-$SUBARCH
    fi

    pushd openssl-$SUBARCH > /dev/null
    CC=$TARGET-gcc CXXX=$TARGET-g++ $DIR/sources/openssl-OpenSSL_1_1_1m/Configure \
                   --prefix=$CROSSPREFIX --openssldir=/etc/ssl --libdir=lib \
                   $TARGET shared zlib-dynamic no-afalgeng
    make -j$PARALLELISM || cmd_error
    sed -i '/INSTALL_LIBS/s/libcrypto.a libssl.a//' Makefile
    make DESTDIR=$SYSROOT MANSUFFIX=ssl install || cmd_error
    popd > /dev/null
fi

# Build wget
if $BUILD_WGET; then
    if [ ! -d "wget-$SUBARCH" ]; then
        mkdir wget-$SUBARCH
    fi

    pushd $DIR/sources/wget-1.21.1 > /dev/null
    PATH=$DIR/tools/autoconf-2.69/bin:$DIR/tools/automake-1.15/bin:$PATH autoreconf -fis
    popd > /dev/null

    pushd wget-$SUBARCH > /dev/null
    $DIR/sources/wget-1.21.1/configure --host=$TARGET --prefix=$CROSSPREFIX --with-sysroot=$SYSROOT \
                                       --sysconfdir=/etc --disable-nls --with-ssl=openssl --with-openssl
    make -j$PARALLELISM || cmd_error
    make DESTDIR=$SYSROOT install || cmd_error
    popd > /dev/null
fi

# Build ca-certificates
if $BUILD_CA_CERTIFICATES; then
    if [ -d "ca-certificates-$SUBARCH" ]; then
        rm -rf ca-certificates-$SUBARCH
    fi

    cp -r $DIR/sources/make-ca-1.10 ca-certificates-$SUBARCH

    pushd ca-certificates-$SUBARCH > /dev/null
    cp $DIR/sources/nss-3.74/nss/lib/ckfw/builtins/certdata.txt .
    make DESTDIR=$SYSROOT install || cmd_error
    ./make-ca -f -C certdata.txt -D $SYSROOT || cmd_error
    install -dm755 $SYSROOT/etc/ssl/local
    chmod 0755 $SYSROOT/etc/ssl/certs/.
    popd > /dev/null
fi

popd > /dev/null
