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

#ifndef _PROFILE_H_
#define _PROFILE_H_

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/proc.h>

#define KPROF_SAMPLE_BUFSIZE    (4 << 20)

#define KPROF_START     1
#define KPROF_STOP      2

struct kprof_sample {
    endpoint_t endpt;
    void* pc;
};

struct kprof_info {
    int mem_used;
    int idle_samples;
    int system_samples;
    int user_samples;
    int total_samples;
};

struct kprof_proc {
    endpoint_t endpt;
    char name[PROC_NAME_LEN];
};

int kprof(int action, size_t size, int freq, void* info, void* buf);

#endif
