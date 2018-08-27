#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/shm.h>

#define GETTY "/usr/bin/getty"
#define NR_TTY	4

int main(int argc, char * argv[])
{
	int fd_stdin  = open("/dev/console", O_RDWR);
	int fd_stdout = open("/dev/console", O_RDWR);
	int fd_stderr = open("/dev/console", O_RDWR);

	int pid_rc = fork();
	if (pid_rc) {
		waitpid(pid_rc, NULL, 0);
	} else {
		char * rc_args[] = {"/bin/sh", "/etc/rc", NULL};
		char * rc_env[] = {NULL};
		execve("/bin/sh", rc_args, rc_env);
	}

	/* set hostname */
	int fd_hostname = open("/etc/hostname", O_RDONLY);
	if (fd_hostname != -1) {
		char hostname[256];
		memset(hostname, 0, sizeof(hostname));
		int len = read(fd_hostname, hostname, sizeof(hostname));
		printf("Setting hostname to %s\n", hostname);
		sethostname(hostname, len);
		close(fd_hostname);
	}
	
	char * ttylist[NR_TTY] = {"/dev/tty1", "/dev/tty2", "/dev/tty3", "/dev/ttyS0"};
	int i;
	for (i = 0; i < NR_TTY; i++) {
		int pid = fork();
		if (pid) {
			//printf("Parent\n");
		} else {
			close(fd_stdin);
			close(fd_stdout);
			close(fd_stderr);

			open(ttylist[i], O_RDWR);
			open(ttylist[i], O_RDWR);
			open(ttylist[i], O_RDWR);

			char * argv[] = {GETTY, NULL};
			_exit(execv(GETTY, argv));
		}
	}

	/*
	int fb = open("/dev/fb0", O_RDWR);
	int* fbp = mmap(NULL, 4000, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
	int* p;
	for (p = fbp, i = 0; i < 1000; i++) {
		*p++ = 0xff0000;
	}
	close(fb); */
	
	/*
	int shmid = shmget(IPC_PRIVATE, 0x100, IPC_CREAT | 0600);
	int* shm = shmat(shmid, NULL, SHM_RDONLY);
	*shm = 100;
	*/
	while (1) {
		int s;
		wait(&s);
	}

	return 0;
}
