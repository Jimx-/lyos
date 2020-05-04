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
ARCH=x86

export SUBARCH=$SUBARCH ARCH=$ARCH

DISK=lyos-disk.img
SRCDIR=./
MOUNT_POINT=/mnt/lyos-root

source $SRCDIR/.config

LOOPRAW=`losetup -f`
losetup $LOOPRAW $DISK
TMP=`kpartx -av $DISK`
TMP2=${TMP/add map /}
LOOP=${TMP2%%p1 *}
LOOPDEV=/dev/${LOOP}
LOOPMAP=/dev/mapper/${LOOP}p1

mount $LOOPMAP /$MOUNT_POINT

echo "Installing kernel."
if [[ $CONFIG_COMPRESS_GZIP == "y" ]]
then
    cp -r $SRCDIR/arch/x86/lyos.gz /$MOUNT_POINT/boot/
else
    cp -r $SRCDIR/arch/x86/lyos.elf /$MOUNT_POINT/boot/
fi

#cp -rf obj/destdir.$ARCH/boot/* /$MOUNT_POINT/boot/
#cp -rf obj/destdir.$ARCH/bin/profile /$MOUNT_POINT/bin/
cp -rf obj/destdir.$ARCH/usr/bin/bash /$MOUNT_POINT/usr/bin/bash
cp -rf obj/destdir.$ARCH/bin/* /$MOUNT_POINT/bin/
#cp -rf obj/destdir.$ARCH/sbin/procfs /$MOUNT_POINT/sbin/
cp -rf obj/destdir.$ARCH/usr/bin/getty /$MOUNT_POINT/usr/bin/
cp -rf obj/destdir.$ARCH/usr/bin/login /$MOUNT_POINT/usr/bin/
cp -rf obj/destdir.$ARCH/usr/bin/vim /$MOUNT_POINT/usr/bin/
cp -rf obj/destdir.$ARCH/usr/bin/strace /$MOUNT_POINT/usr/bin/
#cp -rf obj/destdir.$ARCH/usr/ /$MOUNT_POINT/
cp -rf obj/destdir.$ARCH/usr/lib/libg.so /$MOUNT_POINT/usr/lib/
cp -rf obj/destdir.$ARCH/usr/lib/libc.so /$MOUNT_POINT/usr/lib/
cp -rf obj/destdir.$ARCH/lib/ld-lyos.so /$MOUNT_POINT/lib/
cp -rf sysroot/etc/* /$MOUNT_POINT/etc/
cp -rf /$MOUNT_POINT/home/jimx/tsc.out .
#cp -rf sysroot/boot/* /$MOUNT_POINT/boot/
sync

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
