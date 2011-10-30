#include "stdio.h"
#include "unistd.h"
#include "sys/console.h"

int main(int argc, char * argv[]){

	int fd = open(argv[1], O_RDWR);
	if (fd == -1){
		printf("open file failed.\n");
		return 1;
	}

	struct stat * st;
	int i = stat(argv[1], st);
	char * bufr;

	int n = read(fd, bufr, st->st_size);
	printf("%s\n", bufr);
		
	close(fd);
	return 0;
}
