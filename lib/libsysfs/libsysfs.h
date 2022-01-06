#ifndef _LIBSYSFS_H_
#define _LIBSYSFS_H_

#include <sys/types.h>
#include <sys/syslimits.h>
#include <lyos/list.h>

/* dynamic attribute */
typedef int sysfs_dyn_attr_id_t;
struct sysfs_dyn_attr;
typedef ssize_t (*sysfs_dyn_attr_show_t)(struct sysfs_dyn_attr* attr,
                                         char* buf);
typedef ssize_t (*sysfs_dyn_attr_store_t)(struct sysfs_dyn_attr* attr,
                                          const char* buf, size_t count);

typedef struct sysfs_dyn_attr {
    char label[NAME_MAX];
    sysfs_dyn_attr_id_t attr_id;
    int flags;
    void* cb_data;
    struct list_head list;
    sysfs_dyn_attr_show_t show;
    sysfs_dyn_attr_store_t store;
} __attribute__((packed)) sysfs_dyn_attr_t;

/* Privilege */
#define SF_PRIV_RETRIEVE  0x1
#define SF_PRIV_OVERWRITE 0x2
#define SF_PRIV_DELETE    0x4

/* Node types */
#define SF_TYPE_DOMAIN  0x10
#define SF_TYPE_U32     0x20
#define SF_TYPE_DEVNO   0x40
#define SF_TYPE_DYNAMIC 0x80
#define SF_TYPE_LINK    0x100

#define SF_OVERWRITE 0x1000
#define SF_CHECK_NOW 0x2000

#define SF_PRIV_MASK 0xF
#define SF_TYPE_MASK 0xFF0

/* Events */
#define SF_EVENT_PUBLISH 0x1
#define SF_EVENT_DELETE  0x2

#define SYSFS_SERVICE_DOMAIN_LABEL   "services.%s"
#define SYSFS_SERVICE_ENDPOINT_LABEL SYSFS_SERVICE_DOMAIN_LABEL ".endpoint"

#define SYSFS_BUS_DOMAIN_LABEL   "bus.%s"
#define SYSFS_CLASS_DOMAIN_LABEL "class.%s"

int sysfs_publish_domain(char* key, int flags);
int sysfs_publish_u32(char* key, u32 value, int flags);
int sysfs_publish_link(char* target, char* linkpath);

int sysfs_retrieve_u32(char* key, u32* value);

int sysfs_subscribe(char* regexp, int flags);
int sysfs_get_event(char* key, int* typep, int* eventp);

int sysfs_init_dyn_attr(sysfs_dyn_attr_t* attr, char* label, int flags,
                        void* cb_data, sysfs_dyn_attr_show_t show,
                        sysfs_dyn_attr_store_t store);
int sysfs_publish_dyn_attr(sysfs_dyn_attr_t* attr);
ssize_t sysfs_handle_dyn_attr(MESSAGE* msg);

#endif /* _LIBSYSFS_H_ */
