#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/stat.h"

int main(int argc, char * argv[]){

	int fd = open(argv[1], O_RDWR);
	if (fd == -1){
		printf("open file failed.\n");
		return 1;
	}

	struct stat st;
	stat(argv[1], &st);
	char bufr;

	read(fd, &bufr, st.st_size);
	printf("%s\n", &bufr);
		
	close(fd);
	return 0;
}
