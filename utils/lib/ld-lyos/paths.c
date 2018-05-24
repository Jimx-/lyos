#include <elf.h>
#include <stddef.h>
#include <string.h>

#include "ldso.h"

static void add_path(struct search_paths* list, const char* path, int len)
{
	struct search_path* ppath = &list->paths[list->count];
	memcpy((char*)ppath->pathname, path, len);
	ppath->pathname[len] = '\0';
	ppath->name_len = len;
	list->count++;
}

void ldso_init_paths(struct search_paths* list)
{
	list->count = 0;
}

static char* _strchr(const char* str, char character)
{
	char* p = (char*)str;
	while (*p != '\0' && *p != character) {
		p++;
	}

	if (*p == '\0') return NULL;
	return p;
}

void ldso_add_paths(struct search_paths* list, const char* paths)
{
	const char* start = paths;
	char* end;
	int len;

	while (1) {
		end = _strchr(start, ':');

		if (end) len = end - start;
		else len = strlen(start);

		add_path(list, start, len);

		if (!end) break;
		start = end + 1;
	}
}
