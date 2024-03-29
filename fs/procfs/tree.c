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
#include "lyos/config.h"
#include "errno.h"
#include "stdio.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"

#include "global.h"
#include "proto.h"

struct proc proc[NR_TASKS + NR_PROCS];
struct pmproc pmproc[NR_PROCS];

typedef int (*rdlink_func_t)(char* ptr, size_t max, endpoint_t user_endpt);

/* slot == NR_TASKS + proc_nr */
static int slot_in_use(int slot) { return !(proc[slot].state & PST_FREE_SLOT); }

static int update_proc_table() { return get_proctab(proc); }

static int update_pmproc_table()
{
    return pm_getinfo(PM_INFO_PROCTAB, pmproc, sizeof(pmproc));
}

static int update_tables()
{
    int retval;

    if ((retval = update_proc_table()) != 0) return retval;

    if ((retval = update_pmproc_table()) != 0) return retval;

    return 0;
}

static void make_stat(struct memfs_stat* stat, int slot, int index)
{
    if (index == NO_INDEX)
        stat->st_mode =
            S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    else
        stat->st_mode = pid_files[index].mode;
    stat->st_uid = pmproc[slot - NR_TASKS].realuid;
    stat->st_gid = pmproc[slot - NR_TASKS].realgid;
}

static void build_pid_dirs()
{
    struct memfs_inode* root = memfs_get_root_inode();
    char name[NAME_MAX + 1];

    int i;
    for (i = 0; i < NR_PROCS + NR_TASKS; i++) {
        if (!slot_in_use(i)) continue;

        if (memfs_find_inode_by_index(root, i) != NULL) continue;
        pid_t pid;
        if (i < NR_TASKS) {
            pid = i - NR_TASKS;
        } else {
            pid = pmproc[i - NR_TASKS].pid;
        }

        snprintf(name, NAME_MAX + 1, "%d", pid);

        struct memfs_stat stat;
        memset(&stat, 0, sizeof(stat));
        make_stat(&stat, i, NO_INDEX);

        memfs_add_inode(root, name, i, &stat, (cbdata_t)(uintptr_t)pid);
    }
}

static int is_pid_dir(struct memfs_inode* pin)
{
    return (memfs_node_parent(pin) == memfs_get_root_inode()) &&
           (memfs_node_index(pin) != NO_INDEX);
}

static void build_one_pid_entry(struct memfs_inode* parent, const char* name,
                                int slot)
{
    if (memfs_find_inode_by_name(parent, name)) return;

    int i;
    for (i = 0; pid_files[i].name != NULL; i++) {
        if (!strcmp(pid_files[i].name, name)) {
            struct memfs_stat stat;
            memset(&stat, 0, sizeof(stat));
            make_stat(&stat, slot, i);

            memfs_add_inode(parent, name, i, &stat, (cbdata_t)0);
        }
    }
}

static void build_all_pid_entries(struct memfs_inode* parent, int slot)
{
    int i;
    struct memfs_inode* node;
    struct memfs_stat stat;

    for (i = 0; pid_files[i].name != NULL; i++) {
        node = memfs_find_inode_by_index(parent, i);
        if (node) continue;

        memset(&stat, 0, sizeof(stat));
        make_stat(&stat, slot, i);

        node =
            memfs_add_inode(parent, pid_files[i].name, i, &stat, (cbdata_t)0);
    }
}

static void build_pid_entries(struct memfs_inode* pin, const char* name)
{
    int slot = memfs_node_index(pin);

    if (name) {
        build_one_pid_entry(pin, name, slot);
    } else {
        build_all_pid_entries(pin, slot);
    }
}

int procfs_lookup_hook(struct memfs_inode* parent, const char* name,
                       cbdata_t data)
{
    static clock_t last_update = 0;

    clock_t now;
    get_ticks(&now, NULL);

    if (last_update != now) {
        update_tables();

        last_update = now;
    }

    if (parent == memfs_get_root_inode()) {
        build_pid_dirs();
    } else if (is_pid_dir(parent)) {
        build_pid_entries(parent, name);
    }

    return 0;
}

int procfs_getdents_hook(struct memfs_inode* inode, cbdata_t data)
{
    if (inode == memfs_get_root_inode()) {
        update_tables();

        build_pid_dirs();
    } else if (is_pid_dir(inode)) {
        build_pid_entries(inode, NULL);
    }

    return 0;
}

int procfs_rdlink_hook(struct memfs_inode* inode, char* ptr, size_t max,
                       endpoint_t user_endpt, cbdata_t data)
{
    int retval = EINVAL;

    if (memfs_node_index(inode) == NO_INDEX) {
        if (!data) return retval;

        retval = ((rdlink_func_t)data)(ptr, max, user_endpt);
    }

    return retval;
}

int root_self(char* ptr, size_t max, endpoint_t user_endpt)
{
    int proc_nr = ENDPOINT_P(user_endpt);

    update_tables();
    build_pid_dirs();

    if (proc_nr < 0 || proc_nr >= NR_PROCS) return EINVAL;
    if (!slot_in_use(proc_nr + NR_TASKS)) return EINVAL;

    snprintf(ptr, max, "%u", pmproc[proc_nr].pid);

    return 0;
}
