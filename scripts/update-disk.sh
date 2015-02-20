#!/bin/bash

if [[ $EUID -ne 0 ]]; then
    echo -e "\033[1;31mYou're going to need to run this as root\033[0m" 1>&2
    echo "Additionally, verify that /dev/loop4 is available and that" 1>&2
    echo "/mnt is available for mounting; otherwise, modify the script" 1>&2
    echo "to use alternative loop devices or mount points as needed." 1>&2
    exit 1
fi

SUBARCH=$(uname -m | sed -e s/sun4u/sparc64/ \
		-e s/arm.*/arm/ -e s/sa110/arm/ \
		-e s/s390x/s390/ -e s/parisc64/parisc/ \
		-e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
		-e s/sh[234].*/sh/ )
ARCH=$SUBARCH

while getopts "m:" arg
do
        case $arg in
             m)
                ARCH=${OPTARG}
                ;;
             ?)
                echo "unkonw argument"
                exit 1
                ;;
        esac
done

if [ $ARCH = "i686" ]; then
	ARCH=x86
fi

export SUBARCH=$SUBARCH ARCH=$ARCH

DISK=lyos-disk.img
SRCDIR=./
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=loop1

source $SRCDIR/.config

# Here's where we need to be root.
losetup /dev/$LOOP_DEVICE $DISK

IMAGE_SIZE=`wc -c < $DISK`
IMAGE_SIZE_SECTORS=`expr $IMAGE_SIZE / 512`
MAPPER_LINE="0 $IMAGE_SIZE_SECTORS linear 7:1 0"

echo "$MAPPER_LINE" | dmsetup create hda

kpartx -a /dev/mapper/hda

mount /dev/mapper/hda1 /$MOUNT_POINT

echo "Installing kernel."
if [[ $CONFIG_COMPRESS_GZIP == "y" ]]
then
	cp -r $SRCDIR/arch/x86/lyos.gz /$MOUNT_POINT/boot/
else
	cp -r $SRCDIR/arch/x86/lyos.bin /$MOUNT_POINT/boot/
fi

cp -rf obj/destdir.$ARCH/boot/* /$MOUNT_POINT/boot/
cp -rf obj/destdir.$ARCH/bin/service /$MOUNT_POINT/bin/service 
cp -rf obj/destdir.$ARCH/sbin/* /$MOUNT_POINT/sbin/ 
cp -rf obj/destdir.$ARCH/usr/bin/* /$MOUNT_POINT/usr/bin/
cp -rf sysroot/etc/* /$MOUNT_POINT/etc/
cp -rf sysroot/boot/* /$MOUNT_POINT/boot/

umount $MOUNT_POINT
kpartx -d /dev/mapper/hda
dmsetup remove hda
losetup -d /dev/$LOOP_DEVICE

if [ -n "$SUDO_USER" ] ; then
    echo "Reassigning permissions on disk image to $SUDO_USER"
    chown $SUDO_USER:$SUDO_USER $DISK
fi

echo "Done. You can boot the disk image with qemu now."
