#!/usr/bin/env bash
#
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

export PATH=$DIR/local/bin:$PATH
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

export PKG_CONFIG_PATH_FOR_BUILD="$PREFIX/lib/pkgconfig"

echo "#!/bin/sh

export PKG_CONFIG_PATH=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}

exec pkg-config \"\$@\"" > $DIR/local/bin/$TARGET-pkg-config
chmod +x $DIR/local/bin/$TARGET-pkg-config
