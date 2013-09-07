#!/bin/bash

DISK=lyos-disk.img
SRCDIR=./
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=loop1

# Here's where we need to be root.
losetup /dev/$LOOP_DEVICE $DISK

IMAGE_SIZE=`wc -c < $DISK`
IMAGE_SIZE_SECTORS=`expr $IMAGE_SIZE / 512`
MAPPER_LINE="0 $IMAGE_SIZE_SECTORS linear 7:1 0"

echo "$MAPPER_LINE" | dmsetup create hda

kpartx -a /dev/mapper/hda

mount /dev/mapper/hda1 /$MOUNT_POINT
