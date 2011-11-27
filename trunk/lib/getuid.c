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
#include "proto.h"

/*****************************************************************************
 *                                getuid
 *************************************************************************//**
 * Get the uid of given process
 * 
 * @return The uid
 *****************************************************************************/
PUBLIC int getuid(void)
{
	MESSAGE msg;
	msg.type	= GETUID;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                setuid
 *************************************************************************//**
 * Set the uid of given process
 * 
 * @param uid
 * 
 * @return 
 *****************************************************************************/
PUBLIC int setuid(uid_t uid)
{
	MESSAGE msg;
	msg.type	= SETUID;
	msg.REQUEST = uid;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                getgid
 *************************************************************************//**
 * Get the gid of given process
 * 
 * @return The gid
 *****************************************************************************/
PUBLIC int getgid(void)
{
	MESSAGE msg;
	msg.type	= GETGID;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                setgid
 *************************************************************************//**
 * Set the gid of given process
 * 
 * @param gid
 * 
 * @return 
 *****************************************************************************/
PUBLIC int setgid(gid_t gid)
{
	MESSAGE msg;
	msg.type	= SETUID;
	msg.REQUEST = gid;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                geteuid
 *************************************************************************//**
 * Get the euid of given process
 * 
 * @return The euid
 *****************************************************************************/
PUBLIC int geteuid(void)
{
	MESSAGE msg;
	msg.type	= GETEUID;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                getegid
 *************************************************************************//**
 * Get the egid of given process
 * 
 * @return The egid
 *****************************************************************************/
PUBLIC int getegid(void)
{
	MESSAGE msg;
	msg.type	= GETEGID;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

