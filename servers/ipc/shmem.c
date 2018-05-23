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

#include <lyos/compile.h>
#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/sysutils.h>
#include <lyos/config.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <sys/mman.h>
#include <sys/shm.h>

#include "page.h"

#include "const.h"
#include "proto.h"

struct shm {
    struct shmid    shmid;
    vir_bytes       page;
};

#define     SHM_INUSE   0x0800

PRIVATE struct shm  shm_list[SHMMNI];
PRIVATE int         shm_count = 0;

PRIVATE struct shm* shm_find_key(key_t key)
{
    unsigned int i;

    if (key == IPC_PRIVATE) return NULL;

    for (i = 0; i < shm_count; i++) {
        if (!(shm_list[i].shmid.perm.mode & SHM_INUSE)) continue;
        if (shm_list[i].shmid.perm.key == key)
            return &shm_list[i];
    }

    return NULL;
}

PRIVATE struct shm* shm_find_id(int id)
{
    struct shm* shm;
    unsigned int i;

    i = IPCID_TO_IX(id);
    if (i >= shm_count) return NULL;

    shm = &shm_list[i];
    if (!(shm->shmid.perm.mode & SHM_INUSE)) return NULL;
    if (!(shm->shmid.perm.seq != IPCID_TO_SEQ(id))) return NULL;

    return shm;
}

PUBLIC int do_shmget(MESSAGE* msg)
{
    key_t key;
    size_t old_size, size;
    int i, flags;
    struct shm* shm;
    void* page;
    endpoint_t endpoint = msg->source;

    key = msg->IPC_KEY;
    old_size = size = msg->IPC_SIZE;
    flags = msg->IPC_FLAGS;
    shm = shm_find_key(key);

    if (shm) {
        if (!check_perm(&shm->shmid.perm, endpoint, flags)) {
            return EACCES;
        }
        if ((flags & IPC_CREAT) && (flags & IPC_EXCL)) {
            return EEXIST;
        }
        if (size && shm->shmid.size < size) {
            return EINVAL;
        }

        i = shm - shm_list;
    } else {
        if (!(flags & IPC_CREAT)) {
            return ENOENT;
        }
        if (size <= 0) {
            return EINVAL;
        }
        size = roundup(size, ARCH_PG_SIZE);

        for (i = 0; i < SHMMNI; i++) {
            if (!(shm_list[i].shmid.perm.mode & SHM_INUSE)) break;
        }
        if (i == SHMMNI) {
            return ENOSPC;
        }

        page = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
        if (page == MAP_FAILED)
            return ENOMEM;
        memset(page, 0, size);

        uid_t uid;
        gid_t gid;
        pid_t pid = get_epinfo(endpoint, &uid, &gid);

        shm = &shm_list[i];
        int seq = shm->shmid.perm.seq;
        memset(shm, 0, sizeof(*shm));
        shm->shmid.perm.key = key;
        shm->shmid.perm.uid = uid;
        shm->shmid.perm.gid = gid;
        shm->shmid.perm.cuid = uid;
        shm->shmid.perm.cgid = gid;
        shm->shmid.perm.mode = SHM_INUSE | (flags & 0777);
        shm->shmid.perm.seq = (seq + 1) & 0x7fff;
        shm->shmid.size = old_size;
        shm->shmid.cpid = pid;
        shm->shmid.lpid = 0;
        shm->page = (vir_bytes) page;

        if (i == shm_count) shm_count++;
    }

    msg->IPC_RETID = IXSEQ_TO_IPCID(i, shm->shmid.perm.seq);
    return 0;
}

PUBLIC int do_shmat(MESSAGE* msg)
{
    int id, flags;
    vir_bytes addr;

    id = msg->IPC_ID;
    addr = (vir_bytes) msg->IPC_ADDR;
    flags = msg->IPC_FLAGS;

    if (addr % ARCH_PG_SIZE) {
        if (flags & SHM_RND) {
            addr -= (addr % ARCH_PG_SIZE);
        } else {
            return EINVAL;
        }
    }

    struct shm* shm = shm_find_id(id);
    if (!shm) return EINVAL;

    int mask = 0;
    if (flags & SHM_RDONLY) mask = IPC_R;
    else mask = IPC_R | IPC_W;
    if (!check_perm(&shm->shmid.perm, msg->source, mask)) return EACCES;
}
