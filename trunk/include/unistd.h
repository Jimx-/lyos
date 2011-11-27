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
   
#ifndef _UNISTD_H_
#define _UNISTD_H_

PUBLIC	int		open		(const char *pathname, int flags);
PUBLIC	int		close		(int fd);
PUBLIC 	int		read		(int fd, void *buf, int count);
PUBLIC 	int		write		(int fd, const void *buf, int count);
PUBLIC	int		lseek		(int fd, int offset, int whence);
PUBLIC	int		unlink		(const char *pathname);
PUBLIC 	int		getpid		(void);
PUBLIC 	int 	getuid		(void);
PUBLIC 	int 	setuid		(uid_t uid);
PUBLIC 	int 	getgid		(void);
PUBLIC 	int 	setgid		(gid_t gid);
PUBLIC 	int 	geteuid		(void);
PUBLIC 	int 	getegid		(void);
PUBLIC 	int		fork		(void);
PUBLIC 	void	exit		(int status);
PUBLIC 	int		wait		(int * status);
#define waitpid wait
PUBLIC 	int		kill		(int pid,int signo);
PUBLIC 	int		raise		(int signo);
PUBLIC 	int		exec		(const char * path);
PUBLIC 	int		execl		(const char * path, const char *arg, ...);
PUBLIC 	int		execv		(const char * path, char * argv[]);
PUBLIC 	int		stat		(const char *path, struct stat *buf);
PUBLIC	int		syslog		(const char *fmt, ...);
PUBLIC 	int 	mount		(int dev, char * dir);
PUBLIC	int		umount		(void);
PUBLIC 	int 	uname		(struct utsname * name);

#endif
