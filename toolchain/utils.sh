#!/usr/bin/env bash

function download () {
    echo "Downloading $1 from $2/$3..."
    if [ ! -f "$3" ]; then
        wget "$2/$3"
    else
        echo "$1 already downloaded"
    fi
}

function unzip() {
    echo "Decompressing $1..."
    if [ ! -d "$2" ]; then
        tar -xf $1
    else
        echo "$1 has already been decompressed"
    fi
}

function patc() {
    echo "Patching $1..."
    pushd "$1" > /dev/null
    if [ ! -f ".patched" ]; then
        patch -p1 < $DIR/patches/$1.patch
        touch .patched
    else
        echo "$1 has already been patched"
    fi
    popd > /dev/null
}

function install_newlib () {
    cp -rf $DIR/patches/newlib/lyos $1/newlib/libc/sys/
}

function cmd_error() {
    echo -e "\033[1;31mBuild failed.\033[0m"
    exit 1
}
