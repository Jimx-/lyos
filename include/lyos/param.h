/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _PARAM_H_
#define _PARAM_H_

#define KINFO_MAGIC     0x4B494E

#define KINFO_CMDLINE_LENGTH	1024
#define KINFO_MAXMEMMAP 		40

struct kinfo_mmap_entry {
	u64 addr;
	u64 len;
	#define KINFO_MEMORY_AVAILABLE              1
	#define KINFO_MEMORY_RESERVED               2
	u32 type;
};
     
typedef struct kinfo {
    int magic;

	int memmaps_count;
	struct kinfo_mmap_entry memmaps[KINFO_MAXMEMMAP];

	int mods_count;

	char cmdline[KINFO_CMDLINE_LENGTH];

    struct kern_log * kern_log;
} kinfo_t;

typedef int (*syscall_gate_t)(int syscall_nr, int arg0, int arg1, int arg2);

struct sysinfo {
#define SYSINFO_MAGIC   0x534946
    int magic;
    
    syscall_gate_t syscall_gate;

    kinfo_t * kinfo;
};

#endif
