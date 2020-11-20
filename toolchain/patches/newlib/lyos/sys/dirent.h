#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <bits/dirent.h>

__BEGIN_DECLS

int alphasort(const struct dirent**, const struct dirent**);
DIR* fdopendir(int fd);
DIR* opendir(const char* dirname);
int closedir(DIR* dir);
struct dirent* readdir(DIR* dirp);
void rewinddir(DIR* dirp);
int dirfd(DIR*);
int scandir(const char*, struct dirent***, int (*)(const struct dirent*),
            int (*)(const struct dirent**, const struct dirent**));

__END_DECLS

#endif
