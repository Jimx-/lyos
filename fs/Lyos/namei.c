/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */
    
#include "type.h"
#include "stdio.h"
#include "unistd.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"
#include "hd.h"
#include "fs.h"

PUBLIC char dot1[2] = ".";
PUBLIC char dot2[3] = "..";	

PRIVATE char * get_name(char * old_name, char string[MAX_FILENAME_LEN]);
#define SEARCH_FILE_V2

/**********************************************************************
 * 						namei
 * ********************************************************************/
/**
 * 
 * Determine the inode of the given path name.
 * 
 * @param path The full pathname
 * 
 * @return i_num of path inode if successful, otherwisr zero.
 * 
 * ********************************************************************/
PUBLIC int namei(char * path)
{
	struct inode *ldip, *rip;
	char string[MAX_FILENAME_LEN];	/* hold 1 path component name here */

  	/* First open the path down to the final directory. */
  	if ( (ldip = last_dir(path, string)) == NULL) {
		return 0;	/* we couldn't open final directory */
	}

  	/* The path consisting only of "/" is a special case, check for it. */
  	if (string[0] == '\0') return ldip->i_num;

  	int inr = find_entry(ldip, string);
	rip = get_inode(ldip->i_dev, inr);
  	put_inode(ldip);
  	return rip -> i_num;
}

/*****************************************************************************
 *				last_dir				     *
 *****************************************************************************/
PUBLIC struct inode *last_dir(char *path, char string[MAX_PATH])
{
	struct inode *rip;
	char *new_name;
	struct inode *new_ip;

  	rip = (*path == '/' ? current->root : current->pwd);

  	if (*path == '\0') {
		return NULL;
  	}

	/* Scan the path component by component. */
	while (TRUE) {
	/* Extract one component. */
	if ( (new_name = get_name(path, string)) == (char*) 0) {
		put_inode(rip);
		return NULL;
	}
	if (*new_name == '\0') {
		if (rip->i_mode == I_DIRECTORY) {
			return(rip);	/* normal exit */
		} else {
			/* last file of path prefix is not a directory */
			put_inode(rip);			
			return NULL;
		}
        }

	/* There is more path.  Keep parsing. */
	int inr = find_entry(rip, string);
	new_ip = get_inode(rip->i_dev, inr);
	put_inode(rip);		
	if (new_ip == NULL) return NULL;

	/* Continue to next */
	path = new_name;
	rip = new_ip;
  }
}

/*********************************************************************
 * 						get_name									 *
 ********************************************************************/
/**
 *																	
 *  Parse the path.								   			
 * 																	   			
 *  e.g. After path = get_name("/var/log/foo", dir) finishes, we get: 
 * 			- path:"/log/foo"										   				
 * 			- dir :"var"											   	
 * 																	   		
 ********************************************************************/ 
PRIVATE char * get_name(char * old_name, char string[MAX_FILENAME_LEN])
{
  int c;
  char *np, *rnp;

  np = string;			/* 'np' points to current position */
  rnp = old_name;		/* 'rnp' points to unparsed string */
  while ( (c = *rnp) == '/') rnp++;	/* skip leading slashes */

  while ( rnp < &old_name[MAX_PATH]  &&  c != '/'   &&  c != '\0') {
	if (np < &string[MAX_FILENAME_LEN]) *np++ = c;
	c = *++rnp;		/* advance to next character */
  }

  while (c == '/' && rnp < &old_name[MAX_PATH]) c = *++rnp;

  if (np < &string[MAX_FILENAME_LEN]) *np = '\0';	/* Terminate string */

  return(rnp);
}
 
/*********************************************************************
 * 						get_path									 *
 ********************************************************************/
/**
 *																	
 *  Parse the path.								   			
 * 																	   			
 *  e.g. After path = get_path("/var/log/foo") finishes, we get: 
 * 			- path:"/var/log"										   														   	
 * 																	   		
 ********************************************************************/ 
/*PUBLIC char * get_path(char * path)
{
	int i, j;
	int namelen = strlen(path);
	char * c;
	char *np, *rnp;

	np = path;
	
	for (i=namelen-1;i>=0;i--){
		if (np[i] == '/') break;
	}
	
	memcpy(rnp, np, i);
	
	return rnp;
}
 */
/*********************************************************************
 *                                find_entry
 ********************************************************************/
/**
 * Find the dir_entry and return its inode_nr.
 *
 * @param dir_inode The inode of the directory where to find.
 * @return         	Ptr to the i-node of the file if successful, 
 * 					otherwise zero.
 * 
 * @see open()
 * @see do_open()
 ********************************************************************/
PUBLIC int find_entry(struct inode * dir_inode, char filename[MAX_PATH])
{
    	int i, j;
	int dir_blk0_nr = dir_inode->i_start_sect;
	int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
	int nr_dir_entries =
	  dir_inode->i_size / DIR_ENTRY_SIZE; /**
					       * including unused slots
					       * (the file has been deleted
					       * but the slot is still there)
					       */
	int m = 0;
	struct dir_entry * pde;

    	if (dir_inode->i_mode != I_DIRECTORY) return 0;

	for (i = 0; i < nr_dir_blks; i++) {
		RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
		pde = (struct dir_entry *)fsbuf;
		for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
			if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0)
				return pde->inode_nr;
			if (++m > nr_dir_entries)
				break;
		}
		if (m > nr_dir_entries) /* all entries have been iterated */
			break;
	}
	return 0;
}

/*****************************************************************************
 *                                search_file
 *****************************************************************************/
/**
 * Search the file and return the inode_nr.
 *
 * @param[in] path The full path of the file to search.
 * @return         Ptr to the i-node of the file if successful, otherwise zero.
 * 
 * @see open()
 * @see do_open()
 *****************************************************************************/
PUBLIC int search_file(char * path)
{
    
#ifdef SEARCH_FILE_V1
	int i, j;

	char filename[MAX_PATH];
	memset(filename, 0, MAX_FILENAME_LEN);
	struct inode * dir_inode;
	if (strip_path(filename, path, &dir_inode) != 0)
		return 0;

	if (filename[0] == 0)	/* path: "/" */
		return dir_inode->i_num;

	/**
	 * Search the dir for the file.
	 */
	int dir_blk0_nr = dir_inode->i_start_sect;
	int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
	int nr_dir_entries =
	  dir_inode->i_size / DIR_ENTRY_SIZE; /**
					       * including unused slots
					       * (the file has been deleted
					       * but the slot is still there)
					       */
	int m = 0;
	struct dir_entry * pde;
	for (i = 0; i < nr_dir_blks; i++) {
		RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
		pde = (struct dir_entry *)fsbuf;
		for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
			if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0)
				return pde->inode_nr;
			if (++m > nr_dir_entries)
				break;
		}
		if (m > nr_dir_entries) /* all entries have been iterated */
			break;
	}

	/* file not found */
	return 0;
#endif
#ifdef SEARCH_FILE_V2
    return namei(path);
#endif

}

/*****************************************************************************
 *                                strip_path
 *****************************************************************************/
/**
 * Get the basename from the fullpath.
 *
 * 
 *
 *
 * This routine should be called at the very beginning of file operations
 * such as open(), read() and write(). It accepts the full path and returns
 * two things: the basename and a ptr of the root dir's i-node.
 *
 * e.g. After stip_path(filename, "/etc/log/foo", ppinode) finishes, we get:
 *      - filename: "foo"
 *      - *ppinode: the ptr of log's inode 
 *      - ret val:  0 (successful)
 *
 * Currently an acceptable pathname should begin with at most one `/'
 * preceding a filename.
 *
 * Filenames may contain any character except '/' and '\\0'.
 *
 * @param[out] filename The string for the result.
 * @param[in]  pathname The full pathname.
 * @param[out] ppinode  The ptr of the dir's inode will be stored here.
 * 
 * @return Zero if success, otherwise the pathname is not valid.
 *****************************************************************************/
PUBLIC int strip_path(char * filename, const char * pathname,
		      struct inode** ppinode)
{
	const char * s = pathname;
	char * t = filename;

	if (s == 0)
		return -1;

	if (*s == '/')
		s++;

	while (*s) {		/* check each character */
		if (*s == '/')
			return -1;
		*t++ = *s++;
		/* if filename is too long, just truncate it */
		if (t - filename >= MAX_FILENAME_LEN)
			break;
	}
	*t = 0;

	*ppinode = root_inode;

	return 0;
}

