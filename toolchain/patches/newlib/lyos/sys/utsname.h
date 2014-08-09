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

#ifndef _UNAME_H_
#define _UNAME_H_

/* uname */
#define _UTSNAME_SYSNAME_LENGTH 65
#define _UTSNAME_NODENAME_LENGTH 65
#define _UTSNAME_RELEASE_LENGTH 65
#define _UTSNAME_VERSION_LENGTH 65
#define _UTSNAME_MACHINE_LENGTH 65

#define SIZE_UTSNAME	(_UTSNAME_SYSNAME_LENGTH + \
                        _UTSNAME_NODENAME_LENGTH + \
                        _UTSNAME_RELEASE_LENGTH + \
                        _UTSNAME_VERSION_LENGTH + \
                        _UTSNAME_MACHINE_LENGTH)

struct utsname {
	char sysname[_UTSNAME_SYSNAME_LENGTH];
	char nodename[_UTSNAME_NODENAME_LENGTH];
	char release[_UTSNAME_RELEASE_LENGTH];
	char version[_UTSNAME_VERSION_LENGTH];
	char machine[_UTSNAME_MACHINE_LENGTH];
};

int uname(struct utsname * name);

#endif
