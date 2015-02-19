#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/termios.h>
#include <limits.h>
#include <pwd.h>

char * getpass(const char * prompt)
{
	printf(prompt);
	fflush(stdout);
	
	struct termios old_tio, new_tio;
	tcgetattr(STDIN_FILENO, &old_tio);
	new_tio = old_tio;
	new_tio.c_lflag &= (~ECHO);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tio);

	static char password[_PASSWORD_LEN];

	int len = read(STDIN_FILENO, password, _PASSWORD_LEN);
	password[len] = '\0';
	
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_tio);
	return password;
}
