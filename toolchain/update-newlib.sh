#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/utils.sh

pushd "$DIR" > /dev/null

if [ ! -d "sources" ]; then 
    mkdir sources
fi

pushd sources > /dev/null

rm -rf newlib-4.6.0.20260123
unzip "newlib-4.6.0.20260123.tar.gz" "newlib-4.6.0.20260123"
patc "newlib-4.6.0.20260123"
install_newlib "newlib-4.6.0.20260123"

popd > /dev/null

popd > /dev/null
