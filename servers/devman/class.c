#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <stdlib.h>
#include <errno.h>
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "proto.h"
#include <lyos/fs.h>
#include "lyos/proc.h"
#include "lyos/driver.h"
#include <lyos/sysutils.h>
#include <libsysfs/libsysfs.h>
#include <libdevman/libdevman.h>
#include "type.h"

#define ID2INDEX(id) (id - 1)
#define INDEX2ID(idx) (idx + 1)

#define NR_CLASSES 32
static struct class classes[NR_CLASSES];

void init_class()
{
    int i;
    for (i = 0; i < NR_CLASSES; i++) {
        classes[i].id = NO_CLASS_ID;
    }
}

static struct class* alloc_class()
{
    int i;
    struct class* class = NULL;

    for (i = 0; i < NR_CLASSES; i++) {
        if (classes[i].id == NO_CLASS_ID) {
            class = &classes[i];
            break;
        }
    }

    if (!class) return NULL;

    class->id = INDEX2ID(i);
    memset(class->name, 0, sizeof(class->name));

    return class;
}

void class_domain_label(struct class* class, char* buf)
{
    snprintf(buf, MAX_PATH, SYSFS_CLASS_DOMAIN_LABEL, class->name);
}

static int publish_class(struct class* class)
{
    char label[MAX_PATH];
    int retval;

    class_domain_label(class, label);
    retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);

    return retval;
}

class_id_t do_class_register(MESSAGE* m)
{
    char name[CLASS_NAME_MAX];
    int retval;

    if (m->NAME_LEN >= CLASS_NAME_MAX) return ENAMETOOLONG;

    data_copy(SELF, name, m->source, m->BUF, m->NAME_LEN);
    name[m->NAME_LEN] = '\0';

    struct class* class = alloc_class();
    if (!class) return ENOMEM;

    strlcpy(class->name, name, sizeof(class->name));
    class->owner = m->source;

    retval = publish_class(class);
    if (retval) return retval;

    m->u.m_devman_register_reply.id = class->id;
    return 0;
}

struct class* get_class(class_id_t id)
{
    if (id == NO_CLASS_ID) return NULL;

    int i = ID2INDEX(id);
    return &classes[i];
}
