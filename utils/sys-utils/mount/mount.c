#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>
#include <stdio.h>
#include <mntent.h>

static int list() { return 0; }

static int mount_all()
{
    struct mntent* m;
    FILE* f = NULL;
    f = setmntent(_PATH_MNTTAB, "r");

    if (!f) exit(EXIT_FAILURE);

    while ((m = getmntent(f))) {
        int mount_flags = 0;

        if (strcmp(m->mnt_fsname, "none") == 0) m->mnt_fsname = NULL;

        if (mount(m->mnt_fsname, m->mnt_dir, m->mnt_type, mount_flags, NULL) !=
            0) {
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

    if (argc == 1) return list();

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            opt = argv[i] + 1;
            while (*opt) {
                switch (*opt++) {
                case 'a':
                    a_flag = 1;
                    break;
                default:
                    usage();
                    break;
                }
            }
        }
    }

    if (a_flag) return mount_all();

    return EXIT_SUCCESS;
}
