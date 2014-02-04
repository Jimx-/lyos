#!/bin/bash

pushd ../ramdisk

rm -rf dev
mkdir dev

pushd dev

sudo mknod hd1 b 3 0
sudo mknod hd1a b 3 1

popd

popd
