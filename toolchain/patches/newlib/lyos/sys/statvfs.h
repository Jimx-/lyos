#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H

#include <sys/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define ST_RDONLY 1
#define ST_NOSUID 2

struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;

    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    fsfilcnt_t f_favail;

    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

int statvfs(const char* __restrict, struct statvfs* __restirct);
int fstatvfs(int, struct statvfs*);

__END_DECLS

#endif // _SYS_STATVFS_H
