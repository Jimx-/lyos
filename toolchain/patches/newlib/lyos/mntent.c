#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mntent.h>

#define BUFLEN 1024

FILE* setmntent(const char* filename, const char* type)
{
    return fopen(filename, type);
}

struct mntent* getmntent_r(FILE* fp, struct mntent* mntbuf, char* buf,
                           int buflen)
{
    char *line = NULL, *saveptr = NULL;
    const char* sep = " \t\n";

    if (!fp || !mntbuf || !buf) return NULL;

    while ((line = fgets(buf, buflen, fp)) != NULL) {
        if (buf[0] == '#' || buf[0] == '\n') continue;
        break;
    }

    if (!line) return NULL;

    mntbuf->mnt_fsname = strtok_r(buf, sep, &saveptr);
    if (!mntbuf->mnt_fsname) return NULL;

    mntbuf->mnt_dir = strtok_r(NULL, sep, &saveptr);
    if (!mntbuf->mnt_dir) return NULL;

    mntbuf->mnt_type = strtok_r(NULL, sep, &saveptr);
    if (!mntbuf->mnt_type) return NULL;

    mntbuf->mnt_opts = strtok_r(NULL, sep, &saveptr);
    if (!mntbuf->mnt_opts) mntbuf->mnt_opts = "";

    line = strtok_r(NULL, sep, &saveptr);
    mntbuf->mnt_freq = !line ? 0 : atoi(line);

    line = strtok_r(NULL, sep, &saveptr);
    mntbuf->mnt_passno = !line ? 0 : atoi(line);

    return mntbuf;
}

struct mntent* getmntent(FILE* f)
{
    static char buf[BUFLEN];
    static struct mntent mnt;

    return getmntent_r(f, &mnt, buf, sizeof(buf));
}

int addmntent(FILE* stream, const struct mntent* mnt)
{
    if (!stream || !mnt) return 1;

    return fprintf(stream, "%s %s %s %s %d %d\n", mnt->mnt_fsname, mnt->mnt_dir,
                   mnt->mnt_type, mnt->mnt_opts, mnt->mnt_freq,
                   mnt->mnt_passno) < 0;
}

int endmntent(FILE* streamp)
{
    fclose(streamp);
    return 1;
}

char* hasmntopt(const struct mntent* mnt, const char* opt)
{
    const char* at;
    size_t len;

    if (!mnt || !mnt->mnt_opts || !opt) return NULL;

    len = strlen(opt);
    for (at = mnt->mnt_opts; *at;) {
        if (!strncmp(at, opt, len) &&
            (at[len] == '\0' || at[len] == ',' || at[len] == '='))
            return (char*)at;

        at = strchr(at, ',');
        if (!at) break;
        at++;
    }

    return NULL;
}
