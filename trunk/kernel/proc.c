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
#include "tty.h"
#include "console.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "signal.h"

PRIVATE void block(struct proc* p);
PRIVATE void unblock(struct proc* p);
PRIVATE int  msg_send(struct proc* current, int des, MESSAGE* m);
PRIVATE int  msg_receive(struct proc* current, int src, MESSAGE* m);
PRIVATE int  deadlock(int src, int dest);

/*****************************************************************************
 *                                schedule
 *****************************************************************************/
/**
 * <Ring 0> Choose one proc to run.
 * 
 *****************************************************************************/
PUBLIC void schedule()
{
	struct proc*	p;
	int		greatest_counter = 0;

	/* check alarm */
	for (p = &FIRST_PROC; p <= &LAST_PROC; p++) {
		if (p->alarm && p->alarm < jiffies) {
			p->signal |= (1 << (SIGALRM - 1));
			p->alarm = 0;
		}
	}
			
	while (!greatest_counter) {
		for (p = &FIRST_PROC; p <= &LAST_PROC; p++) {
			if (p->state == 0) {
				if (p->counter > greatest_counter) {
					greatest_counter = p->counter;
					current = p;
				}
			}
		}

		if (!greatest_counter)
			for (p = &FIRST_PROC; p <= &LAST_PROC; p++)
				if (p->state == 0)
					p->counter = p->priority;
	}
}

PUBLIC int do_setuid()
{
	current->uid = mm_msg.REQUEST;
	return 0;
}

PUBLIC int do_getuid()
{
	return current->uid;
}

PUBLIC int do_setgid()
{
	current->gid = mm_msg.REQUEST;
	return 0;
}

PUBLIC int do_getgid()
{
	return current->gid;
}

PUBLIC int do_geteuid()
{
	return current->euid;
}

PUBLIC int do_getegid()
{
	return current->egid;
}


PUBLIC int do_nice()
{
	if (current->priority - mm_msg.REQUEST > 0)
		current -= mm_msg.REQUEST;
	else
		return 1;
	return 0;
} 

/*****************************************************************************
 *                                sys_sendrec
 *****************************************************************************/
/**
 * <Ring 0> The core routine of system call `sendrec()'.
 * 
 * @param function SEND or RECEIVE
 * @param src_dest To/From whom the message is transferred.
 * @param m        Ptr to the MESSAGE body.
 * @param p        The caller proc.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int sys_sendrec(int function, int src_dest, MESSAGE* m, struct proc* p)
{
	assert(k_reenter == 0);	/* make sure we are not in ring0 */
	assert((src_dest >= 0 && src_dest < NR_TASKS + NR_PROCS) ||
	       src_dest == ANY ||
	       src_dest == INTERRUPT);

	int ret = 0;
	int caller = proc2pid(p);
	MESSAGE* mla = (MESSAGE*)va2la(caller, m);
	mla->source = caller;

	assert(mla->source != src_dest);

	/**
	 * Actually we have the third message type: BOTH. However, it is not
	 * allowed to be passed to the kernel directly. Kernel doesn't know
	 * it at all. It is transformed into a SEND followed by a RECEIVE
	 * by `send_recv()'.
	 */
	if (function == SEND) {
		ret = msg_send(p, src_dest, m);
		if (ret != 0)
			return ret;
	}
	else if (function == RECEIVE) {
		ret = msg_receive(p, src_dest, m);
		if (ret != 0)
			return ret;
	}
	else {
		panic("{sys_sendrec} invalid function: "
		      "%d (SEND:%d, RECEIVE:%d).", function, SEND, RECEIVE);
	}

	return 0;
}

/*****************************************************************************
 *				  ldt_seg_linear
 *****************************************************************************/
/**
 * <Ring 0~1> Calculate the linear address of a certain segment of a given
 * proc.
 * 
 * @param p   Whose (the proc ptr).
 * @param idx Which (one proc has more than one segments).
 * 
 * @return  The required linear address.
 *****************************************************************************/
PUBLIC int ldt_seg_linear(struct proc* p, int idx)
{
	struct descriptor * d = &p->ldts[idx];

	return d->base_high << 24 | d->base_mid << 16 | d->base_low;
}

/*****************************************************************************
 *				  va2la
 *****************************************************************************/
/**
 * <Ring 0~1> Virtual addr --> Linear addr.
 * 
 * @param pid  PID of the proc whose address is to be calculated.
 * @param va   Virtual address.
 * 
 * @return The linear address for the given virtual address.
 *****************************************************************************/
PUBLIC void* va2la(int pid, void* va)
{
	struct proc* p = &proc_table[pid];

	u32 seg_base = ldt_seg_linear(p, INDEX_LDT_RW);
	u32 la = seg_base + (u32)va;

	if (pid < NR_TASKS + NR_NATIVE_PROCS) {
		assert(la == (u32)va);
	}

	return (void*)la;
}

/*****************************************************************************
 *                                reset_msg
 *****************************************************************************/
/**
 * <Ring 0~3> Clear up a MESSAGE by setting each byte to 0.
 * 
 * @param p  The message to be cleared.
 *****************************************************************************/
PUBLIC void reset_msg(MESSAGE* p)
{
	memset(p, 0, sizeof(MESSAGE));
}

/*****************************************************************************
 *                                block
 *****************************************************************************/
/**
 * <Ring 0> This routine is called after `state' has been set (!= 0), it
 * calls `schedule()' to choose another proc as the `proc_ready'.
 *
 * @attention This routine does not change `state'. Make sure the `state'
 * of the proc to be blocked has been set properly.
 * 
 * @param p The proc to be blocked.
 *****************************************************************************/
PRIVATE void block(struct proc* p)
{
	assert(p->state);
	schedule();
}

/*****************************************************************************
 *                                unblock
 *****************************************************************************/
/**
 * <Ring 0> This is a dummy routine. It does nothing actually. When it is
 * called, the `state' should have been cleared (== 0).
 * 
 * @param p The unblocked proc.
 *****************************************************************************/
PRIVATE void unblock(struct proc* p)
{
	assert(p->state == 0);
}

/*****************************************************************************
 *                                deadlock
 *****************************************************************************/
/**
 * <Ring 0> Check whether it is safe to send a message from src to dest.
 * The routine will detect if the messaging graph contains a cycle. For
 * instance, if we have procs trying to send messages like this:
 * A -> B -> C -> A, then a deadlock occurs, because all of them will
 * wait forever. If no cycles detected, it is considered as safe.
 * 
 * @param src   Who wants to send message.
 * @param dest  To whom the message is sent.
 * 
 * @return Zero if success.
 *****************************************************************************/
PRIVATE int deadlock(int src, int dest)
{
	struct proc* p = proc_table + dest;
	while (1) {
		if (p->state & SENDING) {
			if (p->sendto == src) {
				/* print the chain */
				p = proc_table + dest;
				printl("=_=%s", p->name);
				do {
					assert(p->msg);
					p = proc_table + p->sendto;
					printl("->%s", p->name);
				} while (p != proc_table + src);
				printl("=_=");

				return 1;
			}
			p = proc_table + p->sendto;
		}
		else {
			break;
		}
	}
	return 0;
}

/*****************************************************************************
 *                                msg_send
 *****************************************************************************/
/**
 * <Ring 0> Send a message to the dest proc. If dest is blocked waiting for
 * the message, copy the message to it and unblock dest. Otherwise the caller
 * will be blocked and appended to the dest's sending queue.
 * 
 * @param current  The caller, the sender.
 * @param dest     To whom the message is sent.
 * @param m        The message.
 * 
 * @return Zero if success.
 *****************************************************************************/
PRIVATE int msg_send(struct proc* current, int dest, MESSAGE* m)
{
	struct proc* sender = current;
	struct proc* p_dest = proc_table + dest; /* proc dest */

	assert(proc2pid(sender) != dest);

	/* check for deadlock here */
	if (deadlock(proc2pid(sender), dest)) {
		panic(">>DEADLOCK<< %s->%s", sender->name, p_dest->name);
	}

	if ((p_dest->state & RECEIVING) && /* p_dest is waiting for the msg */
	    (p_dest->recvfrom == proc2pid(sender) ||
	     p_dest->recvfrom == ANY)) {
		assert(p_dest->msg);
		assert(m);

		phys_copy(va2la(dest, p_dest->msg),
			  va2la(proc2pid(sender), m),
			  sizeof(MESSAGE));
		p_dest->msg = 0;
		p_dest->state &= ~RECEIVING; /* p_dest has received the msg */
		p_dest->recvfrom = NO_TASK;
		unblock(p_dest);

		assert(p_dest->state == 0);
		assert(p_dest->msg == 0);
		assert(p_dest->recvfrom == NO_TASK);
		assert(p_dest->sendto == NO_TASK);
		assert(sender->state == 0);
		assert(sender->msg == 0);
		assert(sender->recvfrom == NO_TASK);
		assert(sender->sendto == NO_TASK);
	}
	else { /* p_dest is not waiting for the msg */
		sender->state |= SENDING;
		assert(sender->state == SENDING);
		sender->sendto = dest;
		sender->msg = m;

		/* append to the sending queue */
		struct proc * p;
		if (p_dest->q_sending) {
			p = p_dest->q_sending;
			while (p->next_sending)
				p = p->next_sending;
			p->next_sending = sender;
		}
		else {
			p_dest->q_sending = sender;
		}
		sender->next_sending = 0;

		block(sender);

		assert(sender->state == SENDING);
		assert(sender->msg != 0);
		assert(sender->recvfrom == NO_TASK);
		assert(sender->sendto == dest);
	}

	return 0;
}


/*****************************************************************************
 *                                msg_receive
 *****************************************************************************/
/**
 * <Ring 0> Try to get a message from the src proc. If src is blocked sending
 * the message, copy the message from it and unblock src. Otherwise the caller
 * will be blocked.
 * 
 * @param current The caller, the proc who wanna receive.
 * @param src     From whom the message will be received.
 * @param m       The message ptr to accept the message.
 * 
 * @return  Zero if success.
 *****************************************************************************/
PRIVATE int msg_receive(struct proc* current, int src, MESSAGE* m)
{
	struct proc* who_wanna_recv = current; /**
						  * This name is a little bit
						  * wierd, but it makes me
						  * think clearly, so I keep
						  * it.
						  */
	struct proc* from = 0; /* from which the message will be fetched */
	struct proc* prev = 0;
	int copyok = 0;

	assert(proc2pid(who_wanna_recv) != src);

	if ((who_wanna_recv->has_int_msg) &&
	    ((src == ANY) || (src == INTERRUPT))) {
		/* There is an interrupt needs who_wanna_recv's handling and
		 * who_wanna_recv is ready to handle it.
		 */

		MESSAGE msg;
		reset_msg(&msg);
		msg.source = INTERRUPT;
		msg.type = HARD_INT;
		assert(m);
		phys_copy(va2la(proc2pid(who_wanna_recv), m), &msg,
			  sizeof(MESSAGE));

		who_wanna_recv->has_int_msg = 0;

		assert(who_wanna_recv->state == 0);
		assert(who_wanna_recv->msg == 0);
		assert(who_wanna_recv->sendto == NO_TASK);
		assert(who_wanna_recv->has_int_msg == 0);

		return 0;
	}


	/* Arrives here if no interrupt for who_wanna_recv. */
	if (src == ANY) {
		/* who_wanna_recv is ready to receive messages from
		 * ANY proc, we'll check the sending queue and pick the
		 * first proc in it.
		 */
		if (who_wanna_recv->q_sending) {
			from = who_wanna_recv->q_sending;
			copyok = 1;

			assert(who_wanna_recv->state == 0);
			assert(who_wanna_recv->msg == 0);
			assert(who_wanna_recv->recvfrom == NO_TASK);
			assert(who_wanna_recv->sendto == NO_TASK);
			assert(who_wanna_recv->q_sending != 0);
			assert(from->state == SENDING);
			assert(from->msg != 0);
			assert(from->recvfrom == NO_TASK);
			assert(from->sendto == proc2pid(who_wanna_recv));
		}
	}
	else {
		/* who_wanna_recv wants to receive a message from
		 * a certain proc: src.
		 */
		from = &proc_table[src];

		if ((from->state & SENDING) &&
		    (from->sendto == proc2pid(who_wanna_recv))) {
			/* Perfect, src is sending a message to
			 * who_wanna_recv.
			 */
			copyok = 1;

			struct proc* p = who_wanna_recv->q_sending;
			assert(p); /* from must have been appended to the
				    * queue, so the queue must not be NULL
				    */
			while (p) {
				assert(from->state & SENDING);
				if (proc2pid(p) == src) { /* if p is the one */
					from = p;
					break;
				}
				prev = p;
				p = p->next_sending;
			}

			assert(who_wanna_recv->state == 0);
			assert(who_wanna_recv->msg == 0);
			assert(who_wanna_recv->recvfrom == NO_TASK);
			assert(who_wanna_recv->sendto == NO_TASK);
			assert(who_wanna_recv->q_sending != 0);
			assert(from->state == SENDING);
			assert(from->msg != 0);
			assert(from->recvfrom == NO_TASK);
			assert(from->sendto == proc2pid(who_wanna_recv));
		}
	}

	if (copyok) {
		/* It's determined from which proc the message will
		 * be copied. Note that this proc must have been
		 * waiting for this moment in the queue, so we should
		 * remove it from the queue.
		 */
		if (from == who_wanna_recv->q_sending) { /* the 1st one */
			assert(prev == 0);
			who_wanna_recv->q_sending = from->next_sending;
			from->next_sending = 0;
		}
		else {
			assert(prev);
			prev->next_sending = from->next_sending;
			from->next_sending = 0;
		}

		assert(m);
		assert(from->msg);
		/* copy the message */
		phys_copy(va2la(proc2pid(who_wanna_recv), m),
			  va2la(proc2pid(from), from->msg),
			  sizeof(MESSAGE));

		from->msg = 0;
		from->sendto = NO_TASK;
		from->state &= ~SENDING;
		unblock(from);
	}
	else {  /* nobody's sending any msg */
		/* Set state so that who_wanna_recv will not
		 * be scheduled until it is unblocked.
		 */
		who_wanna_recv->state |= RECEIVING;

		who_wanna_recv->msg = m;

		if (src == ANY)
			who_wanna_recv->recvfrom = ANY;
		else
			who_wanna_recv->recvfrom = proc2pid(from);

		block(who_wanna_recv);

		assert(who_wanna_recv->state == RECEIVING);
		assert(who_wanna_recv->msg != 0);
		assert(who_wanna_recv->recvfrom != NO_TASK);
		assert(who_wanna_recv->sendto == NO_TASK);
		assert(who_wanna_recv->has_int_msg == 0);
	}

	return 0;
}

/*****************************************************************************
 *                                inform_int
 *****************************************************************************/
/**
 * <Ring 0> Inform a proc that an interrupt has occured.
 * 
 * @param task_nr  The task which will be informed.
 *****************************************************************************/
PUBLIC void inform_int(int task_nr)
{
	struct proc* p = proc_table + task_nr;



	if ((p->state & RECEIVING) && /* dest is waiting for the msg */
	    ((p->recvfrom == INTERRUPT) || (p->recvfrom == ANY))) {
		p->msg->source = INTERRUPT;
		p->msg->type = HARD_INT;
		p->msg = 0;
		p->has_int_msg = 0;
		p->state &= ~RECEIVING; /* dest has received the msg */
		p->recvfrom = NO_TASK;
		assert(p->state == 0);
		unblock(p);

		assert(p->state == 0);
		assert(p->msg == 0);
		assert(p->recvfrom == NO_TASK);
		assert(p->sendto == NO_TASK);
	}
	else {
		p->has_int_msg = 1;
	}
}

/*****************************************************************************
 *                                dumproc
 *****************************************************************************/
PUBLIC void dumproc(struct proc* p)
{
	char info[STR_DEFAULT_LEN];
	int i;
	int text_color = MAKE_COLOR(GREEN, RED);

	int dumlen = sizeof(struct proc);

	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, 0);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, 0);

	sprintf(info, "byte dump of proc_table[%d]:\n", p - proc_table); disp_color_str(info, text_color);
	for (i = 0; i < dumlen; i++) {
		sprintf(info, "%x.", ((unsigned char *)p)[i]);
		disp_color_str(info, text_color);
	}

	/* printl("^^"); */

	disp_color_str("\n\n", text_color);
	sprintf(info, "ANY: 0x%x.\n", ANY); disp_color_str(info, text_color);
	sprintf(info, "NO_TASK: 0x%x.\n", NO_TASK); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);

	sprintf(info, "ldt_sel: 0x%x.  ", p->ldt_sel); disp_color_str(info, text_color);
	sprintf(info, "counter: 0x%x.  ", p->counter); disp_color_str(info, text_color);
	sprintf(info, "priority: 0x%x.  ", p->priority); disp_color_str(info, text_color); 

	sprintf(info, "name: %s.  ", p->name); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);
	sprintf(info, "state: 0x%x.  ", p->state); disp_color_str(info, text_color);
	sprintf(info, "recvfrom: 0x%x.  ", p->recvfrom); disp_color_str(info, text_color);
	sprintf(info, "sendto: 0x%x.  ", p->sendto); disp_color_str(info, text_color);

	disp_color_str("\n", text_color);
	sprintf(info, "has_int_msg: 0x%x.  ", p->has_int_msg); disp_color_str(info, text_color);
}


/*****************************************************************************
 *                                dummsg
 *****************************************************************************/
PUBLIC void dump_msg(const char * title, MESSAGE* m)
{
	int packed = 0;
	printl("{%s}<0x%x>{%ssrc:%s(%d),%stype:%d,%s(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)%s}%s",  //, (0x%x, 0x%x, 0x%x)}",
	       title,
	       (int)m,
	       packed ? "" : "\n        ",
	       proc_table[m->source].name,
	       m->source,
	       packed ? " " : "\n        ",
	       m->type,
	       packed ? " " : "\n        ",
	       m->u.m3.m3i1,
	       m->u.m3.m3i2,
	       m->u.m3.m3i3,
	       m->u.m3.m3i4,
	       (int)m->u.m3.m3p1,
	       (int)m->u.m3.m3p2,
	       packed ? "" : "\n",
	       packed ? "" : "\n"/* , */
		);
}

