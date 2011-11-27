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
#include "sys/const.h"
#include "time.h"

char * opt;
int ok;

void usage(int status){
	if (status == 0){
		printf("date: unrecognized option'%s'\n", opt);
		printf("Try 'date -h' for more information.\n");
	}
	else{
		printf("Usage : date [option]\n");
		printf("option:\n");
		printf("-y    Display year\n");
		printf("-m    Display month\n");
		printf("-d    Display day\n");
		printf("-H    Display hour\n");
		printf("-M    Display minute\n");
		printf("-s    Display second\n");
	}
	exit(1);
}
int main(int argc, char * argv[]){
	
	MESSAGE msg;

	msg.type = GET_RTC_TIME;
	send_recv(BOTH, TASK_SYS, &msg);

	struct time * t;

	t = (struct time*)msg.BUF;

	ok = 0;

	int i = 1;
	int j;
	while (argv[i][0] == '-'){
		j = 1;
		while(argv[i][j]){
			opt = argv[i][j];
			switch(argv[i][j]){
			case 'y':
				ok = 1;
				printf("year: %d\n", t->year);
				break;
			case 'm':
				ok = 1;
				printf("month: %d\n", t->month);
				break;
			case 'M':
				ok = 1;
				printf("minute: %d\n", t->minute);
				break;
			case 'd':
				ok = 1;
				printf("day: %d\n", t->day);
				break;
			case 'h':
				ok = 1;
				usage(1);
				break;
			case 'H':
				ok = 1;
				printf("hour: %d\n", t->hour);
				break;
			case 's':
				ok = 1;
				printf("second: %d\n", t->second);
				break;
			default:
				usage(0);
				break;

			}
			j++;
		}
		i++;
	}
	if (!ok) printf("%d/%d/%d  %d:%d:%d\n", t->month, t->day, t->year, t->hour, t->minute, t->second);

	return 0;
}
