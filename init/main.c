#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main()
{
	int fd_stdin  = open("/dev/tty0", O_RDWR);
	int fd_stdout = open("/dev/tty0", O_RDWR);
	int fd_stderr = open("/dev/tty0", O_RDWR);

	char hello[] = "Hello world!\n";
	write(1, hello, strlen(hello));
	//printf("Hello world\n");
	while (1) {
		int s;
		wait(&s);
	}
}
