This is Lyos <http://lyos.googlecode.com>

WHAT IS LYOS
============
	
Lyos is an open source microkernel multitasking operating system, it runs
on 32-bit x86-based PCs, it supports IDE harddisk and floppy disk.

It is distributed under the GNU General Public License.

FEATURES
========
Lyos has many standard features:

* Microkernel
	
    - Kernel only handles message passing, interrupts and task scheduling, other things like reading/writing files, memory managing and device operations are handled by servers.


* Multitasking

* TTYs

	- Multi-TTY support
    
* Interprocess Communication

* Device drivers
	- Hard disk drive support
    - Ramdisk support
    - Floppy(under development)
    
* Virtual file system
	- Ext2 filesystem support(under development)
    
* Memory management


COMPILATION AND INSTALLATION
============================

1. Download the source
	- You can get the source with git: 
        
        ```git clone git://github.com/Jimx-/lyos.git lyos```

    - If you download the bzip file, unpack it:

        ```tar -jxvf lyos-0.3.X.tar.bz2```

2. Setup toolchain

	- Under the lyos' root directory, run:

        ```sudo bash ./scripts/setup-toolchain.sh```

    - Activate the toolchain:
    
    	```source ./scripts/activate-toolchain.sh```
3. Compile Lyos

	- Remove all the object files:

        ```make mrproper```

    - Configure Lyos(Optional):

        ```make config```
        or
        ```make menuconfig```

    - Build the kernel:
    
        ```make```

    - When this is finished, you can find the compressed kernel ```lyos.gz``` in ``` arch/<target-arch>/```

4. Create disk image

	- Create disk image:

        ```sudo bash scripts/setup-disk.sh```

5. Run Lyos

	- Get Bochs Emulator at http://bochs.sourceforge.net

    - Run bochs:
    
        ```bochs```
        
    - If you want to use qemu instead, run:

        ```kvm -hda lyos-disk.img``` 



