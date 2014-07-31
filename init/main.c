#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>

#define GETTY "/usr/bin/getty"
#define NR_TTY	3

int main(int argc, char * argv[])
{
	mount("/dev/hd1a", "/", "ext2", 0, NULL);
	int fd_stdin  = open("/dev/tty0", O_RDWR);
	int fd_stdout = open("/dev/tty0", O_RDWR);
	int fd_stderr = open("/dev/tty0", O_RDWR);

	//printf("Hello world\n");

	/*int fd_motd = open("/etc/motd", O_RDWR);
	if (fd_motd != -1) {
		char * motd = (char*)malloc(2987);
		int rd_cnt = 2987;
		read(fd_motd, motd, rd_cnt);
		close(fd_motd);
		int cnt = 0, j;
		for (j = 0; j < rd_cnt; j ++) if (motd[j] == 0) cnt ++;
		motd[rd_cnt] = '\0';
		printf("%d\n\n%s\n\n", cnt,motd);
		free(motd);
	}
	while(1);*/
	char * ttylist[NR_TTY] = {"/dev/tty0", "/dev/tty1", "/dev/tty2"};
	int i;
	for (i = 0; i < NR_TTY; i++) {
		int pid = fork();
		if (pid) {
			printf("Parent\n");

		} else {
			close(fd_stdin);
			close(fd_stdout);
			close(fd_stderr);
		
			char * argv[2];
			argv[0] = GETTY;
			argv[1] = ttylist[i];
			_exit(execv(GETTY, argv));
		}
	}

	while (1) {
		int s;
		wait(&s);
	}

	return 0;
}
