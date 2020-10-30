/* O_*, F_*, FD_* bit values for Linux.
   Copyright (C) 2001-2020 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _SYS_FCNTL_H_
#error "Never use <bits/fcntl-linux.h> directly; include <fcntl.h> instead."
#endif

#include <sys/cdefs.h>

#if __GNU_VISIBLE
/* File handle structure.  */
struct file_handle {
    unsigned int handle_bytes;
    int handle_type;
    /* File identifier.  */
    unsigned char f_handle[0];
};

/* Maximum handle size (for now).  */
#define MAX_HANDLE_SZ 128
#endif

__BEGIN_DECLS

#if __GNU_VISIBLE

/* Map file name to file handle.  */
extern int name_to_handle_at(int __dfd, const char* __name,
                             struct file_handle* __handle, int* __mnt_id,
                             int __flags);

/* Open file using the file handle. */
extern int open_by_handle_at(int __mountdirfd, struct file_handle* __handle,
                             int __flags);

#endif

__END_DECLS
