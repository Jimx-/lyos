#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -z "$SUBARCH" ]; then
    SUBARCH=i686
    ARCH=x86
fi  

if [ -z "$ARCH" ]; then
    echo "ARCH must be set"
    exit 1
fi

PREFIX=$DIR/local
CROSSPREFIX=/usr

DESTDIR=`readlink -f $DIR/../obj/destdir.$ARCH/`
export SYSROOT=$DESTDIR

TARGET=$SUBARCH-elf-lyos

export TOOLCHAIN=$SYSROOT/usr
