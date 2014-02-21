#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>

#define GETTY "/usr/bin/getty"

int main(int argc, char * argv[])
{
	mount("/dev/hd1a", "/", "ext2", 0, NULL);
	int fd_stdin  = open("/dev/tty0", O_RDWR);
	int fd_stdout = open("/dev/tty0", O_RDWR);
	int fd_stderr = open("/dev/tty0", O_RDWR);

	int fd_motd = open("/etc/motd", O_RDWR);
	if (fd_motd != -1) {
		char * motd = (char*)malloc(128);
		read(fd_motd, motd, 128);
		close(fd_motd);
		printf("\n\n%s\n\n", motd);
		free(motd);
	}

	char * ttylist[] = {"/dev/tty0", "/dev/tty1", "/dev/tty2"};
	int i = fork();
	if (i) {
		printf("Parent\n");
		while (1) {
			int s;
			wait(&s);
		}
	} else {
		close(fd_stdin);
		close(fd_stdout);
		close(fd_stderr);
		
		char * argv[2];
		argv[0] = GETTY;
		argv[1] = ttylist[0];
		//execv(GETTY, argv);
		while(1);
	}

	return 0;
}
