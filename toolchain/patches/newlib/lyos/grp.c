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

#include <grp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

typedef int (*grp_pred_t)(const struct group* grp, const void* data);

static int extract_entry(struct group* grp, const char* line, char* buf,
                         size_t buflen)
{
    char* segments[4];
    const char *p, *next, *buflim;
    char* out;
    char** memv;
    int memcount;
    size_t len;
    int i, retval;

    memset(grp, 0, sizeof(*grp));
    memset(segments, 0, sizeof(segments));

    buflim = buf + buflen;

    i = 0;
    p = line;
    while (i < 4) {
        next = strchr(p, ':');
        if (!next) next = p + strlen(p);

        len = next - p;

        if (len >= buflen) {
            retval = ERANGE;
            goto free_segs;
        }

        if (len) {
            memcpy(buf, p, len);
            buf[len] = '\0';
            segments[i] = strdup(buf);

            if (!segments[i]) {
                retval = ENOMEM;
                goto free_segs;
            }
        }

        i++;

        if (!*next) break;
        p = next + 1;
    }

    out = buf;
    memv = (char**)out;

    if (segments[3]) {
        /* parse member list */
        memcount = 1;

        for (p = segments[3]; p < segments[3] + strlen(segments[3]); p++) {
            if (*p == ',') memcount++;
        }

        /* skip the NULL terminator */
        out = (char*)&memv[memcount + 1];
        memv[memcount] = NULL;

        i = 0;
        p = segments[3];

        while (i < memcount) {
            next = strchr(p, ',');
            if (!next) next = p + strlen(p);

            len = next - p;

            if (len >= buflim - out) {
                retval = ERANGE;
                goto free_segs;
            }

            memcpy(out, p, len);
            out[len] = '\0';
            memv[i] = out;
            out += len + 1;

            if (!segments[i]) {
                retval = ENOMEM;
                goto free_segs;
            }

            i++;
            p = next + 1;
        }

    } else {
        *memv = NULL;
        out += sizeof(*memv);
    }

    grp->gr_mem = memv;

    /* parse gid */
    grp->gr_gid = atoi(segments[2]);

    retval = ENOMEM;

    /* copy name */
    len = strlen(segments[0]);
    if (len >= buflim - out) goto free_segs;
    strcpy(out, segments[0]);
    grp->gr_name = out;
    out += len + 1;

    /* copy password */
    len = strlen(segments[1]);
    if (len >= buflim - out) goto free_segs;
    strcpy(out, segments[1]);
    grp->gr_passwd = out;
    out += len + 1;

    retval = 0;
free_segs:
    for (i = 0; i < 4; i++)
        free(segments[i]);

    return retval;
}

static struct group* search_file(grp_pred_t pred, struct group* grp, char* buf,
                                 size_t buflen, const void* data)
{
    char line[512];
    char* end;
    struct group* rgrp = NULL;
    int retval = 0;

    FILE* file = fopen("/etc/group", "r");
    if (!file) return NULL;

    while (fgets(line, sizeof(line), file)) {
        /* trim the input */
        end = line + strlen(line) - 1;
        while (end >= line && (*end == '\n' || *end == '\r'))
            *end-- = '\0';

        retval = extract_entry(grp, line, buf, buflen);
        if (retval) {
            errno = retval;
            goto out;
        }

        if (pred(grp, data)) {
            rgrp = grp;
            goto out;
        }
    }

    if (!rgrp) errno = ESRCH;

out:
    fclose(file);
    return rgrp;
}

static int gid_pred(const struct group* grp, const void* data)
{
    gid_t gid = (gid_t)(uintptr_t)data;

    return grp->gr_gid == gid;
}

static int name_pred(const struct group* grp, const void* data)
{
    return !strcmp(grp->gr_name, data);
}

int getgrgid_r(gid_t gid, struct group* grp, char* buf, size_t buflen,
               struct group** result)
{
    struct group* rgrp;

    rgrp = search_file(gid_pred, grp, buf, buflen, (const void*)(uintptr_t)gid);

    if (rgrp) {
        *result = rgrp;
        return 0;
    }

    *result = NULL;

    return errno == ESRCH ? 0 : errno;
}

int getgrnam_r(const char* name, struct group* grp, char* buf, size_t buflen,
               struct group** result)
{
    struct group* rgrp;

    rgrp = search_file(name_pred, grp, buf, buflen, name);

    if (rgrp) {
        *result = rgrp;
        return 0;
    }

    *result = NULL;

    return errno == ESRCH ? 0 : errno;
}

struct group* getgrgid(gid_t gid)
{
    static char buf[512];
    static struct group grp;

    return search_file(gid_pred, &grp, buf, sizeof(buf),
                       (const void*)(uintptr_t)gid);
}

struct group* getgrnam(const char* name)
{
    static char buf[512];
    static struct group grp;

    return search_file(name_pred, &grp, buf, sizeof(buf), name);
}

int getgroups(int size, gid_t list[])
{
    char line[512];
    static char buf[512];
    struct group grp;
    char* end;
    int count = 0;
    int retval = 0;

    FILE* file = fopen("/etc/group", "r");
    if (!file) return -1;

    while (fgets(line, sizeof(line), file)) {
        /* trim the input */
        end = line + strlen(line) - 1;
        while (end >= line && (*end == '\n' || *end == '\r'))
            *end-- = '\0';

        retval = extract_entry(&grp, line, buf, sizeof(buf));
        if (retval) {
            continue;
        }

        if (count < size)
            list[count++] = grp.gr_gid;
        else
            break;
    }

out:
    fclose(file);
    return count;
}
