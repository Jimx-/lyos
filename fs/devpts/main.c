#include <lyos/types.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <lyos/service.h>
#include <lyos/fs.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>

#include <libfsdriver/libfsdriver.h>

#define ROOT_INO 1
#define BASE_INO 2

struct node {
    dev_t device;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    time_t ctime;
};

static struct node root_node = {
    .mode = S_IFDIR | 0755, .uid = 0, .gid = 0, .device = NO_DEV};

static bitchunk_t node_map[BITCHUNKS(NR_PTYS)];
static struct node node_table[NR_PTYS];

static struct node* get_node(unsigned int index)
{
    if (index >= NR_PTYS || !GET_BIT(node_map, index)) return NULL;

    return &node_table[index];
}

static struct node* get_node_by_num(ino_t num)
{
    unsigned int index;

    if (num == ROOT_INO) return &root_node;

    if (num < BASE_INO || num >= BASE_INO + NR_PTYS) return NULL;

    index = num - BASE_INO;

    return get_node(index);
}

static int set_node(unsigned int index, struct node* node)
{
    if (index >= NR_PTYS) return ENOMEM;

    SET_BIT(node_map, index);

    node_table[index] = *node;
    return 0;
}

static void clear_node(unsigned int index) { UNSET_BIT(node_map, index); }

static int devpts_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                            struct fsdriver_node* node)
{
    static int mounted = FALSE;

    assert(!(fc->sb_flags & RF_ISROOT));

    if (mounted) return EBUSY;

    /* fill result */
    node->fn_num = ROOT_INO;
    node->fn_uid = root_node.uid;
    node->fn_gid = root_node.gid;
    node->fn_size = 0;
    node->fn_mode = root_node.mode;
    node->fn_device = root_node.device;

    mounted = TRUE;

    return 0;
}

static int parse_name(const char* name, unsigned int* indexp)
{
    unsigned int index;
    char* end;

    index = strtol(name, &end, 10);
    if (*end) return FALSE;

    *indexp = index;
    return TRUE;
}

static int devpts_lookup(dev_t dev, ino_t start, const char* name,
                         struct fsdriver_node* fn, int* is_mountpoint)
{
    ino_t num;
    unsigned int index;
    struct node* node;

    if (start != ROOT_INO) return ENOENT;

    if (name[0] == '.' && name[1] == '\0') {
        num = ROOT_INO;
        node = &root_node;
    } else {
        if (!parse_name(name, &index)) return ENOENT;

        num = BASE_INO + index;

        node = get_node(index);
        if (!node) return ENOENT;
    }

    /* fill result */
    fn->fn_num = num;
    fn->fn_uid = node->uid;
    fn->fn_gid = node->gid;
    fn->fn_size = 0;
    fn->fn_mode = node->mode;
    fn->fn_device = node->device;
    *is_mountpoint = FALSE;

    return 0;
}

static int make_name(char* name, size_t size, unsigned int index)
{
    ssize_t retval;

    if ((retval = snprintf(name, size, "%u", index)) < 0) return EINVAL;

    if (retval >= size) return ENAMETOOLONG;

    return 0;
}

static int stat_type(mode_t mode)
{
    if (S_ISREG(mode))
        return DT_REG;
    else if (S_ISDIR(mode))
        return DT_DIR;
    else if (S_ISBLK(mode))
        return DT_BLK;
    else if (S_ISCHR(mode))
        return DT_CHR;
    else if (S_ISLNK(mode))
        return DT_LNK;
    else if (S_ISSOCK(mode))
        return DT_SOCK;
    else if (S_ISFIFO(mode))
        return DT_FIFO;

    return DT_UNKNOWN;
}

static ssize_t devpts_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                               loff_t* ppos, size_t count)
{
#define GETDENTS_BUFSIZE 4096
    static char getdents_buf[GETDENTS_BUFSIZE];
    struct fsdriver_dentry_list list;
    char name[NAME_MAX];
    off_t pos;
    ino_t ino;
    int type, index;
    struct node* node;
    int retval;

    if (num != ROOT_INO) return EINVAL;

    fsdriver_dentry_list_init(&list, data, count, getdents_buf,
                              sizeof(getdents_buf));

    for (;;) {
        pos = (*ppos)++;

        if (pos < 2) {
            strlcpy(name, (pos == 0) ? "." : "..", sizeof(name));
            ino = ROOT_INO;
            type = DT_DIR;
        } else {
            if (pos - 2 >= NR_PTYS) break;
            index = pos - 2;

            node = get_node(index);
            if (!node) continue;

            if (make_name(name, sizeof(name), index) != OK) continue;

            ino = BASE_INO + index;
            type = stat_type(node->mode);
        }

        retval = fsdriver_dentry_list_add(&list, ino, name, strlen(name), type);
        if (retval < 0) return retval;

        if (retval == 0) break;
    }

    return fsdriver_dentry_list_finish(&list);
}

static int devpts_stat(dev_t dev, ino_t num, struct fsdriver_data* data)
{
    struct stat sbuf;
    struct node* node;

    memset(&sbuf, 0, sizeof(sbuf));

    node = get_node_by_num(num);
    if (!node) return EINVAL;

    sbuf.st_mode = node->mode;
    sbuf.st_uid = node->uid;
    sbuf.st_gid = node->gid;
    sbuf.st_nlink = S_ISDIR(node->mode) ? 2 : 1;
    sbuf.st_rdev = node->device;
    sbuf.st_atime = node->ctime;
    sbuf.st_mtime = node->ctime;
    sbuf.st_ctime = node->ctime;

    return fsdriver_copyout(data, 0, &sbuf, sizeof(struct stat));
}

static void devpts_other(MESSAGE* msg)
{
    MESSAGE reply_msg;
    struct node node;
    int retval;

    memset(&reply_msg, 0, sizeof(reply_msg));

    switch (msg->type) {
    case DEVPTS_SET:
        memset(&node, 0, sizeof(node));
        node.device = msg->u.m_devpts_req.dev;
        node.uid = msg->u.m_devpts_req.uid;
        node.gid = msg->u.m_devpts_req.gid;
        node.mode = msg->u.m_devpts_req.mode;
        node.ctime = now();

        retval = set_node(msg->u.m_devpts_req.index, &node);
        break;

    case DEVPTS_CLEAR:
        clear_node(msg->u.m_devpts_req.index);
        retval = 0;
        break;

    default:
        retval = ENOSYS;
        break;
    }

    reply_msg.u.m_devpts_req.status = retval;

    send_recv(SEND, msg->source, &reply_msg);
}

const static struct fsdriver devpts_driver = {
    .name = "devpts",
    .root_num = ROOT_INO,

    .fs_readsuper = devpts_readsuper,
    .fs_lookup = devpts_lookup,
    .fs_stat = devpts_stat,
    .fs_getdents = devpts_getdents,
    .fs_other = devpts_other,
};

static int init_devpts(void)
{
    printl("devpts: devpts filesystem driver is running.\n");

    memset(node_map, 0, sizeof(node_map));

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(init_devpts);
    serv_init();

    return fsdriver_start(&devpts_driver);
}
