#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <lyos/profile.h>

#define START 1
#define STOP 2

#define DEF_FREQ 6
#define DEF_OUTFILE "profile.out"
#define DEF_MEMSIZE 4

#define OUTPUT_MAGIC 0x4652504b /* kprf */

static const char* prog_name;
static int action = 0;
static char* outfile = "";
static FILE* foutput;
static int freq = 0;
static size_t memsize = 0;
static char* sample_buf = NULL;
static struct kprof_info kprof_info;

static int parse_args(int argc, char* argv[]);
static int start();
static int stop();

void die(int error, const char* msg)
{
    fprintf(stderr, "%s: %s (%d)", prog_name, msg, error);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    int ret = parse_args(argc, argv);
    if (ret) {
        return EXIT_FAILURE;
    }

    switch (action) {
    case START:
        if (start()) return EXIT_FAILURE;
        break;
    case STOP:
        if (stop()) return EXIT_FAILURE;
        break;
    default:
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int parse_args(int argc, char* argv[])
{
    prog_name = argv[0];

    while (--argc) {
        ++argv;

        if (strcmp(*argv, "start") == 0) {
            if (action) {
                return EINVAL;
            }
            action = START;
        } else if (strcmp(*argv, "stop") == 0) {
            if (action) {
                return EINVAL;
            }
            action = STOP;
        } else if (strcmp(*argv, "-f") == 0) {
            if (--argc == 0) return EINVAL;
            if (sscanf(*++argv, "%u", &freq) != 1) return EINVAL;
        } else if (strcmp(*argv, "-o") == 0) {
            if (--argc == 0) return EINVAL;
            outfile = *++argv;
        }
    }

    if (action == START) {
        if (!freq) freq = DEF_FREQ;
    } else if (action == STOP) {
        if (strcmp(outfile, "") == 0) outfile = DEF_OUTFILE;
        if (!memsize) memsize = DEF_MEMSIZE;
        memsize *= (1024 * 1024);
        memsize -= (memsize % sizeof(struct kprof_sample));
    }

    return 0;
}

static int start() { return kprof(KPROF_START, 0, freq, NULL, NULL); }

int alloc_mem()
{
    if ((sample_buf = (char*)malloc(memsize)) == NULL) {
        return ENOMEM;
    } else
        memset(sample_buf, 0, memsize);

    return 0;
}

int init_outfile()
{
    foutput = fopen(outfile, "wb");
    if (foutput == NULL) {
        return errno;
    }

    return 0;
}

int write_outfile()
{
    int header[4];
    header[0] = OUTPUT_MAGIC;
    header[1] = sizeof(struct kprof_info);
    header[2] = sizeof(struct kprof_proc);
    header[3] = sizeof(struct kprof_sample);

    if (fwrite(header, sizeof(int), 4, foutput) < 0) return errno;
    if (fwrite(&kprof_info, sizeof(struct kprof_info), 1, foutput) < 0)
        return errno;

    size_t size = memsize;
    if (size > kprof_info.mem_used) size = kprof_info.mem_used;

    if (fwrite(sample_buf, 1, size, foutput) < 0) return errno;
    return 0;
}

static int stop()
{
    int retval;

    if ((retval = alloc_mem()) != 0) return retval;
    if ((retval = init_outfile()) != 0) {
        die(retval, "cannot open output file");
    }

    retval = kprof(KPROF_STOP, memsize, 0, &kprof_info, sample_buf);
    if (retval) return retval;

    if ((retval = write_outfile()) != 0) {
        die(retval, "cannot write to output");
    }

    fclose(foutput);

    return 0;
}
