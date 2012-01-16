#!/bin/sh

ARCH=$1
VERSION=$2
CC=$3

( 
  echo \/\* Generated by gencompile.sh \*\/
  echo \#define ARCH $ARCH
  echo
  echo \#define UTS_MACHINE \"$ARCH\"
  echo \#define UTS_VERSION \"$VERSION\"
  echo 
  echo \#define LYOS_COMPILE_TIME \"`date +%F\ %T`\" 
  echo \#define LYOS_COMPILE_BY \"`whoami`\"
  echo \#define LYOS_COMPILE_HOST \"`hostname`\"
  echo \#define LYOS_COMPILER \"`$CC -v 2>&1 | tail -n 1`\"
) > ./include/lyos/compile.h
