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
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "fcntl.h"
#include "sys/wait.h"
#include "sys/utsname.h"
#include "lyos/compile.h"
#include "errno.h"
#include "multiboot.h"

#define LYOS_BANNER "Lyos version "UTS_VERSION" (compiled by "LYOS_COMPILE_BY"@"LYOS_COMPILE_HOST")("LYOS_COMPILER"), compiled on "LYOS_COMPILE_TIME". \n"

PUBLIC void init_arch();

/*****************************************************************************
 *                               kernel_main
 *****************************************************************************/
/**
 * jmp from kernel.asm::_start. 
 * 
 *****************************************************************************/
PUBLIC int kernel_main()
{

	init_arch();

	jiffies = 0;

	init_clock();
    init_keyboard();

	restart();

	while(1){}
}


/*****************************************************************************
 *                                get_ticks
 *****************************************************************************/
PUBLIC int get_ticks()
{
	MESSAGE msg;
	reset_msg(&msg);
	msg.type = GET_TICKS;
	send_recv(BOTH, TASK_SYS, &msg);
	return msg.RETVAL;
}


/**
 * @struct posix_tar_header
 * Borrowed from GNU `tar'
 */
struct posix_tar_header
{				/* byte offset */
	char name[100];		/*   0 */
	char mode[8];		/* 100 */
	char uid[8];		/* 108 */
	char gid[8];		/* 116 */
	char size[12];		/* 124 */
	char mtime[12];		/* 136 */
	char chksum[8];		/* 148 */
	char typeflag;		/* 156 */
	char linkname[100];	/* 157 */
	char magic[6];		/* 257 */
	char version[2];	/* 263 */
	char uname[32];		/* 265 */
	char gname[32];		/* 297 */
	char devmajor[8];	/* 329 */
	char devminor[8];	/* 337 */
	char prefix[155];	/* 345 */
	/* 500 */
};

/*****************************************************************************
 *                                untar
 *****************************************************************************/
/**
 * Extract the tar file and store them.
 * 
 * @param filename The tar file.
 *****************************************************************************/
void untar(const char * filename)
{
	printf("[Uncompressing files...\n");
	int fd = open(filename, O_RDWR);
	assert(fd != -1);

	char buf[SECTOR_SIZE * 16];
	int chunk = sizeof(buf);
	int i = 0;
	int bytes = 0;

	while (1) {
		bytes = read(fd, buf, SECTOR_SIZE);
		assert(bytes == SECTOR_SIZE); /* size of a TAR file
					       * must be multiple of 512
					       */
		if (buf[0] == 0) {
			if (i == 0)
				printf("   need not uncompress.\n");
			break;
		}
		i++;

		struct posix_tar_header * phdr = (struct posix_tar_header *)buf;

		/* calculate the file size */
		char * p = phdr->size;
		int f_len = 0;
		while (*p)
			f_len = (f_len * 8) + (*p++ - '0'); /* octal */

		int bytes_left = f_len;
		int fdout = open(phdr->name, O_CREAT | O_RDWR | O_TRUNC);
		if (fdout == -1) {
			printf("failed to uncompress file: %s\n", phdr->name);
			printf("aborted\n");
			close(fd);
			return;
		}
		printf("    %s (%d bytes)\n", phdr->name, (int)*phdr->size);
		while (bytes_left) {
			int iobytes = min(chunk, bytes_left);
			read(fd, buf,
			     ((iobytes - 1) / SECTOR_SIZE + 1) * SECTOR_SIZE);
			bytes = write(fdout, buf, iobytes);
			assert(bytes == iobytes);
			bytes_left -= iobytes;
		}
		close(fdout);
	}

	if (i) {
		lseek(fd, 0, SEEK_SET);
		buf[0] = 0;
		bytes = write(fd, buf, 1);
		assert(bytes == 1);
	}

	close(fd);

	printf("done.]\n");
}

/*****************************************************************************
 *                                shell
 *****************************************************************************/
/**
 * A very very simple shell.
 * 
 * @param tty_name  TTY file name.
 *****************************************************************************/
void shell(const char * tty_name)
{
	int fd_stdin  = open(tty_name, O_RDWR);
	assert(fd_stdin  == 0);
	int fd_stdout = open(tty_name, O_RDWR);
	assert(fd_stdout == 1);

	char rdbuf[128];

	struct utsname name;
	uname(&name);
	while (1) {
		printf("[root@%s]$", name.nodename);
		int r = read(fd_stdin, rdbuf, 70);
		rdbuf[r] = 0;

		int argc = 0;
		char * argv[PROC_ORIGIN_STACK];
		char * p = rdbuf;
		char * s;
		int word = 0;
		char ch;
		do {
			ch = *p;
			if (*p != ' ' && *p != 0 && !word) {
				s = p;
				word = 1;
			}
			if ((*p == ' ' || *p == 0) && word) {
				word = 0;
				argv[argc++] = s;
				*p = 0;
			}
			p++;
		} while(ch);
		argv[argc] = 0;




		int fd = open(argv[0], O_RDWR);
		if (fd == -1) {
			if (rdbuf[0]) {
				printf("shell:Command not found.\n");
			}
		}
		else {
			close(fd);
			int pid = fork();
			if (pid != 0) { /* parent */
				int s;
				wait(&s);
			}
			else {	/* child */
				execv(argv[0], argv);
			}
		}
	}

	close(1);
	close(0);
}

/*****************************************************************************
 *                                Init
 *****************************************************************************/
/**
 * The hen.
 * 
 *****************************************************************************/
void Init()
{
	int fd_stdin  = open("/dev/tty0", O_RDWR);
	int fd_stdout = open("/dev/tty0", O_RDWR);
	int fd_stderr = open("/dev/tty0", O_RDWR);
	
	/*
	printf("\n");
	printf(LYOS_BANNER);
	printf("(c)Copyright Jimx 2010-2012\n\n");
	*/
	for(;;);

	printf("Init() is running ...\n");

	/* fd test */
	char buf[3];
	MESSAGE m;
	m.type = DEV_READ;
	m.DEVICE = MAKE_DEV(2, 0);
	m.BUF = &buf;
	m.CNT = 3;
	m.POSITION = 0;
	send_recv(BOTH, TASK_FD, &m);
	printl("%d\n", buf[0]);

	/* extract `cmd.tar' */
	untar("/cmd.tar");
	
	printf("\n");
	printf(LYOS_BANNER);
	printf("(c)Copyright Jimx 2010-2012\n\n");\

	char * tty_list[] = {"/dev_tty0","/dev_tty1", "/dev_tty2"};
	printf("\n");

	int i;
	for (i = 0; i < sizeof(tty_list) / sizeof(tty_list[0]); i++) {
		int pid = fork();
		if (pid != 0) { /* parent process */
			/* printf("[parent is running, child pid:%d]\n", pid); */
		}
		else {	/* child process */
			/* printf("[child is running, pid:%d]\n", getpid()); */
			close(fd_stdin);
			close(fd_stdout);
			
			shell(tty_list[i]);
			assert(0);
		}
	}

	while (1) {
		int s;
		/* int child = */wait(&s);
		//printf("child (%d) exited with status: %d.\n", child, s);
	}

	assert(0);
}

/* test only. */
/*======================================================================*
                               TestA
 *======================================================================*/
void TestA()
{
	int fd;
	const char filename[] = "/dev/tty0";
	fd=open(filename, O_RDWR);
	close(fd);
	for(;;);
	const char bufw[] = "def";
	char bufr[3];
	int n;
	fd=open(filename, O_CREAT|O_RDWR);
	printf("File created: fd: %d\n", fd);
	n=write(fd, bufw, strlen(bufw));
	close(fd);
	fd=open(filename, O_RDWR);
	n=read(fd, bufr, 3);
	bufr[n] =0;
	printf("%s\n", bufr);
	close(fd);
	for(;;);
}

/*======================================================================*
                               TestB
 *======================================================================*/
void TestB()
{
	for(;;);
}

/*======================================================================*
                               TestB
 *======================================================================*/
void TestC()
{
	for(;;);
}

/*****************************************************************************
 *                                panic
 *****************************************************************************/
PUBLIC void panic(const char *fmt, ...)
{
	char buf[256];

	/* 4 is the size of fmt in the stack */
	va_list arg = (va_list)((char*)&fmt + 4);

	vsprintf(buf, fmt, arg);

	printl("Kernel panic: %s", buf);

	/* should never arrive here */
	__asm__ __volatile__("ud2");
}
