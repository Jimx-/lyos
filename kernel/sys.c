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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "errno.h"
#include "sys/utsname.h"

PUBLIC	int sys_reboot(int _unused1, int _unused2, int flags, struct proc* p)
{
	hard_reboot();
	return 0;
}

PUBLIC int do_ftime()
{
	return -ENOSYS;
}

PUBLIC int do_mknod()
{
	return -ENOSYS;
}

PUBLIC int do_break()
{
	return -ENOSYS;
}

PUBLIC int do_ustat()
{
	return -ENOSYS;
}

PUBLIC int do_ptrace()
{
	return -ENOSYS;
}

PUBLIC int do_stty()
{
	return -ENOSYS;
}

PUBLIC int do_gtty()
{
	return -ENOSYS;
}

PUBLIC int do_rename()
{
	return -ENOSYS;
}

PUBLIC int do_prof()
{
	return -ENOSYS;
}

PUBLIC int do_acct()
{
	return -ENOSYS;
}

PUBLIC int do_phys()
{
	return -ENOSYS;
}

PUBLIC int do_lock()
{
	return -ENOSYS;
}

PUBLIC int do_mpx()
{
	return -ENOSYS;
}

PUBLIC int do_ulimit()
{
	return -ENOSYS;
}

PUBLIC int do_stime()
{
	return -ENOSYS;
}

PUBLIC int do_times()
{
	return -ENOSYS;
}

PUBLIC int do_brk()
{
	return -ENOSYS;
}

PUBLIC int do_setpgid()
{
	return -ENOSYS;
}

PUBLIC int do_getpgrp()
{
	return -ENOSYS;
}

PUBLIC int do_setsid()
{
	return -ENOSYS;
}

PUBLIC int do_uname(int src, struct utsname * name)
{
	phys_copy(va2la(src, name), &thisname, SIZE_UTSNAME);
	return 0;
}

PUBLIC int do_umask()
{
	return -ENOSYS;
}
