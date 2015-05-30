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

#define LOGIN "/usr/bin/login"

void print_banner(char * ttyname);

typedef struct
{
  unsigned a_type;		/* Entry type */
  union
    {
      unsigned a_val;		/* Integer value */
      /* We use to have pointer elements added here.  We cannot do that,
	 though, since it does not work when using 32-bit definitions
	 on 64-bit platforms and vice versa.  */
    } a_un;
} Elf32_auxv_t;

int main(int argc, char * argv[], char* envp[]) 
{	
	print_banner(ttyname(0));

        Elf32_auxv_t *auxv;
        while(*envp++ != NULL); 
        for (auxv = (Elf32_auxv_t *)envp; auxv->a_type != 0; auxv++)
        {
                if( auxv->a_type == 31)
                        printf("AT_EXECFN is: %s\n", (char*)auxv->a_un.a_val);
                if( auxv->a_type == 15)
                        printf("AT_PLATFORM is: %s\n", (char*)auxv->a_un.a_val);
        }


	char name[NAME_LEN];
	fgets(name, NAME_LEN, stdin);
	name[strlen(name) - 1] = '\0';

	char * login_argv[] = {name, NULL};

	pid_t pgrp = setsid();
	tcsetpgrp(0, pgrp);

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
