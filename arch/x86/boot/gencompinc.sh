#!/bin/sh

LOADER=$1
KERNEL=$1

( 
  echo \#define LYOS_COMPILE_TIME \"`date +%F\ %T`\" 
) > ./include/lyos/compile.h
