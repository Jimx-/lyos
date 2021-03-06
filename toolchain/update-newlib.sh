#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/utils.sh

pushd "$DIR" > /dev/null

if [ ! -d "sources" ]; then 
    mkdir sources
fi

pushd sources > /dev/null

rm -rf newlib-3.0.0
unzip "newlib-3.0.0.tar.gz" "newlib-3.0.0"
patc "newlib-3.0.0"
install_newlib "newlib-3.0.0"

popd > /dev/null

popd > /dev/null
