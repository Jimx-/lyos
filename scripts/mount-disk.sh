#!/bin/bash

DISK=lyos-disk.img
SRCDIR=./
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=loop1

source $SRCDIR/.config

LOOPRAW=`losetup -f`
losetup $LOOPRAW $DISK
TMP=`kpartx -av $DISK`
TMP2=${TMP/add map /}
LOOP=${TMP2%%p1 *}
LOOPDEV=/dev/${LOOP}
LOOPMAP=/dev/mapper/${LOOP}p1

IMAGE_SIZE=`wc -c < $DISK`
IMAGE_SIZE_SECTORS=`expr $IMAGE_SIZE / 512`
MAPPER_LINE="0 $IMAGE_SIZE_SECTORS linear /dev/$LOOP_DEVICE 0"

echo "$MAPPER_LINE" | dmsetup create hda

mount $LOOPMAP /$MOUNT_POINT
