#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main()
{
	int fd_stdin  = open("/dev/tty0", O_RDWR);
	int fd_stdout = open("/dev/tty0", O_RDWR);
	int fd_stderr = open("/dev/tty0", O_RDWR);

	//char * string = malloc(4096);
	printf("Hello world!\n");

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
