#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/termios.h>

#define PASSWORD_LEN	512

int main(int argc, char * argv[]) 
{
	printf("Password: ");
	fflush(stdout);
	
	struct termios old_tio, new_tio;
	tcgetattr(STDIN_FILENO, &old_tio);
	new_tio = old_tio;
	new_tio.c_lflag &= (~ECHO);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tio);

	char password[PASSWORD_LEN];

	int len = read(STDIN_FILENO, password, PASSWORD_LEN);
	password[len] = '\0';
	
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_tio);
	printf("\n\n");

	//check_pass(argv[1], password);

	while(1);
}
