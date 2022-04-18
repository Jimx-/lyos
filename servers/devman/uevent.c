#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/config.h>
#include <lyos/const.h>
#include <string.h>
#include <stdarg.h>
#include <lyos/driver.h>
#include <lyos/sysutils.h>
#include <lyos/netlink.h>
#include <sys/stat.h>
#include <libnetlink/libnetlink.h>

#include "const.h"
#include "proto.h"

#define UEVENT_NUM_ENVP    32
#define UEVENT_BUFFER_SIZE 2048

struct uevent_env {
    char* envp[UEVENT_NUM_ENVP];
    int envp_idx;
    char buf[UEVENT_BUFFER_SIZE];
    size_t buflen;
};

static nlsockid_t uevent_sock;

static const char* kobject_actions[] = {
    [KOBJ_ADD] = "add",
    [KOBJ_REMOVE] = "remove",
    [KOBJ_CHANGE] = "change",
};

static u64 uevent_seqnum;

void uevent_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .groups = 1,
    };

    uevent_sock = netlink_create(NETLINK_KOBJECT_UEVENT, &cfg);
    if (uevent_sock < 0) panic("devman: failed to create uevent socket");
}

int add_uevent_var(struct uevent_env* env, const char* format, ...)
{
    va_list args;
    int len;

    if (env->envp_idx >= UEVENT_NUM_ENVP) return -ENOMEM;

    va_start(args, format);
    len = vsnprintf(&env->buf[env->buflen], sizeof(env->buf) - env->buflen,
                    format, args);
    va_end(args);

    if (len >= (sizeof(env->buf) - env->buflen)) return -ENOMEM;

    env->envp[env->envp_idx++] = &env->buf[env->buflen];
    env->buflen += len + 1;

    return 0;
}

static char* alloc_uevent_buf(const struct uevent_env* env,
                              const char* action_string, const char* devpath,
                              size_t* lenp)
{
    char* buf;
    size_t len;

    len = strlen(action_string) + strlen(devpath) + 2;
    buf = malloc(len + env->buflen);
    if (!buf) return NULL;

    sprintf(buf, "%s@%s", action_string, devpath);
    memcpy(&buf[len], env->buf, env->buflen);

    *lenp = len + env->buflen;
    return buf;
}

static int device_uevent_broadcast(struct device* dev,
                                   const struct uevent_env* env,
                                   const char* action_string,
                                   const char* devpath)
{
    char* buf;
    size_t len;
    int retval;

    buf = alloc_uevent_buf(env, action_string, devpath, &len);
    if (!buf) return -ENOMEM;

    retval = netlink_broadcast(uevent_sock, 0, 1, buf, len);

    free(buf);
    return retval;
}

int device_uevent_env(struct device* dev, enum kobject_action action,
                      char* envp[])
{
    const char* action_string = kobject_actions[action];
    struct device* top_dev;
    struct class* class;
    const char* subsystem;
    const char* devpath = NULL;
    struct uevent_env* env;
    int i, retval;

    if (action == KOBJ_REMOVE) dev->state_remove_uevent_sent = 1;

    top_dev = dev;
    while (!top_dev->class && top_dev->parent)
        top_dev = top_dev->parent;

    class = top_dev->class;
    if (!class) return -EINVAL;

    if (dev->uevent_suppress) return 0;

    subsystem = class->name;

    env = malloc(sizeof(*env));
    if (!env) return -ENOMEM;

    memset(env, 0, sizeof(*env));

    devpath = device_get_path(dev);
    if (!devpath) {
        retval = -ENOENT;
        goto out_free;
    }

    if ((retval = add_uevent_var(env, "ACTION=%s", action_string)) != OK)
        goto out_free;
    if ((retval = add_uevent_var(env, "DEVPATH=%s", devpath)) != OK)
        goto out_free;
    if ((retval = add_uevent_var(env, "SUBSYSTEM=%s", subsystem)) != OK)
        goto out_free;

    if (dev->devt != NO_DEV) {
        if ((retval = add_uevent_var(env, "DEVNAME=%s",
                                     (*dev->path) ? dev->path : dev->name)) !=
            OK)
            goto out_free;
        if ((retval = add_uevent_var(env, "MAJOR=%d", MAJOR(dev->devt))) != OK)
            goto out_free;
        if ((retval = add_uevent_var(env, "MINOR=%d", MINOR(dev->devt))) != OK)
            goto out_free;
    }

    if (envp) {
        for (i = 0; envp[i]; i++) {
            retval = add_uevent_var(env, "%s", envp[i]);
            if (retval) goto out_free;
        }
    }

    switch (action) {
    case KOBJ_ADD:
        dev->state_add_uevent_sent = 1;
        break;
    default:
        break;
    }

    if ((retval = add_uevent_var(env, "SEQNUM=%llu", uevent_seqnum++)) != OK)
        goto out_free;

    retval = device_uevent_broadcast(dev, env, action_string, devpath);

out_free:
    free((void*)devpath);
    free(env);
    return retval;
}

int device_uevent(struct device* dev, enum kobject_action action)
{
    return device_uevent_env(dev, action, NULL);
}

static int kobject_action_type(const char* buf, size_t count,
                               enum kobject_action* type, const char** args)
{
    enum kobject_action action;
    size_t count_first;
    const char* args_start;
    int ret = -EINVAL;

    if (count && (buf[count - 1] == '\n' || buf[count - 1] == '\0')) count--;

    if (!count) goto out;

    args_start = memchr(buf, ' ', count);
    if (args_start) {
        count_first = args_start - buf;
        args_start = args_start + 1;
    } else
        count_first = count;

    for (action = 0;
         action < sizeof(kobject_actions) / sizeof(kobject_actions[0]);
         action++) {
        if (strncmp(kobject_actions[action], buf, count_first) != 0) continue;
        if (kobject_actions[action][count_first] != '\0') continue;
        if (args) *args = args_start;
        *type = action;
        ret = 0;
        break;
    }

out:
    return ret;
}

int device_synth_uevent(struct device* dev, const char* buf, size_t count)
{
    char* no_uuid_envp[] = {"SYNTH_UUID=0", NULL};
    const char* args;
    enum kobject_action action;
    int retval;

    retval = kobject_action_type(buf, count, &action, &args);
    if (retval) goto out;

    if (!args) {
        retval = device_uevent_env(dev, action, no_uuid_envp);
        goto out;
    }

    retval = -EINVAL;

out:
    return retval;
}

ssize_t device_show_uevent(struct device* dev, char* buf)
{
    struct uevent_env* env;
    size_t count = 0;
    int i;

    env = malloc(sizeof(*env));
    if (!env) return -ENOMEM;

    memset(env, 0, sizeof(*env));

    if (dev->devt != NO_DEV) {
        add_uevent_var(env, "MAJOR=%d", MAJOR(dev->devt));
        add_uevent_var(env, "MINOR=%d", MINOR(dev->devt));
        add_uevent_var(env, "DEVNAME=%s", (*dev->path) ? dev->path : dev->name);
    }

    for (i = 0; i < env->envp_idx; i++)
        count += sprintf(&buf[count], "%s\n", env->envp[i]);

    free(env);
    return count;
}
