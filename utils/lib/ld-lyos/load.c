#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ldso.h"
#include "env.h"

extern struct so_info* si_list;

static int match_name(char* name, struct so_info* si)
{
	int name_len = strlen(name);
	if (name_len != strlen(si->name)) return 0;

	char* p = name;
	char* q = si->name;
	while (p < name + name_len) {
		if (*p != *q) break;
		else {
			p++;
			q++;
		}
	}

	return (p == name + name_len);
}

struct so_info* ldso_load_object(char* pathname)
{
	struct stat sbuf;
	int fd;
	struct so_info* si = NULL;

	fd = open(pathname, O_RDONLY);
	if (fd == -1) return NULL;

	if (fstat(fd, &sbuf) == -1) {
		close(fd);
		return NULL;
	}

	for (si = si_list; si != NULL; si = si->list) {
		if (si->dev == sbuf.st_dev && si->ino == sbuf.st_ino) {
			close(fd);
			break;
		}
	}

	if (si == NULL) {
		close(fd);
	}

	return NULL;
}

static struct so_info* search_library(char* name, int name_len, char* path, int path_len)
{
	char pathname[PATH_MAX];
	if (name_len + path_len + 2 >= PATH_MAX) return NULL;

	memcpy(pathname, path, path_len);
	pathname[path_len] = '/';
	memcpy(pathname + path_len + 1, name, name_len);

	return ldso_load_object(pathname);
}

struct so_info* ldso_load_library(char* name, struct so_info* who)
{
	struct so_info* si;
	int name_len = strlen(name);

	int i;
	for (i = 0; i < ld_paths.count; i++) {
		struct search_path* path = &ld_paths.paths[i];
		if ((si = search_library(name, name_len, path->pathname, path->name_len)) != NULL) goto loaded;
	}

	for (i = 0; i < ld_default_paths.count; i++) {
		struct search_path* path = &ld_default_paths.paths[i];
		if ((si = search_library(name, name_len, path->pathname, path->name_len)) != NULL) goto loaded;
	}

loaded:
	return si;
}

static int ldso_load_by_name(char* name, struct needed_entry* needed, struct so_info* who)
{
	struct so_info* si;
	for (si = si_list; si != NULL; si = si->list) {
		if (match_name(name, si)) {
			si->refcnt++;
			needed->si = si;
			return 1;
		}
	}

	return (needed->si = ldso_load_library(name, who)) != NULL;
}

int ldso_load_needed(struct so_info* first)
{
	struct so_info* si;
	int retval = 0;

	for (si = first; si; si = si->next) {
		struct needed_entry* needed;

		for (needed = si->needed; needed; needed = needed->next) {
			char* name = si->strtab + needed->name;

			if (!ldso_load_by_name(name, needed, si)) retval = -1;
		}
	}

	return retval;
}
