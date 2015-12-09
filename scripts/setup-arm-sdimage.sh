#!/bin/bash

if [[ $EUID -ne 0 ]]; then
    echo -e "\033[1;31mYou're going to need to run this as root\033[0m" 1>&2
    echo "Additionally, verify that /dev/loop1 is available and that" 1>&2
    echo "/mnt is available for mounting; otherwise, modify the script" 1>&2
    echo "to use alternative loop devices or mount points as needed." 1>&2
    exit 1
fi

IMG=lyos-sdimage.img
SRCDIR=./
SIZE=262144
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=/dev/loop1
MLO=./scripts/download/MLO
UBOOT=./scripts/download/u-boot.img

source $SRCDIR/.config

apt-get install u-boot-tools

# Create a 1GiB blank disk image.
#dd if=/dev/zero of=$IMG bs=4096 count=$SIZE

echo "Partitioning..."
# Partition it with fdisk.
#cat ./scripts/fdisk-sd.conf | fdisk $IMG

echo "Done partition."

# Setting up boot partition
losetup -o $[2048*512] --sizelimit $[143359*512] $LOOP_DEVICE $IMG
mkfs.vfat -F32 $LOOP_DEVICE
mount $LOOP_DEVICE $MOUNT_POINT

if [ ! -f $MLO ];
then
   wget http://downloads.angstrom-distribution.org/demo/beaglebone/MLO -O $MLO
fi
if [ ! -f $UBOOT ];
then
   wget http://downloads.angstrom-distribution.org/demo/beaglebone/u-boot.img -O $UBOOT
fi

mkimage -A arm -O linux -T multi -C none -a 0x80200000 -e 0x80200000 -n 'Lyos/arm' -d  obj/destdir.arm/boot/lyos.bin obj/destdir.arm/boot/lyos.ub

cp $MLO $UBOOT ./scripts/uEnv.txt $MOUNT_POINT
cp -rf obj/destdir.arm/boot/* $MOUNT_POINT
sync

umount $MOUNT_POINT
losetup -d $LOOP_DEVICE

if [ -n "$SUDO_USER" ] ; then
    echo "Reassigning permissions on disk image to $SUDO_USER"
    chown $SUDO_USER:$SUDO_USER $IMG
fi

echo "Done. You can boot the disk image with qemu now."
