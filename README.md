This is lyos <https://github.com/Jimx-/lyos>

![cmdprompt][1]

WHAT IS LYOS 简介
===============
    
Lyos is an open source microkernel multitasking operating system, it runs
on 32-bit x86-based PCs. It is distributed under the GNU General Public License.

Lyos是一个开源的微内核多任务操作系统，运行于32位x86 PC上， 使用GNU GPL协议。

FEATURES 功能
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
    - Dash, GNU Binutils, gcc...

Lyos具有的功能：

* 微内核
    - 内核仅处理消息传递，中断和任务调度。内存管理，文件/设备操作由运行在用户空间的驱动程序完成
* 多任务
    - BFS调度器
* 对称多处理(SMP)
* ELF加载
* 文件系统
    - Ext2
    - procfs, sysfs等
* 内存管理
    - 基于虚拟内存
* 用户空间
    - Newlib
    - Dash, GNU Binutils, gcc等

COMPILATION AND INSTALLATION 编译与安装
======================================

1. Get the source ```git clone git://github.com/Jimx-/lyos.git```

2. Setup toolchain, run ``./scripts/setup-toolchain.sh``
3. Compile Lyos, run
    `make mrproper`
    `make menuconfig`
    `make`

4. Create disk image, run```make setup-disk```

5. Run Lyos with QEMU, run ```make kvm``` 

  [1]: http://jimx.1x.net/images/screenshot-6.png
  