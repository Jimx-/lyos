#include <asm/page.h>

.section ".bootmods","a",@progbits

  .globl _bootmod_mm_start, _bootmod_mm_end
  .align ARCH_PG_SHIFT
_bootmod_mm_start:
  .incbin "mm"
_bootmod_mm_end:

  .globl _bootmod_pm_start, _bootmod_pm_end
  .align ARCH_PG_SHIFT
_bootmod_pm_start:
  .incbin "pm"
_bootmod_pm_end:

  .globl _bootmod_servman_start, _bootmod_servman_end
  .align ARCH_PG_SHIFT
_bootmod_servman_start:
  .incbin "servman"
_bootmod_servman_end:

  .globl _bootmod_devman_start, _bootmod_devman_end
  .align ARCH_PG_SHIFT
_bootmod_devman_start:
  .incbin "devman"
_bootmod_devman_end:

  .globl _bootmod_sched_start, _bootmod_sched_end
  .align ARCH_PG_SHIFT
_bootmod_sched_start:
  .incbin "sched"
_bootmod_sched_end:

  .globl _bootmod_vfs_start, _bootmod_vfs_end
  .align ARCH_PG_SHIFT
_bootmod_vfs_start:
  .incbin "vfs"
_bootmod_vfs_end:

  .globl _bootmod_systask_start, _bootmod_systask_end
  .align ARCH_PG_SHIFT
_bootmod_systask_start:
  .incbin "systask"
_bootmod_systask_end:

  .globl _bootmod_tty_start, _bootmod_tty_end
  .align ARCH_PG_SHIFT
_bootmod_tty_start:
  .incbin "tty"
_bootmod_tty_end:

  .globl _bootmod_ramdisk_start, _bootmod_ramdisk_end
  .align ARCH_PG_SHIFT
_bootmod_ramdisk_start:
  .incbin "ramdisk"
_bootmod_ramdisk_end:

  .globl _bootmod_initfs_start, _bootmod_initfs_end
  .align ARCH_PG_SHIFT
_bootmod_initfs_start:
  .incbin "initfs"
_bootmod_initfs_end:

  .globl _bootmod_sysfs_start, _bootmod_sysfs_end
  .align ARCH_PG_SHIFT
_bootmod_sysfs_start:
  .incbin "sysfs"
_bootmod_sysfs_end:

  .globl _bootmod_ipc_start, _bootmod_ipc_end
  .align ARCH_PG_SHIFT
_bootmod_ipc_start:
  .incbin "ipc"
_bootmod_ipc_end:

  .globl _bootmod_init_start, _bootmod_init_end
  .align ARCH_PG_SHIFT
_bootmod_init_start:
  .incbin "init"
_bootmod_init_end:
