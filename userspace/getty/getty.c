#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char * argv[]) 
{
	int fd_stdin  = open(argv[1], O_RDWR);
	int fd_stdout = open(argv[1], O_RDWR);
	int fd_stderr = open(argv[1], O_RDWR);

	printf("%s\n", argv[1]);
	
	while(1);
}
