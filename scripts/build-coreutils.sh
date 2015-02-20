#!/bin/bash

if [ -f /etc/debian_version ]; then
    sudo apt-get install python nasm build-essential wget libmpfr-dev libmpc-dev libgmp3-dev
fi


SUBARCH=$(uname -m | sed -e s/sun4u/sparc64/ \
		-e s/arm.*/arm/ -e s/sa110/arm/ \
		-e s/s390x/s390/ -e s/parisc64/parisc/ \
		-e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
		-e s/sh[234].*/sh/ )
ARCH=$SUBARCH

while getopts "m:" arg
do
        case $arg in
             m)
                ARCH=${OPTARG}
                ;;
             ?)
                echo "unkonw argument"
                exit 1
                ;;
        esac
done

if [ $ARCH = "i686" ]; then
	ARCH=x86
fi

export SUBARCH=$SUBARCH ARCH=$ARCH

pushd toolchain > /dev/null
python build-coreutils.py
popd > /dev/null
