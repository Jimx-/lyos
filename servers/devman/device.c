/*
    This file is part of Lyos.

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
#include "stdio.h"
#include <stdlib.h>
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/driver.h"
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include <sys/syslimits.h>

#include <libsysfs/libsysfs.h>
#include <libdevman/libdevman.h>
#include <libmemfs/libmemfs.h>

#include "type.h"
#include "proto.h"

struct device_attr_cb_data {
    struct list_head list;
    sysfs_dyn_attr_id_t sysfs_id;
    dev_attr_id_t id;
    endpoint_t owner;
    struct device* device;
};

#define DEVICE_ATTR_HASH_LOG2 7
#define DEVICE_ATTR_HASH_SIZE ((unsigned long)1 << DEVICE_ATTR_HASH_LOG2)
#define DEVICE_ATTR_HASH_MASK (((unsigned long)1 << DEVICE_ATTR_HASH_LOG2) - 1)

/* device attribute hash table */
static struct list_head device_attr_table[DEVICE_ATTR_HASH_SIZE];

/* device attribute show buffer size */
#define BUFSIZE 4096

#define ID2INDEX(id)  (id - 1)
#define INDEX2ID(idx) (idx + 1)

static struct device devices[NR_DEVICES];

static int add_class_symlinks(struct device* dev);
static int bus_add_device(struct device* dev);
static int create_sys_dev_entry(struct device* dev);
static int add_device_node(struct device* dev);

static ssize_t device_dev_show(sysfs_dyn_attr_t* attr, char* buf, size_t size)
{
    struct device* dev = (struct device*)attr->cb_data;
    return snprintf(buf, size, "%lu:%lu\n", MAJOR(dev->devt), MINOR(dev->devt));
}

static ssize_t uevent_show(sysfs_dyn_attr_t* attr, char* buf, size_t size)
{
    struct device* dev = (struct device*)attr->cb_data;
    return device_show_uevent(dev, buf);
}

static ssize_t uevent_store(struct sysfs_dyn_attr* attr, const char* buf,
                            size_t count)
{
    struct device* dev = (struct device*)attr->cb_data;
    int retval;

    retval = device_synth_uevent(dev, buf, count);
    if (retval != OK) return retval;

    return count;
}

void init_device()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        devices[i].id = NO_DEVICE_ID;
    }

    for (i = 0; i < DEVICE_ATTR_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&device_attr_table[i]);
    }
}

static struct device* alloc_device()
{
    int i;
    struct device* dev = NULL;

    for (i = 0; i < NR_DEVICES; i++) {
        if (devices[i].id == NO_DEVICE_ID) {
            dev = &devices[i];
            break;
        }
    }

    if (!dev) return NULL;

    memset(dev, 0, sizeof(*dev));
    dev->id = INDEX2ID(i);

    return dev;
}

static void device_domain_label(struct device* dev, char* buf)
{
    char name[DEVICE_NAME_MAX];

    snprintf(name, DEVICE_NAME_MAX, "%s", dev->name);
    dev = dev->parent;

    int len = strlen(name) + 1;
    while (dev) {
        size_t name_len = strlen(dev->name);
        memmove(name + name_len + 1, name, len);
        len += name_len + 1;
        strcpy(name, dev->name);
        name[name_len] = '.';
        dev = dev->parent;
    }

    snprintf(buf, PATH_MAX, "devices.%s", name);
}

static int publish_device(struct device* dev)
{
    /* TODO: proper cleanup on error */
    char label[PATH_MAX];
    char device_root[PATH_MAX - DEVICE_NAME_MAX - 1];
    sysfs_dyn_attr_t dev_uevent_attr;
    int retval;

    device_domain_label(dev, device_root);

    retval = sysfs_publish_domain(device_root, SF_PRIV_OVERWRITE);
    if (retval) {
        return retval;
    }

    snprintf(label, PATH_MAX, "%s.uevent", device_root);
    if ((retval =
             sysfs_init_dyn_attr(&dev_uevent_attr, label, SF_PRIV_OVERWRITE,
                                 dev, uevent_show, uevent_store)) != OK)
        return retval;
    if ((retval = sysfs_publish_dyn_attr(&dev_uevent_attr)) < 0) return -retval;

    retval = add_class_symlinks(dev);
    if (retval) return retval;

    retval = bus_add_device(dev);
    if (retval) return retval;

    if (dev->devt != NO_DEV) {
        /* add dev attribute */
        snprintf(label, PATH_MAX, "%s.dev", device_root);

        sysfs_dyn_attr_t dev_attr;
        retval = sysfs_init_dyn_attr(&dev_attr, label, SF_PRIV_OVERWRITE,
                                     (void*)dev, device_dev_show, NULL);
        if (retval) return retval;
        retval = sysfs_publish_dyn_attr(&dev_attr);
        if (retval < 0) return -retval;

        retval = create_sys_dev_entry(dev);
        if (retval) return retval;

        /* add /dev node */
        retval = add_device_node(dev);
        if (retval) return retval;
    }

    device_uevent(dev, KOBJ_ADD);

    return 0;
}

static int add_class_symlinks(struct device* dev)
{
    char class_root[PATH_MAX - CLASS_NAME_MAX - 1];
    char device_root[PATH_MAX - DEVICE_NAME_MAX - 1];
    char label[PATH_MAX];
    int retval;

    device_domain_label(dev, device_root);

    if (!dev->class) return 0;

    class_domain_label(dev->class, class_root);

    /* class -> device */
    snprintf(label, PATH_MAX, "%s.%s", class_root, dev->name);
    retval = sysfs_publish_link(device_root, label);
    if (retval) return retval;

    /* device -> class */
    snprintf(label, PATH_MAX, "%s.subsystem", device_root);
    retval = sysfs_publish_link(class_root, label);
    if (retval) return retval;

    /* device -> parent */
    if (dev->parent) {
        snprintf(label, PATH_MAX, "%s.device", device_root);
        device_domain_label(dev->parent, device_root);
        retval = sysfs_publish_link(device_root, label);
        if (retval) return retval;
    }

    return 0;
}

static int bus_add_device(struct device* dev)
{
    char bus_root[PATH_MAX - BUS_NAME_MAX - 1];
    char device_root[PATH_MAX - DEVICE_NAME_MAX - 1];
    char label[PATH_MAX];
    int retval;

    device_domain_label(dev, device_root);

    if (dev->bus) {
        bus_domain_label(dev->bus, bus_root);

        /* bus -> device */
        snprintf(label, PATH_MAX, "%s.devices.%s", bus_root, dev->name);
        retval = sysfs_publish_link(device_root, label);
        if (retval) return retval;

        /* device -> bus */
        snprintf(label, PATH_MAX, "%s.subsystem", device_root);
        retval = sysfs_publish_link(bus_root, label);
        if (retval) return retval;
    }

    return 0;
}

static int create_sys_dev_entry(struct device* dev)
{
    char device_root[PATH_MAX - DEVICE_NAME_MAX - 1];
    char label[PATH_MAX];
    int retval;

    device_domain_label(dev, device_root);

    snprintf(label, PATH_MAX, "dev.%s.%lu:%lu",
             (dev->type == DT_BLOCKDEV ? "block" : "char"), MAJOR(dev->devt),
             MINOR(dev->devt));
    retval = sysfs_publish_link(device_root, label);
    if (retval) return retval;

    return 0;
}

static int add_device_node(struct device* dev)
{
    struct memfs_inode* dir_pin;
    struct memfs_stat stat;
    struct memfs_inode* pin;
    char component[NAME_MAX];
    size_t name_len;
    const char *name, *p;

    dir_pin = memfs_get_root_inode();

    name = strlen(dev->path) ? dev->path : dev->name;

    while (*name != '\0') {
        p = name;
        while (*p != '\0' && *p != '/')
            p++;

        name_len = p - name;
        if (!name_len) {
            name++;
            continue;
        }

        memcpy(component, name, name_len);
        component[name_len] = '\0';

        if (*p == '\0') break;

        pin = memfs_find_inode_by_name(dir_pin, component);

        if (!pin) {
            memset(&stat, 0, sizeof(stat));
            stat.st_mode =
                S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
            stat.st_uid = SU_UID;
            stat.st_gid = 0;
            pin = memfs_add_inode(dir_pin, component, NO_INDEX, &stat, NULL);
            if (!pin) return ENOMEM;
        }

        dir_pin = pin;
        name = p + 1;
    }

    if (!strlen(component)) return 0;

    memset(&stat, 0, sizeof(stat));
    stat.st_mode = (dev->type == DT_BLOCKDEV ? S_IFBLK : S_IFCHR) | 0666;
    stat.st_uid = SU_UID;
    stat.st_gid = 0;
    stat.st_device = dev->devt;
    pin = memfs_add_inode(dir_pin, component, NO_INDEX, &stat, NULL);

    if (!pin) {
        return ENOMEM;
    }

    return 0;
}

device_id_t do_device_register(MESSAGE* m)
{
    struct device_info devinf;
    int retval;

    if (m->BUF_LEN != sizeof(devinf)) return EINVAL;

    data_copy(SELF, &devinf, m->source, m->BUF, m->BUF_LEN);

    struct device* dev = alloc_device();
    if (!dev) return ENOMEM;

    strlcpy(dev->name, devinf.name, sizeof(dev->name));
    strlcpy(dev->path, devinf.path, sizeof(dev->path));
    dev->owner = m->source;
    dev->bus = get_bus_type(devinf.bus);
    dev->class = get_class(devinf.class);
    dev->parent = get_device(devinf.parent);
    dev->devt = devinf.devt;
    dev->type = devinf.type;

    retval = publish_device(dev);
    if (retval) return retval;

    device_uevent(dev, KOBJ_ADD);

    m->u.m_devman_register_reply.id = dev->id;
    return 0;
}

struct device* get_device(device_id_t id)
{
    if (id == NO_DEVICE_ID) return NULL;

    return &devices[ID2INDEX(id)];
}

static struct device_attr_cb_data* alloc_device_attr()
{
    static dev_attr_id_t next_id = 1;
    struct device_attr_cb_data* attr =
        (struct device_attr_cb_data*)malloc(sizeof(struct device_attr_cb_data));

    if (!attr) return NULL;

    attr->id = next_id++;
    INIT_LIST_HEAD(&attr->list);

    return attr;
}

static void device_attr_addhash(struct device_attr_cb_data* attr)
{
    /* Add a device attribute to hash table */
    unsigned int hash = attr->id & DEVICE_ATTR_HASH_MASK;
    list_add(&attr->list, &device_attr_table[hash]);
}

/*
static void device_attr_unhash(struct device_attr_cb_data* attr)
{
    \/\* Remove a dynamic attribute from hash table \*\/
    list_del(&attr->list);
}
*/

static ssize_t device_attr_show(sysfs_dyn_attr_t* sf_attr, char* buf,
                                size_t size)
{
    struct device_attr_cb_data* attr =
        (struct device_attr_cb_data*)sf_attr->cb_data;
    MESSAGE msg;
    int retval;

    msg.type = DM_DEVICE_ATTR_SHOW;
    msg.u.m_devman_attr_req.buf = buf;
    msg.u.m_devman_attr_req.len = size;
    msg.u.m_devman_attr_req.target = attr->id;

    /* TODO: async read/write */
    retval = send_recv(BOTH, attr->owner, &msg);
    if (retval) return -retval;

    sysfs_complete_dyn_attr(msg.u.m_devman_attr_reply.status,
                            msg.u.m_devman_attr_reply.count);

    return SUSPEND;
}

static ssize_t device_attr_store(sysfs_dyn_attr_t* sf_attr, const char* buf,
                                 size_t count)
{
    struct device_attr_cb_data* attr =
        (struct device_attr_cb_data*)(sf_attr->cb_data);
    MESSAGE msg;
    int retval;

    msg.type = DM_DEVICE_ATTR_STORE;
    msg.u.m_devman_attr_req.buf = (char*)buf;
    msg.u.m_devman_attr_req.len = count;
    msg.u.m_devman_attr_req.target = attr->id;

    /* TODO: async read/write */
    retval = send_recv(BOTH, attr->owner, &msg);
    if (retval) return -retval;

    sysfs_complete_dyn_attr(msg.u.m_devman_attr_reply.status,
                            msg.u.m_devman_attr_reply.count);

    return SUSPEND;
}

static int get_device_path_length(struct device* dev)
{
    int len = 1;

    do {
        len += strlen(dev->name) + 1;
        dev = dev->parent;
    } while (dev);

    return len;
}

static void fill_device_path(struct device* dev, char* path, int len)
{
    --len;
    for (; dev; dev = dev->parent) {
        int cur = strlen(dev->name);
        len -= cur;
        memcpy(path + len, dev->name, cur);
        *(path + --len) = '/';
    }
}

char* device_get_path(struct device* dev)
{
    char* path;
    const char* prefix = "/devices";
    int prefix_len, len;

    if (!dev) return NULL;

    len = get_device_path_length(dev);
    if (!len) return NULL;
    prefix_len = strlen(prefix);

    path = malloc(prefix_len + len);
    if (!path) return NULL;

    path[prefix_len + len - 1] = '\0';

    strcpy(path, prefix);
    fill_device_path(dev, &path[prefix_len], len);

    return path;
}

int do_device_attr_add(MESSAGE* m)
{
    struct device_attr_info info;
    struct device_attr_cb_data* attr;
    char label[PATH_MAX];
    char *p, *name, *out, *outlim;
    size_t name_len;
    struct device* dev;
    sysfs_dyn_attr_t sysfs_attr;
    int retval;

    if (m->BUF_LEN != sizeof(info)) return EINVAL;

    data_copy(SELF, &info, m->source, m->BUF, m->BUF_LEN);

    dev = get_device(info.device);
    name = info.name;
    out = label;
    outlim = label + sizeof(label);

    device_domain_label(dev, out);
    out += strlen(out);

    if (out + strlen(info.name) + 1 >= outlim) return ENAMETOOLONG;

    while (*name != '\0') {
        p = name;
        while (*p != '\0' && *p != '/')
            p++;

        name_len = p - name;
        if (!name_len) {
            name++;
            continue;
        }

        *out++ = '.';
        memcpy(out, name, name_len);
        out += name_len;
        *out = '\0';

        if (*p == '\0') break;

        retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);
        if (retval != OK && retval != EEXIST) return retval;

        name = p + 1;
    }

    attr = alloc_device_attr();
    attr->device = dev;
    attr->owner = m->source;

    retval =
        sysfs_init_dyn_attr(&sysfs_attr, label, SF_PRIV_OVERWRITE, (void*)attr,
                            device_attr_show, device_attr_store);
    if (retval) return retval;
    retval = sysfs_publish_dyn_attr(&sysfs_attr);
    if (retval < 0) return -retval;

    attr->sysfs_id = retval;
    device_attr_addhash(attr);

    m->ATTRID = attr->id;
    return 0;
}
