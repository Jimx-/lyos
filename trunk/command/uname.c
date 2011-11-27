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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>.  */

#include "type.h"
#include "stdio.h"
#include "unistd.h"

void usage(int status){
	if (status == 0){
		printf("uname: unrecognized option.\n");
		printf("Try 'uname -h' for more information.\n");
	}
	else{
		printf("Usage : uname [option]\n");
		printf("option:\n");
		printf("-a    system infomations\n");
		printf("-s    kernel name\n");
		printf("-n    network node hostname\n");
		printf("-r    kernel release\n");
		printf("-v    kernel version\n");
		printf("-m    machine hardware name\n");
	}
	exit(1);
}

int main(int argc, char * argv[])
{
	int ok;
	struct utsname * name;

	uname(name);

	int i = 1;
	int j;
	while (argv[i][0] == '-'){
		j = 1;
		while(argv[i][j]){
			switch(argv[i][j]){
			case 'a':
				ok = 1;
				printf("%s %s %s %s %s\n", name->sysname, name->nodename, name->release, name->version, name->machine);
				break;
			case 's':
				ok = 1;
				printf("%s\n", name->sysname);
				break;
			case 'n':
				ok = 1;
				printf("%s\n", name->nodename);
				break;
			case 'r':
				ok = 1;
				printf("%s\n", name->release);
				break;
			case 'v':
				ok = 1;
				printf("%s\n", name->version);
				break;
			case 'm':
				ok = 1;
				printf("%s\n", name->machine);
				break;
			default:
				usage(0);
				break;

			}
			j++;
		}
		i++;
	}
	if (!ok)printf("%s\n", name->sysname);

	return 0;
}
