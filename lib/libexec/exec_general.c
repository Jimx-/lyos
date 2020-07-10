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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/sysutils.h>
#include "libexec.h"
#include "lyos/vm.h"
#include "sys/mman.h"

int libexec_allocmem(struct exec_info* execi, void* vaddr, size_t len)
{
    if (mmap_for(execi->proc_e, vaddr, len, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_POPULATE, -1,
                 0) == MAP_FAILED) {
        return ENOMEM;
    }

    return 0;
}

int libexec_allocmem_prealloc(struct exec_info* execi, void* vaddr, size_t len)
{
    if (mmap_for(execi->proc_e, vaddr, len, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1,
                 0) == MAP_FAILED) {
        return ENOMEM;
    }

    return 0;
}

int libexec_clearmem(struct exec_info* execi, void* vaddr, size_t len)
{
#define _ZERO_BUF_LEN 512
    char zerobuf[_ZERO_BUF_LEN];
    memset(zerobuf, 0, sizeof(zerobuf));

    while (len > 0) {
        int copy_len = min(len, _ZERO_BUF_LEN);
        data_copy(execi->proc_e, vaddr, SELF, zerobuf, copy_len);
        vaddr += copy_len;
        len -= copy_len;
    }

    return 0;
}

int libexec_clearproc(struct exec_info* execi)
{
    return procctl(execi->proc_e, PCTL_CLEARMEM);
}
