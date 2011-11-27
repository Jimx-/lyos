#include "stdio.h"
#include "unistd.h"
#include "type.h"

int main(int argc, char * argv[]){

	int i; 
	char pwdr[20];
	char pwd[20];

	int ul = open(argv[1], O_RDWR);
	if (ul == -1)
	{
		printf("su: user not found.\n");
		return 0;
	}

	int m = read(ul, pwd, 20);
	pwd[m] = 0;

	while (1){

		printf("[su]password:");
		int n = read(0, pwdr, 20);
		pwdr[n] = 0;

		int ret = strcmp(pwdr, pwd);

		if (ret == 0) break;

		printf("Sorry,try again.\n");
	}
	return 0;
}

