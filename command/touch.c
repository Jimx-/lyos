/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include "stdio.h"
#include "unistd.h"
#include "type.h"

void usage();

int main(int argc, char * argv[]){

	if (argc == 1){
		printf("touch: file arguments missing.\n");
		usage();
		return 0;
	}

	int check = open(argv[1], O_RDWR);
	if (check != -1){
		printf("touch: file exists.\n");
		return 0;
	}

	int fd = open(argv[1], O_CREAT | O_RDWR);
	close(fd);

	if (fd == -1){
		printf("create file failed.\n");
	}
	else{
		printf("%s created.\n",argv[1]);
	}
}

void usage(){
	printf("Usage: touch file...\n");
	return;
}
