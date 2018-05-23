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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include <lyos/config.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int env_argc = 0;
char ** env_argv = NULL;

#define ENV_BUF_SIZE	32

PUBLIC void panic(const char *fmt, ...);

PUBLIC void env_setargs(int argc, char * argv[])
{
	env_argc = argc;
	env_argv = argv;
} 

PRIVATE char * get_value(const char * param, const char * key)
{
	char * envp = (char *)param;
	const char * name = key;

	for (; *envp != 0;) {
		for (name = key; *name != 0 && *name == *envp; name++, envp++);
		if (*name == '\0' && *envp == '=') return envp + 1;
		while (*envp++ != 0);
	}

	return NULL;
}

PUBLIC int env_get_param(const char * key, char * value, int max_len)
{
	static char kernel_cmdline[KINFO_CMDLINE_LEN];

	if (key == NULL) return EINVAL;

	int key_len = strlen(key);
	int i;
	for (i = 0; i < env_argc; i++) {
		if (strncmp(env_argv[i], key, key_len) != 0) continue;
		if (strlen(env_argv[i]) < key_len) continue;
		if (env_argv[i][key_len] != '=') continue;

		char * key_value = env_argv[i] + key_len + 1;
		if (strlen(key_value) + 1 > max_len) return E2BIG;
		strcpy(value, key_value);

		return 0;
	}

	MESSAGE m;
    m.REQUEST = GETINFO_CMDLINE;
    m.BUF = kernel_cmdline;
    m.BUF_LEN = sizeof(kernel_cmdline);

    int retval = syscall_entry(NR_GETINFO, &m);
    if (retval) return retval;

    char * key_value = get_value(kernel_cmdline, key);
    if (key_value == NULL) return ESRCH;

    if (strlen(key_value) > max_len) return E2BIG;
    strcpy(value, key_value);

	return 0;
}

PUBLIC void env_panic(char * key)
{
	static char value[ENV_BUF_SIZE] = "<unknown>";
  	env_get_param(key, value, sizeof(value));
	panic("bad environment setting: %s = %s\n", key, value);
}

PUBLIC int env_get_long(char * key, long * value, const char * fmt, int field, long min, long max)
{
	char val_buf[ENV_BUF_SIZE];
	int retval = 0;
	char punct[] = ":;,.";
	long param;
	unsigned long uparam;

	if ((retval = env_get_param(key, val_buf, ENV_BUF_SIZE)) != 0) return retval;
	char * pv = val_buf, * end;
	int i = 0;

	while (1) {
		while (*pv == ' ') pv++;
		if (*pv == 0) return retval;
		if (*fmt == 0) break;

		if (strchr(punct, *pv)) {
			if (strchr(punct, *fmt) != NULL) i++;
			if (*fmt++ == *pv) pv++;
		} else {
			int radix = -1, sign = 1;
			switch (*fmt) {
				case '*': break;
				case 'd': radix = 10; break;
				case 'x': radix = 16; break;
				case 'o': radix = 8; break;
				case 'u': radix = 0; sign = 0; break;
				default: goto badenv;
			}

			if (radix < 0) while (strchr(punct, *pv) == NULL) pv++;
			else {
				if (sign)
					param = strtol(pv, &end, radix);
				else 
					uparam = strtoul(pv, &end, radix);

				if (pv == end) break;
				pv = end;
			}

			if (i == field) {
				if (sign) {
					if (min >= 0 && param < min) break;
					if (max >= 0 && param > max) break;
					*value = param;
				} else {
					*(unsigned long *)value = uparam;
				}
			}
		}
	}

badenv:
	env_panic(key);
	return EINVAL;
}
