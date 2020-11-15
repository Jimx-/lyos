#ifndef _LIBFSDRIVER_PROTO_H_
#define _LIBFSDRIVER_PROTO_H_

int fsdriver_register(const struct fsdriver* fsd);
int fsdriver_readsuper(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_mountpoint(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_putinode(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_readwrite(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_stat(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_ftrunc(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_chmod(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_sync(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_getdents(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_lookup(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_create(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_mkdir(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_mknod(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_rdlink(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_link(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_symlink(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_unlink(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_rmdir(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_utime(const struct fsdriver* fsd, MESSAGE* m);
int fsdriver_chown(const struct fsdriver* fsd, MESSAGE* m);

#endif
