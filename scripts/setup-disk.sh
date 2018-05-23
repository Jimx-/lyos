#!/bin/bash

if [[ $EUID -ne 0 ]]; then
    echo -e "\033[1;31mYou're going to need to run this as root\033[0m" 1>&2
    echo "Additionally, verify that /dev/loop1 is available and that" 1>&2
    echo "/mnt is available for mounting; otherwise, modify the script" 1>&2
    echo "to use alternative loop devices or mount points as needed." 1>&2
    exit 1
fi

DISK=lyos-disk.img
SRCDIR=./
SIZE=262144
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=loop2

source $SRCDIR/.config

type kpartx >/dev/null 2>&1 || { echo "Trying to install kpartx..."; apt-get install kpartx; }

# Create a 1GiB blank disk image.
#dd if=/dev/zero of=$DISK bs=4096 count=$SIZE

echo "Partitioning..."
# Partition it with fdisk.
cat ./scripts/fdisk.conf | fdisk $DISK
sync

echo "Done partition."

LOOPRAW=`losetup -f`
losetup $LOOPRAW $DISK
TMP=`kpartx -av $DISK`
TMP2=${TMP/add map /}
LOOP=${TMP2%%p1 *}
LOOPDEV=/dev/${LOOP}
LOOPMAP=/dev/mapper/${LOOP}p1

mkfs.ext2 $LOOPMAP

mount $LOOPMAP /$MOUNT_POINT

echo "Installing sysroot..."
cp -rf $SRCDIR/sysroot/* /$MOUNT_POINT/
cp -rf $SRCDIR/obj/destdir.x86/* /$MOUNT_POINT/
chown 1000:1000 /$MOUNT_POINT/home/jimx
sync

echo "Creating devices..."
debugfs -f ./scripts/create-dev.conf -w $LOOPMAP

echo "Installing grub..."
grub-install --target=i386-pc --boot-directory=$MOUNT_POINT/boot $LOOPRAW

umount $MOUNT_POINT
kpartx -d $LOOPMAP
dmsetup remove $LOOPMAP
losetup -d $LOOPDEV
losetup -d $LOOPRAW

if [ -n "$SUDO_USER" ] ; then
    echo "Reassigning permissions on disk image to $SUDO_USER"
    chown $SUDO_USER:$SUDO_USER $DISK
fi

echo "Done. You can boot the disk image with qemu now."
