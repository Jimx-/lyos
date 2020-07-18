#ifndef _MNTENT_H_
#define _MNTENT_H_

#define _PATH_MNTTAB  "/etc/fstab"
#define _PATH_MOUNTED "/etc/mtab"

struct mntent {
    char* mnt_fsname; /* name of mounted file system */
    char* mnt_dir;    /* file system path prefix */
    char* mnt_type;   /* mount type (see mntent.h) */
    char* mnt_opts;   /* mount options (see mntent.h) */
    int mnt_freq;     /* dump frequency in days */
    int mnt_passno;   /* pass number on parallel fsck */
};

FILE* setmntent(const char* filename, const char* type);
struct mntent* getmntent(FILE* stream);
int addmntent(FILE* stream, const struct mntent* mnt);
int endmntent(FILE* streamp);
char* hasmntopt(const struct mntent* mnt, const char* opt);

#endif
