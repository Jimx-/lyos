#include <stdio.h>
#include <mntent.h>

FILE *setmntent(const char *filename, const char *type)
{
	return fopen(filename, type);
}

struct mntent *getmntent(FILE *stream)
{

}

int addmntent(FILE *stream, const struct mntent *mnt)
{

}

int endmntent(FILE *streamp)
{
	fclose(streamp);
	return 1;
}

char *hasmntopt(const struct mntent *mnt, const char *opt)
{

}