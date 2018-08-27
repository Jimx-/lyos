#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <regex.h>

int regcomp(regex_t *preg, const char *regex, int cflags)
{
    
}

int regexec(const regex_t *preg, const char *string, size_t nmatch,
            regmatch_t pmatch[], int eflags)
{
    
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf,
                size_t errbuf_size)
{
    
}

void regfree(regex_t *preg)
{
    
}
