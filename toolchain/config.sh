#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SUBARCH=$(uname -m | sed -e s/sun4u/sparc64/ \
        -e s/arm.*/arm/ -e s/sa110/arm/ \
        -e s/s390x/s390/ -e s/parisc64/parisc/ \
        -e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
        -e s/sh[234].*/sh/ )

OPTIND=1
while getopts "m:" arg
do
        case $arg in
             m)
                SUBARCH=${OPTARG}
                ;;
             ?)
                echo "unkonw argument"
                exit 1
                ;;
        esac
done

ARCH=$SUBARCH

if [ $ARCH = "i686" ]; then
    ARCH=x86
fi

if [ $ARCH = "x86_64" ]; then
    ARCH=x86
fi

if [ $ARCH = "aarch64" ]; then
    ARCH=arm64
fi

if [ $ARCH = "riscv64" ]; then
    ARCH=riscv
fi

export SUBARCH=$SUBARCH ARCH=$ARCH

PREFIX=$DIR/local
CROSSPREFIX=/usr

DESTDIR=`readlink -f $DIR/../obj/destdir.$SUBARCH/`
export SYSROOT=$DESTDIR

TARGET=$SUBARCH-elf-lyos

export TOOLCHAIN=$SYSROOT/usr
