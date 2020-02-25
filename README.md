This is lyos <https://github.com/Jimx-/lyos>
[![Build Status](https://ci2.jimx.site:8080/buildStatus/icon?job=lyos)](https://ci2.jimx.site:8080/job/lyos/)

![procfs][1]

WHAT IS LYOS
===============
    
Lyos is an open source microkernel multitasking operating system, it runs
on 32-bit x86-based PCs. It is distributed under the GNU General Public License.

FEATURES
============
Lyos has many standard features:

* Microkernel
    - Kernel only handles message passing, interrupts and task scheduling, other things like reading/writing files, memory managing and device operations are handled by servers.
    - Servers are single executables(not modules) that are loaded by kernel at boot time, running in user privilege level and their own address space.
* Multitasking
    - BFS scheduler 
* Symmetric multiprocessing
* ELF loading
* Interprocess Communication
    - Servers and user programs communicate using fixed length message(40 bytes)
    - Interrupts are converted into messages and sent to userspace servers
    - Unix signals
* Virtual file system
    - Ext2 filesystem support(under development)
    - procfs, sysfs...
* Memory management
    - Virtual memory based
    - Higher half kernel
* Userspace
    - Newlib
    - Dash, GNU Binutils, gcc, GNU Bash...

COMPILATION AND INSTALLATION
======================================

1. Get the source ```git clone git://github.com/Jimx-/lyos.git```

2. Setup toolchain, run ``./scripts/setup-toolchain.sh``
3. Compile Lyos, run
    `make mrproper`
    `make menuconfig`
    `make`

4. Create disk image, run```make setup-disk```

5. Run Lyos with QEMU, run ```make kvm``` 

  [1]: http://i.imgur.com/xRTPGRJ.png
  
