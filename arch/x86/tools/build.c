/*
 *	arch/x86/tools/build.c
 *
 *	used for building a bootable image 
 *
 */

#include <stdio.h>	/* fprintf */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>	/* unistd.h needs this */
#include <sys/stat.h>
#include <unistd.h>	/* contains read/write */
#include <fcntl.h>

#define KERNEL_SIZE 0x20000 /* 128 kB */

#define LOADER_SECTS 16 /* 8 kB */

#define TOSTR(x) #x

void error(char * msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

void usage(void)
{
	error("Usage: build boot loader kernel [> image]");
}

int main(int argc, char ** argv)
{
	int i,c,id;
	char buf[1024];
	struct stat sb;

	if ((argc < 4) || (argc > 5))
		usage();

	if ((id = open(argv[1], O_RDONLY, 0)) < 0)
		error("Unable to open 'boot'");

	i = read(id, buf, sizeof(buf));
	fprintf(stderr, "Boot sector %d bytes.\n",i);
	if (i != 512)
		error("Boot block must be exactly 512 bytes");
	if ((*(unsigned short *)(buf+510)) != 0xAA55)
		error("Boot block hasn't got boot flag (0xAA55)");
	i = write(1,buf,512);
	if (i!=512)
		error("Write call failed");
	close (id);
	
	if ((id=open(argv[2],O_RDONLY,0))<0)
		error("Unable to open 'loader'");

	for (i=0 ; (c=read(id,buf,sizeof(buf)))>0 ; i+=c )
		if (write(1,buf,c)!=c)
			error("Write call failed");
	close (id);

	if (i > LOADER_SECTS*512)
		error("Loader exceeds " TOSTR(LOADER_SECTS) " sectors\n");
	fprintf(stderr,"Loader is %d bytes.\n",i);

	for (c=0 ; c<sizeof(buf) ; c++)
		buf[c] = '\0';
	while (i<LOADER_SECTS*512) {
		c = LOADER_SECTS*512-i;
		if (c > sizeof(buf))
			c = sizeof(buf);
		if (write(1,buf,c) != c)
			error("Write call failed");
		i += c;
	}
	
	if ((id=open(argv[3],O_RDONLY,0))<0)
		error("Unable to open 'kernel'");
	for (i=0 ; (c=read(id,buf,sizeof(buf)))>0 ; i+=c )
		if (write(1,buf,c)!=c)
			error("Write call failed");
	close(id);
	fprintf(stderr,"Kernel is %d bytes.\n",i);
	if (i > KERNEL_SIZE*16)
		error("Kernel is too big");
	return(0);
}
