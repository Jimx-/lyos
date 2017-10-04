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
sudo mknod tty4 c 4 4
sudo mknod tty5 c 4 5
sudo mknod tty6 c 4 6
sudo mknod ttyS0 c 4 16
sudo mknod ttyS1 c 4 17
sudo mknod ttyS2 c 4 18
sudo mknod fb0 c 29 0

popd > /dev/null

popd > /dev/null
