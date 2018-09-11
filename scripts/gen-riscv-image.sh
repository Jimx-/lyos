#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

RISCVPK=$DIR/download/riscv-pk

source $DIR/activate-toolchain.sh

if [ ! -d $RISCVPK ]; then
   git clone https://github.com/riscv/riscv-pk.git $RISCVPK
fi

if [ -d "$RISCVPK/build" ]; then
    rm -rf $RISCVPK/build
fi

mkdir $RISCVPK/build
pushd $RISCVPK/build < /dev/null

../configure --prefix=$RISCV --host=riscv64-elf-lyos --with-payload=$DIR/../arch/riscv/lyos.elf
make

popd < /dev/null
