#ifndef DIRENT_H_
#define DIRENT_H_


struct dirent   {   
	long d_ino; /* inode number */  
	off_t d_off; /* offset to this dirent */  
	unsigned short d_reclen; /* length of this d_name */  
	unsigned char d_type; /* the type of d_name */  
	char d_name [256]; /* file name (null-terminated) */  
};

typedef struct DIR {
	int fd;
	int entry;
	struct dirent * direntp;
} DIR;

DIR * opendir (const char * dirname);
int closedir (DIR * dir);
struct dirent * readdir (DIR * dirp);

#endif
