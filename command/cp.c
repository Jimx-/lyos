#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/stat.h"

int main(int argc, char * argv[]){

	int src = open(argv[1], O_RDWR);
	if (src == -1){
		printf("open %s failed.\n", argv[1]);
		return 1;
	}
	
	int dst = open(argv[2], O_CREAT | O_RDWR);
	if (dst == -1){
		dst = open(argv[2], O_RDWR);
		if (dst == -1) return 1;
		printf("file already exists\n");
	}
	
	printf("\"%s\" -> \"%s\"\n", argv[1], argv[2]);

	struct stat st;
	stat(argv[1], &st);
	char buf;

	int m = read(src, &buf, st.st_size);
	write(dst, &buf, m); 
	printf("%d bytes copied.\n", m);
	close(src);
	close(dst);
	return 0;
}