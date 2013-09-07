DISK=lyos-disk.img
SRCDIR=./
MOUNT_POINT=/mnt/lyos-root
LOOP_DEVICE=loop1

umount $MOUNT_POINT
kpartx -d /dev/mapper/hda
dmsetup remove hda
losetup -d /dev/$LOOP_DEVICE
