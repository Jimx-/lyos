#!/bin/bash

if [[ $EUID -ne 0 ]]; then
    echo -e "\033[1;31mYou're going to need to run this as root\033[0m" 1>&2
    echo "Additionally, verify that /dev/loop4 is available and that" 1>&2
    echo "/mnt is available for mounting; otherwise, modify the script" 1>&2
    echo "to use alternative loop devices or mount points as needed." 1>&2
    exit 1
fi

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

echo "Installing kernel."
cp -r $SRCDIR/arch/x86/kernel.bin /$MOUNT_POINT/boot/

umount $MOUNT_POINT
kpartx -d /dev/mapper/hda
dmsetup remove hda
losetup -d /dev/$LOOP_DEVICE

if [ -n "$SUDO_USER" ] ; then
    echo "Reassigning permissions on disk image to $SUDO_USER"
    chown $SUDO_USER:$SUDO_USER $DISK
fi

echo "Done. You can boot the disk image with qemu now."
