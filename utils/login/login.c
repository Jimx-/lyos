#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <limits.h>
#include <pwd.h>

#define NAME_LEN	30

char * getloginname()
{
	struct utsname utsname;
	uname(&utsname);

	printf("\n%s login: ", utsname.nodename);

	static char name[NAME_LEN];
	fgets(name, NAME_LEN, stdin);
	name[strlen(name) - 1] = '\0';

	return name;
}
				
int main(int argc, char * argv[]) 
{
	int ask = 0;
	char * username, * p, * prompt;
	struct passwd * pwd;

	if (*argv) {
		username = *argv;
	} else {
		ask = 1;
	}

	for (;; ask = 1) {
		int ok = 0;

		if (ask) {
			username = getloginname();
		}

		pwd = getpwnam(username);

		if (pwd) {
			if (pwd->pw_passwd[0] == '\0') {
				ok = 1;
				goto check;
			}
		}

		prompt = "Password: ";
		p = getpass(prompt);
		printf("\n");

		ok = 1;

		if (pwd == NULL) {
			ok = 0;
			goto check;
		}

check:
		if (pwd && ok) break;

		printf("Login incorrect\n");
	}

	struct stat sbuf;
    if (stat("/etc/motd", &sbuf) == 0) {
    	char * motd = malloc(sbuf.st_size + 1);
    	int fd = open("/etc/motd", O_RDONLY);
    	read(fd, motd, sbuf.st_size);
    	motd[sbuf.st_size] = '\0';
    	printf("%s\n", motd);
    	close(fd);
    	free(motd);
    }

    char * shell_argv[] = {pwd->pw_shell, NULL};
    int r = execv(pwd->pw_shell, shell_argv);
    
	setuid(pwd->pw_uid);
	setgid(pwd->pw_gid);

	if (chdir(pwd->pw_dir) != 0) {
		printf("No home directory %s!\n", pwd->pw_dir);
	}

	while(1);
}

