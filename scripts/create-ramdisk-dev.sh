#!/bin/bash

pushd ../ramdisk > /dev/null

rm -rf dev
mkdir dev

pushd dev > /dev/null

sudo mknod hd1 b 3 0
sudo mknod hd1a b 3 1
sudo mknod console c 4 0
sudo mknod tty1 c 4 1
sudo mknod tty2 c 4 2
sudo mknod tty3 c 4 3


popd > /dev/null

popd > /dev/null
