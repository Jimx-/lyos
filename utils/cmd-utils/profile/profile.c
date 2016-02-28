#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <lyos/profile.h>

#define EACTION	1
#define EBADARG	2
#define EFREQ	3

#define START	1
#define STOP	2
int action = 0;

#define DEF_OUTFILE		"profile.out"
char* outfile = "";
int outfile_fd;

#define DEF_FREQ	6
int freq = 0;

#define DEF_MEMSIZE	4
int memsize = 0;

char* sample_buf = NULL;
struct kprof_info kprof_info;

int parse_args(int argc, char* argv[]);
int start();

int main(int argc, char* argv[])
{
	int ret = parse_args(argc, argv);
	if (ret) {
		return 1;
	}

	switch (action) {
	case START:
		if (start()) return 1;
		break;
	case STOP:
		if (stop()) return 1;
		break;
	default:
		return 1;
	}

	return 0;
}

int parse_args(int argc, char* argv[])
{
	while (--argc) {
		++argv;

		if (strcmp(*argv, "start") == 0) {
			if (action) {
				return EACTION;
			}
			action = START;
		} else if (strcmp(*argv, "stop") == 0) {
			if (action) {
				return EACTION;
			}
			action = STOP;
		} else if (strcmp(*argv, "-f") == 0) {
			if (--argc == 0) return EBADARG;
			if (sscanf(*++argv, "%u", &freq) != 1)
				return EFREQ;
		} else if (strcmp(*argv, "-o") == 0) {
			if (--argc == 0) return EBADARG;
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

int start()
{
	return kprof(KPROF_START, 0, freq, NULL, NULL);
}

int alloc_mem()
{
	if ((sample_buf = (char*)malloc(memsize)) == NULL) {
		return 1;
	} else memset(sample_buf, 0, memsize);

	return 0;
}

int init_outfile()
{
	if ((outfile_fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY)) == -1) return 1;
	return 0;
}

int write_outfile()
{
	char header[100];
	sprintf(header, "kprof\n%d %d %d\n", sizeof(struct kprof_info), sizeof(struct kprof_proc), sizeof(struct kprof_sample));

	if (write(outfile_fd, header, strlen(header) < 0)) return 1;

	int size = memsize;
	if (memsize > kprof_info.mem_used) memsize = kprof_info.mem_used;

	if (write(outfile_fd, sample_buf, size < 0)) return 1;
	return 0;
}

int stop()
{
	if (alloc_mem()) return 1;
	if (init_outfile()) return 1;

	int ret = kprof(KPROF_STOP, memsize, 0, &kprof_info, sample_buf);
	if (ret) return ret;

	if (write_outfile()) return 1;

	return 0;
}
