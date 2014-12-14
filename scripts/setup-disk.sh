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
SIZE=65536
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=loop1

source $SRCDIR/.config

type kpartx >/dev/null 2>&1 || { echo "Trying to install kpartx..."; apt-get install kpartx; }

# Create a 1GiB blank disk image.
dd if=/dev/zero of=$DISK bs=4096 count=$SIZE

echo "Partitioning..."
# Partition it with fdisk.
cat ./scripts/fdisk.conf | fdisk $DISK

echo "Done partition."

# Here's where we need to be root.
losetup /dev/$LOOP_DEVICE $DISK

IMAGE_SIZE=`wc -c < $DISK`
IMAGE_SIZE_SECTORS=`expr $IMAGE_SIZE / 512`
MAPPER_LINE="0 $IMAGE_SIZE_SECTORS linear 7:1 0"

echo "$MAPPER_LINE" | dmsetup create hda

kpartx -a /dev/mapper/hda

mkfs.ext2 /dev/mapper/hda1

mount /dev/mapper/hda1 /$MOUNT_POINT

echo "Installing sysroot..."
cp -rf $SRCDIR/sysroot/* /$MOUNT_POINT/
cp -rf $SRCDIR/obj/destdir.x86/* /$MOUNT_POINT/
sync

echo "Creating devices..."
debugfs -f ./scripts/create-dev.conf -w /dev/mapper/hda1

echo "Installing grub..."
grub-install --boot-directory=/$MOUNT_POINT/boot /dev/$LOOP_DEVICE

umount $MOUNT_POINT
kpartx -d /dev/mapper/hda
dmsetup remove hda
losetup -d /dev/$LOOP_DEVICE

if [ -n "$SUDO_USER" ] ; then
    echo "Reassigning permissions on disk image to $SUDO_USER"
    chown $SUDO_USER:$SUDO_USER $DISK
fi

echo "Done. You can boot the disk image with qemu now."
