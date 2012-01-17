/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General License for more details.

    You should have received a copy of the GNU General License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */
   
#ifndef _UNISTD_H_
#define _UNISTD_H_

int	open		(const char *pathname, int flags);
int	close		(int fd);
int	read		(int fd, void *buf, int count);
int	write		(int fd, const void *buf, int count);
int	lseek		(int fd, int offset, int whence);
int	unlink		(const char *pathname);
int	getpid		(void);
int 	getuid		(void);
int 	setuid		(uid_t uid);
int 	getgid		(void);
int 	setgid		(gid_t gid);
int 	geteuid		(void);
int 	getegid		(void);
int	fork		(void);
int	exec		(const char * path);
int	execl		(const char * path, const char *arg, ...);
int	execv		(const char * path, char * argv[]);
int	syslog		(const char *fmt, ...);
int 	mount		(int dev, char * dir);
int	umount		(void);

#endif
