#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#define ISSUE_LEN	256
#define NAME_LEN	30

#define LOGIN "/bin/dash"

void print_banner(char * ttyname);

int main(int argc, char * argv[]) 
{
	int fd_stdin  = open(argv[1], O_RDWR);
	int fd_stdout = open(argv[1], O_RDWR);
	int fd_stderr = open(argv[1], O_RDWR);
	
	print_banner(argv[1]);

	char name[NAME_LEN];
	int len = read(0, name, NAME_LEN);
	name[len] = '\0';

	char * login_argv[] = {"-i", NULL};

	execv(LOGIN, login_argv);

	printf("getty: can't exec!\n");
	while(1);
}

void print_banner(char * ttyname)
{
	struct utsname utsname;
	uname(&utsname);

	char * t;
	if((t = strrchr(ttyname, '/'))) ttyname = t + 1;

	int issue_fd = open("/etc/issue", O_RDONLY);

	if (issue_fd == -1) {
		printf("%s version %s %s %s\n\n%s login: ", utsname.sysname, utsname.release, utsname.nodename, ttyname, utsname.nodename);
	} else {
		char issue[ISSUE_LEN];
		int bytes = read(issue_fd, issue, sizeof(issue));
		issue[bytes] = '\0';
		
		char * p = issue;
		while (*p != '\0') {
			if (*p == '\\') {
				p++;
				switch (*p) {
				case 'n':
					printf("%s", utsname.nodename);
					break;
				case 'v':
					printf("%s", utsname.version);
					break;
				case 'l':
					printf("%s", ttyname);
					break;
				case 'r':
					printf("%s", utsname.release);
					break;
				default:
					break;
				}
			} else {
				printf("%c", *p);
			}
			p++;
		}

		printf("\n\n%s login: ", utsname.nodename);
		close(issue_fd);
	}
	fflush(stdout);
}
