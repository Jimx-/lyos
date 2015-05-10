#ifndef _LIBSYSFS_H_
#define _LIBSYSFS_H_

/* privilege */
#define SF_PRIV_RETRIEVE	0x1
#define SF_PRIV_OVERWRITE	0x2
#define SF_PRIV_DELETE		0x4
#define SF_TYPE_DOMAIN		0x10
#define SF_TYPE_U32			0x20
#define SF_TYPE_DEVNO		0x40
#define SF_TYPE_DYNAMIC		0x80
#define SF_TYPE_LINK		0x100

#define SF_PRIV_MASK		0xF
#define SF_TYPE_MASK		0xFF0

#define SYSFS_SERVICE_DOMAIN_LABEL "services.%s"
#define SYSFS_SERVICE_ENDPOINT_LABEL SYSFS_SERVICE_DOMAIN_LABEL ".endpoint"

PUBLIC int sysfs_publish_domain(char * key, int flags);
PUBLIC int sysfs_publish_u32(char * key, u32 value, int flags);

PUBLIC int sysfs_retrieve_u32(char * key, u32 * value);

#endif /* _LIBSYSFS_H_ */
