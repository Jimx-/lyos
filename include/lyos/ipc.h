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

#ifndef _IPC_H_
#define _IPC_H_

#include <sys/types.h>

#define SUSPEND -1000

#define VERIFY_MESS_SIZE(msg_type) typedef int _VERIFY_##msg_type[sizeof(struct msg_type) == 40 ? 1 : -1]
/**
 * MESSAGE mechanism is borrowed from MINIX
 */
struct mess1 {  /* 16 bytes */
    int m1i1;
    int m1i2;
    int m1i3;
    int m1i4;

	u8	_pad[24];
};
VERIFY_MESS_SIZE(mess1);

struct mess2 {  /* 16 bytes */
    void* m2p1;
    void* m2p2;
    void* m2p3;
    void* m2p4;

	u8 _pad[24];
};
VERIFY_MESS_SIZE(mess2);

struct mess3 {  /* 40 bytes */
    int     m3i1;
    int     m3i2;
    int     m3i3;
    int     m3i4;
    u64     m3l1;
    u64     m3l2;
    void*   m3p1;
    void*   m3p2;
};
VERIFY_MESS_SIZE(mess3);

struct mess4 {  /* 36 bytes */
    u64 m4l1;
    int m4i1, m4i2, m4i3; 
    void *m4p1, *m4p2, *m4p3, *m4p4;

	u32 _pad;
};
VERIFY_MESS_SIZE(mess4);

struct mess5 {  /* 40 bytes */
    int     m5i1;
    int     m5i2;
    int     m5i3;
    int     m5i4;
    int     m5i5;
    int     m5i6;
    int m5i7;
    int m5i8;
    int m5i9;
    int m5i10;
};
VERIFY_MESS_SIZE(mess5);

struct mess_mm_remap {
	endpoint_t 	src;
	endpoint_t 	dest;
	void*       src_addr;
	void*		dest_addr;
	size_t 		size;
	void*		ret_addr;

	u8 			_pad[16];
};
VERIFY_MESS_SIZE(mess_mm_remap);

struct mess_vfs_select {
    u32     nfds;
    void*   readfds;
    void*   writefds;
    void*   exceptfds;
    void*   timeout;

    u8      _pad[20];
};
VERIFY_MESS_SIZE(mess_vfs_select);

struct mess_vfs_cdev_openclose {
    u64     minor;
    u32     id;

    u8      _pad[28];
};
VERIFY_MESS_SIZE(mess_vfs_cdev_openclose);

struct mess_vfs_cdev_readwrite {
    u64         minor;
    endpoint_t  endpoint;
    u32         id;
    void*       buf;
    u32         request;
    off_t       pos;
    size_t      count;

    u8          _pad[8];
};
VERIFY_MESS_SIZE(mess_vfs_cdev_readwrite);

struct mess_vfs_cdev_mmap {
    u64         minor;
    void*       addr;
    endpoint_t  endpoint;
    off_t       pos;
    size_t      count;

    u8          _pad[16];
};
VERIFY_MESS_SIZE(mess_vfs_cdev_mmap);

struct mess_vfs_cdev_select {
    u64     minor;
    u32     ops;

    u8      _pad[28];
};
VERIFY_MESS_SIZE(mess_vfs_cdev_select);

struct mess_vfs_cdev_reply {
    s32     status;
    u32     id;

    u8      _pad[32];
};
VERIFY_MESS_SIZE(mess_vfs_cdev_reply);

struct mess_vfs_cdev_mmap_reply {
    u32         status;
    endpoint_t  endpoint;
    void*       retaddr;

    u8          _pad[28];
};
VERIFY_MESS_SIZE(mess_vfs_cdev_mmap_reply);

typedef struct {
    int source;
    int type;
    union {
        struct mess1 m1;
        struct mess2 m2;
        struct mess3 m3;
        struct mess4 m4;
        struct mess5 m5;
		struct mess_mm_remap m_mm_remap;
        struct mess_vfs_select m_vfs_select;
        struct mess_vfs_cdev_openclose m_vfs_cdev_openclose;
        struct mess_vfs_cdev_readwrite m_vfs_cdev_readwrite;
        struct mess_vfs_cdev_mmap m_vfs_cdev_mmap;
        struct mess_vfs_cdev_select m_vfs_cdev_select;
        struct mess_vfs_cdev_reply m_vfs_cdev_reply;
        struct mess_vfs_cdev_mmap_reply m_vfs_cdev_mmap_reply;

		u8 _pad[40];
    } u;
} MESSAGE;
typedef int _VERIFY_MESSAGE[sizeof(MESSAGE) == 48 ? 1 : -1];

#define ASMF_USED   0x1
#define ASMF_DONE   0x2
typedef struct {
    endpoint_t dest;
    MESSAGE msg;
    int flags;
    int result;
} async_message_t;

int send_recv(int function, int src_dest, MESSAGE* msg);
int send_async(async_message_t* table, size_t len);

int asyncsend3(endpoint_t dest, MESSAGE* msg, int flags);
int async_sendrec(endpoint_t dest, MESSAGE* msg, int flags);

#endif
