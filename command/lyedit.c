#include "type.h"
#include "stdio.h"

int main(int argc, char * argv[])
{
	int m;
	int n;
	char * buf[80];
	int fd = open(argv[1], O_RDWR);
	if (fd == -1){
		printf("unable to open file");
		exit;
	}

	while (1){
		m = read(0, buf, 80);
		buf[m] = 0;
		printf("%s\n", buf);
		if (strcmp(buf, ":") == 0) break;
		n = write(fd, buf, m);
	}
		
}
