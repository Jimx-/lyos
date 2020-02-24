#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/utils.sh

pushd "$DIR" > /dev/null

if [ ! -d "sources" ]; then
    mkdir sources
fi

pushd sources > /dev/null

echo "Donwloading packages..."
download "binutils" "https://mirrors.ustc.edu.cn/gnu/binutils" "binutils-2.31.tar.gz" || cmd_error
download "mpc"  "http://www.multiprecision.org/downloads" "mpc-0.8.1.tar.gz"
download "mpfr" "http://www.mpfr.org/mpfr-2.4.2" "mpfr-2.4.2.tar.gz"
download "gmp"  "ftp://gcc.gnu.org/pub/gcc/infrastructure" "gmp-4.3.2.tar.bz2"
download "gcc" "https://mirrors.ustc.edu.cn/gnu/gcc/gcc-7.1.0" "gcc-7.1.0.tar.gz" || cmd_error
download "gcc-native" "https://mirrors.ustc.edu.cn/gnu/gcc/gcc-4.7.3" "gcc-4.7.3.tar.bz2" || cmd_error
download "newlib" "ftp://sourceware.org/pub/newlib" "newlib-3.0.0.tar.gz" || cmd_error
download "dash" "http://gondor.apana.org.au/~herbert/dash/files/" "dash-0.5.10.tar.gz" || cmd_error
download "coreutils" "https://mirrors.ustc.edu.cn/gnu/coreutils" "coreutils-8.13.tar.xz" || cmd_error
download "bash" "https://ftp.gnu.org/gnu/bash/" "bash-4.3.tar.gz"

echo "Decompressing packages..."
unzip "binutils-2.31.tar.gz" "binutils-2.31"
unzip "gmp-4.3.2.tar.bz2" "gmp-4.3.2"
unzip "mpfr-2.4.2.tar.gz" "mpfr-2.4.2"
unzip "mpc-0.8.1.tar.gz" "mpc-0.8.1"
unzip "gcc-7.1.0.tar.gz" "gcc-7.1.0"
unzip "gcc-4.7.3.tar.bz2" "gcc-4.7.3"
unzip "newlib-3.0.0.tar.gz" "newlib-3.0.0"
unzip "dash-0.5.10.tar.gz" "dash-0.5.10"
unzip "coreutils-8.13.tar.xz" "coreutils-8.13"
unzip "bash-4.3.tar.gz" "bash-4.3"

echo "Patching..."
patc "binutils-2.31"
patc "gmp-4.3.2"
patc "mpfr-2.4.2"
patc "mpc-0.8.1"
patc "gcc-7.1.0"
patc "gcc-4.7.3"
patc "newlib-3.0.0"
patc "coreutils-8.13"

echo "Installing extra files..."
install_newlib "newlib-3.0.0"

popd > /dev/null

if [ ! -d "local" ]; then
    mkdir local
fi

if [ ! -d "binary" ]; then
    mkdir binary
fi

popd > /dev/null
