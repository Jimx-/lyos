
/* Lyos' FS */

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "lyos/protect.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "lyos/fs.h"
#include "sys/stat.h"

/*****************************************************************************
 *                                do_stat
 *************************************************************************//**
 * Perform the stat() syscall.
 * 
 * @return  On success, zero is returned. On error, -1 is returned.
 *****************************************************************************/
PUBLIC int do_stat(MESSAGE * p)
{
	struct inode * pin = 0;
	int src = p->source;	/* caller proc nr. */
	char pathname[MAX_PATH]; /* parameter from the caller */
	char filename[MAX_PATH]; /* directory has been stipped */
	int name_len = p->NAME_LEN;	/* length of filename */
	int inode_nr;
	int fd = p->FD;

	switch (p->type) {
	case STAT:

		/* get parameters from the message */

		assert(name_len < MAX_PATH);
		phys_copy((void*)va2la(TASK_FS, pathname),    /* to   */
			  (void*)va2la(src, p->PATHNAME), /* from */
			  name_len);
		pathname[name_len] = 0;	/* terminate the string */
	
		inode_nr = search_file(pathname);
		if (inode_nr == INVALID_INODE) {	/* file not found */
			printl("do_stat():: search_file() returns "
			       "invalid inode: %s\n", pathname);
			return -1;
		}

		struct inode * dir_inode;
		if (strip_path(filename, pathname, &dir_inode) != 0) {
			/* theoretically never fail here
			 * (it would have failed earlier when
			 *  search_file() was called)
			 */
			assert(0);
		}
		pin = get_inode(dir_inode->i_dev, inode_nr);
		break;
	case FSTAT:
		pin = pcaller->filp[fd]->fd_inode;
		break;
	}

	struct stat s;		/* the thing requested */
	s.st_dev = pin->i_dev;
	s.st_ino = pin->i_num;
	s.st_mode= pin->i_mode;
	s.st_rdev= is_special(pin->i_mode) ? pin->i_start_sect : NO_DEV;
	s.st_size= pin->i_size;

	put_inode(pin);

	phys_copy((void*)va2la(src, p->BUF), /* to   */
		  (void*)va2la(TASK_FS, &s),	 /* from */
		  sizeof(struct stat));

	return 0;
}

