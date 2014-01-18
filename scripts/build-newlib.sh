#!/bin/bash

if [ -f /etc/debian_version ]; then
    sudo apt-get install python nasm build-essential wget libmpfr-dev libmpc-dev libgmp3-dev
fi

pushd toolchain > /dev/null
python build-newlib.py
popd > /dev/null
