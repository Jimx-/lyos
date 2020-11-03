/* realpath.c - Return the canonicalized absolute pathname */

/* Written 2000 by Werner Almesberger */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

/* FIXME: buffer overrun possible, loops forever on cyclic symlinks */

/*
 * Canonical name: never ends with a slash
 */

static int resolve_path(char* path, char* result, char* pos)
{
    if (*path == '/') {
        *result = '/';
        pos = result + 1;
        path++;
    }
    *pos = 0;
    if (!*path) return 0;
    while (1) {
        char* slash;
        struct stat st;

        slash = *path ? strchr(path, '/') : NULL;
        if (slash) *slash = 0;
        if (!path[0] ||
            (path[0] == '.' && (!path[1] || (path[1] == '.' && !path[2])))) {
            pos--;
            if (pos != result && path[0] && path[1])
                while (*--pos != '/')
                    ;
        } else {
            strcpy(pos, path);
            if (lstat(result, &st) < 0) return -1;
            if (S_ISLNK(st.st_mode)) {
                char buf[PATH_MAX];

                if (readlink(result, buf, sizeof(buf)) < 0) return -1;
                *pos = 0;
                if (slash) {
                    *slash = '/';
                    strcat(buf, slash);
                }
                strcpy(path, buf);
                if (*path == '/') result[1] = 0;
                pos = strchr(result, 0);
                continue;
            }
            pos = strchr(result, 0);
        }
        if (slash) {
            *pos++ = '/';
            path = slash + 1;
        }
        *pos = 0;
        if (!slash) break;
    }
    return 0;
}

char* realpath(const char* __restrict path, char* __restrict resolved_path)
{
    char cwd[PATH_MAX];
    char* path_copy;
    char* out;
    int res;

    if (!*path) {
        errno = ENOENT; /* SUSv2 */
        return NULL;
    }

    if (resolved_path)
        out = resolved_path;
    else
        out = malloc(PATH_MAX);

    if (!out) {
        errno = ENOMEM;
        return NULL;
    }

    if (!getcwd(cwd, sizeof(cwd))) goto err;

    strcpy(out, "/");

    if (resolve_path(cwd, out, out)) goto err;
    strcat(out, "/");

    path_copy = strdup(path);
    if (!path_copy) goto err;

    res = resolve_path(path_copy, out, strchr(out, 0));
    free(path_copy);

    if (res) goto err;

    return out;

err:
    if (!resolved_path) free(out);
    return NULL;
}
