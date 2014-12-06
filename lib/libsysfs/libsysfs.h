#ifndef _LIBSYSFS_H_
#define _LIBSYSFS_H_

#define SF_ENTRY_LABEL_MAX	32

/* entry types */
#define ET_U32		1
#define ET_DOMAIN	2
#define ET_STRING	3
#define ET_DEVNO	4
#define ET_DYNAMIC	5
#define ET_LINK		6

/* privilege */
#define SF_PRIV_RETRIEVE	1

PUBLIC int sysfs_publish_domain(char * key, int flags);
PUBLIC int sysfs_publish_u32(char * key, u32 value, int flags);

#endif /* _LIBSYSFS_H_ */
