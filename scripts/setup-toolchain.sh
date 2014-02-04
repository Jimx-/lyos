#!/bin/bash

if [ -f /etc/debian_version ]; then
    sudo apt-get install python nasm build-essential wget qemu autoconf automake libmpfr-dev libmpc-dev libgmp3-dev texinfo
fi

pushd toolchain > /dev/null
python setup.py
popd > /dev/null
