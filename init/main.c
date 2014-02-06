#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

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

	//int i = fork();
	//if (i) {
		//printf("parent\n");
		while (1) {
			int s;
			wait(&s);
		}
	//} else {
		//printf("child\n");
	//	while(1);
	//}
}
