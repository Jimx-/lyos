#include <lyos/types.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <libsysfs.h>
#include <lyos/mgrant.h>

static int sysfs_request(MESSAGE* msg, int type, const char* key)
{
    mgrant_id_t grant;
    size_t key_len;
    int access;

    switch (type) {
    case SYSFS_GET_EVENT:
        key_len = PATH_MAX;
        access = MGF_WRITE;
        break;
    default:
        key_len = strlen(key) + 1;
        access = MGF_READ;
        break;
    }

    grant = mgrant_set_direct(TASK_SYSFS, (vir_bytes)key, key_len, access);
    if (grant == GRANT_INVALID) return ENOMEM;

    msg->type = type;

    msg->u.m_sysfs_req.key_grant = grant;
    msg->u.m_sysfs_req.key_len = key_len;

    send_recv(BOTH, TASK_SYSFS, msg);

    mgrant_revoke(grant);

    return msg->u.m_sysfs_req.status;
}

int sysfs_publish_domain(char* key, int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = SYSFS_PUBLISH;
    msg.u.m_sysfs_req.flags = flags | SF_TYPE_DOMAIN;

    return sysfs_request(&msg, SYSFS_PUBLISH, key);
}

int sysfs_publish_u32(char* key, u32 value, int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = SYSFS_PUBLISH;
    msg.u.m_sysfs_req.flags = flags | SF_TYPE_U32;
    msg.u.m_sysfs_req.val.u32 = value;

    return sysfs_request(&msg, SYSFS_PUBLISH, key);
}

int sysfs_publish_link(char* target, char* linkpath)
{
    MESSAGE msg;
    mgrant_id_t key_grant;
    mgrant_id_t target_grant;
    size_t key_len;
    size_t target_len;

    key_len = strlen(linkpath) + 1;
    target_len = strlen(target) + 1;

    key_grant =
        mgrant_set_direct(TASK_SYSFS, (vir_bytes)linkpath, key_len, MGF_READ);
    if (key_grant == GRANT_INVALID) return ENOMEM;

    target_grant =
        mgrant_set_direct(TASK_SYSFS, (vir_bytes)target, target_len, MGF_READ);
    if (target_grant == GRANT_INVALID) {
        mgrant_revoke(key_grant);
        return ENOMEM;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = SYSFS_PUBLISH_LINK;
    msg.u.m_sysfs_req.target_grant = target_grant;
    msg.u.m_sysfs_req.target_len = target_len;
    msg.u.m_sysfs_req.key_grant = key_grant;
    msg.u.m_sysfs_req.key_len = key_len;

    send_recv(BOTH, TASK_SYSFS, &msg);

    mgrant_revoke(target_grant);
    mgrant_revoke(key_grant);

    return msg.u.m_sysfs_req.status;
}

int sysfs_retrieve_u32(char* key, u32* value)
{
    MESSAGE msg;
    int retval;

    memset(&msg, 0, sizeof(msg));

    msg.u.m_sysfs_req.flags = SF_TYPE_U32;
    retval = sysfs_request(&msg, SYSFS_RETRIEVE, key);
    if (retval) return retval;

    if (value) *value = msg.u.m_sysfs_req.val.u32;

    return 0;
}

int sysfs_subscribe(char* regexp, int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.u.m_sysfs_req.flags = flags;

    return sysfs_request(&msg, SYSFS_SUBSCRIBE, regexp);
}

int sysfs_get_event(char* key, int* typep, int* eventp)
{
    MESSAGE msg;
    int retval;

    memset(&msg, 0, sizeof(msg));
    retval = sysfs_request(&msg, SYSFS_GET_EVENT, key);
    if (retval) return retval;

    if (typep) *typep = msg.u.m_sysfs_req.flags;
    if (eventp) *eventp = msg.u.m_sysfs_req.event;

    return 0;
}
