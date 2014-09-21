#!/bin/sh

if [ ! -f $PWD/.version ]
then
	VERSION=1
else
	VERSION=`expr 0\`cat .version\` + 1`
fi

echo $VERSION > .version
