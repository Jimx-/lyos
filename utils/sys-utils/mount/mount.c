#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <stdio.h>
#include <mntent.h>

static int list() { return 0; }

static int mount_one(const char* source, const char* target, const char* type,
                     int flags, const char* opts)
{
    if (!strcmp(source, "none")) source = NULL;

    return mount(source, target, type, flags, opts);
}

static int mount_all()
{
    struct mntent* m;
    FILE* f = NULL;
    f = setmntent(_PATH_MNTTAB, "r");

    if (!f) exit(EXIT_FAILURE);

    while ((m = getmntent(f))) {
        int mount_flags = 0;

        if (mount_one(m->mnt_fsname, m->mnt_dir, m->mnt_type, mount_flags,
                      m->mnt_opts) != 0) {
            fprintf(stderr, "Can't mount on %s\n", m->mnt_dir);
            exit(EXIT_FAILURE);
        }
    }

    endmntent(f);
    return 0;
}

static void usage()
{
    fprintf(
        stderr,
        "Usage: mount [-a] [-r] [-e] [-t type] [-o options] special name\n");
    exit(1);
}

int main(int argc, char* argv[])
{
    int i, a_flag = 0;
    char* opt;
    const char *types = NULL, *options = NULL;
    const char *source, *target;

    if (argc == 1) return list();

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            opt = argv[i] + 1;
            while (*opt) {
                switch (*opt++) {
                case 'a':
                    a_flag = 1;
                    break;
                case 'o':
                    options = argv[++i];
                    break;
                case 't':
                    types = argv[++i];
                    break;
                default:
                    usage();
                    break;
                }
            }
        }
    }

    if (a_flag) return mount_all();

    if (argc < 3) usage();
    if (!types) usage();

    source = argv[argc - 2];
    target = argv[argc - 1];

    if (mount_one(source, target, types, 0, options) != 0) {
        fprintf(stderr, "Can't mount on %s\n", target);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
