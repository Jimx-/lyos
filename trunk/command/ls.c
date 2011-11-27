#include "type.h"
#include "stdio.h"
#include "unistd.h"
#include "../include/sys/fs.h"
#include "../include/sys/const.h"

void usage(int status){
	if (status == 0){
		printf("ls: unrecognized option.\n");
		printf("Try 'ls -h' for more information.\n");
	}
	else{
		printf("Usage : ls [option]\n");
		printf("option:\n");
		printf("-l      long format\n");
	}
	exit(0);
}

int main(int argc, char * argv[])
{
	int m;
	int n;
	int par_l;
	char * buf[80];
	struct dir_entry * pde;
	struct stat * st;
	
	int fd = open("/", O_RDWR);
	m = read(fd, buf, sizeof(struct dir_entry));
	pde = (struct dir_entry *)buf;

	int i = 1;
	int j;
	while (argv[i][0] == '-'){
		j = 1;
		while(argv[i][j]){
			switch(argv[i][j]){
			case 'l':
				n = stat(pde->name, st);
				printf("size\t\tfile name\n");
				printf("~~~~~~~~~~~~~~\t~~~~~~~~~~~~~\n");
				while(pde->inode_nr){
					printf("%d\tByte(s)\t", st->st_size);
					printf("%s", pde->name);
					m = read(fd, buf, sizeof(struct dir_entry));
					pde = (struct dir_entry *)buf;
					n = stat(pde->name, st);
				}
				break;
			case 'F':
				n = stat(pde->name, st);
				printf("size\t\tfile name\n");
				printf("~~~~~~~~~~~~~~\t~~~~~~~~~~~~~\n");
				while(pde->inode_nr){
					printf("%d\tByte(s)\t", st->st_size);
					printf("%s", pde->name);
					if (st->st_mode == I_DIRECTORY) printf("/");
					printf("\n");
					m = read(fd, buf, sizeof(struct dir_entry));
					pde = (struct dir_entry *)buf;
					n = stat(pde->name, st);
			}
			break;
			case 'h':
				usage(1);
				break;
			default:
				usage(0);
				break;
			}
			j++;
		}
		i++;
	}
	
	while(pde->inode_nr){
		printf("%s\t", pde->name);
		m = read(fd, buf, sizeof(struct dir_entry));
		pde = (struct dir_entry *)buf;
	}
	printf("\n");
	return 0;		
}
