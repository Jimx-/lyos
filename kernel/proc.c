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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "page.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"

PRIVATE struct proc * pick_proc();
PRIVATE void idle();
PUBLIC int  msg_send(struct proc* p_to_send, int des, MESSAGE* m);
PRIVATE int  msg_receive(struct proc* p_to_recv, int src, MESSAGE* m);
PRIVATE int  deadlock(int src, int dest);
PRIVATE void proc_no_time(struct proc * p);

/*****************************************************************************
 *                                pick_proc
 *****************************************************************************/
/**
 * <Ring 0> Choose one proc to run.
 * 
 *****************************************************************************/
PRIVATE struct proc * pick_proc()
{
  	register struct proc *p;
  	struct proc ** rq_head;
  	int q;

  	rq_head = get_cpulocal_var(run_queue_head);
  	for (q = 0; q < NR_SCHED_QUEUES; q++) {	
		if(!(p = rq_head[q])) {
			continue;
		}
		return p;
  	}
  	
  	return NULL;
}

PUBLIC void init_proc()
{
	int i;
	struct proc * p = proc_table;

	for (i = 0; i < NR_TASKS + NR_PROCS; i++,p++) {
		if (i < NR_TASKS + NR_NATIVE_PROCS) {
			PST_SET(p, PST_STOPPED);
		}
	}

	/* prepare idle process struct */
	
	for (i = 0; i < CONFIG_SMP_MAX_CPUS; i++) {
		struct proc * p = get_cpu_var_ptr(i, idle_proc);
		p->state |= PST_STOPPED;
		sprintf(p->name, "idle%d", i);
	}
}

/**
 * <Ring 0> Switch back to user space.
 */
PUBLIC void switch_to_user()
{
	struct proc * p = get_cpulocal_var(proc_ptr);

	if (proc_is_runnable(p)) goto no_schedule;

reschedule:
	while (!(p = pick_proc())) idle();

no_schedule:

	get_cpulocal_var(proc_ptr) = p;

	if (p->counter <= 0) proc_no_time(p);
	if (!proc_is_runnable(p)) goto reschedule;

	switch_address_space(p->pgd.phys_addr);

	p = arch_switch_to_user();

	restore_user_context(p);
}

PRIVATE void switch_address_space_idle()
{
#if CONFIG_SMP
	switch_address_space(proc_table[TASK_MM].pgd.phys_addr);
#endif
}

/**
 * <Ring 0> Called when there is no work to do. Halt the CPU.
 */
PRIVATE void idle()
{
	get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
	switch_address_space_idle();

#if CONFIG_SMP
	get_cpulocal_var(cpu_is_idle) = 1;
#endif

	halt_cpu();
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
PUBLIC int sys_sendrec(MESSAGE* m, struct proc* p)
{
	int function = m->SR_FUNCTION;
	int src_dest = m->SR_SRCDEST;
	MESSAGE * msg = (MESSAGE *)m->SR_MSG;

	assert((src_dest >= 0 && src_dest < NR_TASKS + NR_PROCS) ||
	       src_dest == ANY ||
	       src_dest == INTERRUPT);

	int ret = 0;
	int caller = proc2pid(p);
	MESSAGE* mla = (MESSAGE*)va2la(caller, msg);
	mla->source = caller;

	assert(mla->source != src_dest);

	switch (function) {
		case BOTH:
			/* fall through */
		case SEND:
			ret = msg_send(p, src_dest, msg);
			if (ret != 0 || function == SEND) break;
			/* fall through for BOTH */
		case RECEIVE:
			ret = msg_receive(p, src_dest, msg);
			break;
		default:
			panic("{sys_sendrec} invalid function: "
		      "%d (SEND:%d, RECEIVE:%d, BOTH: %d).", function, SEND, RECEIVE, BOTH);
			break;
	}

	return ret;
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
	return va;
}

/*****************************************************************************
 *				  la2pa
 *****************************************************************************/
/**
 * <Ring 0~1> Linear addr --> physical addr.
 * 
 * @param pid  PID of the proc whose address is to be calculated.
 * @param la   Linear address.
 * 
 * @return The physical address for the given linear address.
 *****************************************************************************/
PUBLIC void * la2pa(int pid, void * la)
{
	struct proc* p = &proc_table[pid];

    unsigned long pgd_index = ARCH_PDE(la);
    unsigned long pt_index = ARCH_PTE(la);

    pte_t * pt = p->pgd.vir_pts[pgd_index];
    return (void*)((pt[pt_index] & 0xFFFFF000) + ((int)la & 0xFFF));
}

/*****************************************************************************
 *				  va2pa
 *****************************************************************************/
/**
 * <Ring 0~1> Virtual addr --> physical addr.
 * 
 * @param pid  PID of the proc whose address is to be calculated.
 * @param va   Virtual address.
 * 
 * @return The physical address for the given virtual address.
 *****************************************************************************/
PUBLIC void * va2pa(int pid, void * va)
{
	return la2pa(pid, va2la(pid, va));
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
		if (p->state & PST_SENDING) {
			if (p->sendto == src) {
				p = proc_table + dest;
				do {
					p = proc_table + p->sendto;
				} while (p != proc_table + src);
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
 * @param p_to_send  The caller, the sender.
 * @param dest     To whom the message is sent.
 * @param m        The message.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int msg_send(struct proc* p_to_send, int dest, MESSAGE* m)
{
	struct proc* sender = p_to_send;
	struct proc* p_dest = proc_table + dest; /* proc dest */

	/* check for deadlock here */
	if (deadlock(proc2pid(sender), dest)) {
		dump_msg("deadlock sender", m);
		dump_msg("deadlock receiver", p_dest->send_msg);
		//panic("deadlock: %s(pid: %d)->%s(pid: %d)", sender->name, proc2pid(sender), p_dest->name, proc2pid(p_dest));
		return EDEADLK;
	}

	if ((p_dest->state & PST_RECEIVING) && /* p_dest is waiting for the msg */
	    (p_dest->recvfrom == proc2pid(sender) ||
	     p_dest->recvfrom == ANY)) {

		vir_copy(dest, D, p_dest->recv_msg, proc2pid(sender), D, m, sizeof(MESSAGE));
		/*phys_copy(va2la(dest, p_dest->msg),
			  va2la(proc2pid(sender), m),
			  sizeof(MESSAGE)); */
		p_dest->recv_msg = 0;
		PST_UNSET(p_dest, PST_RECEIVING);
		p_dest->recvfrom = NO_TASK;
	}
	else { /* p_dest is not waiting for the msg */
		PST_SET(sender, PST_SENDING);
		sender->sendto = dest;
		sender->send_msg = m;

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
 * @param p_to_recv The caller, the proc who wanna receive.
 * @param src     From whom the message will be received.
 * @param m       The message ptr to accept the message.
 * 
 * @return  Zero if success.
 *****************************************************************************/
PRIVATE int msg_receive(struct proc* p_to_recv, int src, MESSAGE* m)
{
	struct proc* who_wanna_recv = p_to_recv; /**
						  * This name is a little bit
						  * wierd, but it makes me
						  * think clearly, so I keep
						  * it.
						  */
	struct proc* from = 0; /* from which the message will be fetched */
	struct proc* prev = 0;
	int copyok = 0;

	/* PST_SENDING is set means that the process failed to send a 
	 * message in sendrec(BOTH), simply block it.
	 */
	if (PST_IS_SET(who_wanna_recv, PST_SENDING)) goto no_msg;

	if ((who_wanna_recv->special_msg) &&
	    ((src == ANY) || (src == INTERRUPT) || (src == KERNEL))) {
		/* There is an interrupt needs who_wanna_recv's handling and
		 * who_wanna_recv is ready to handle it.
		 */

		MESSAGE msg;
		reset_msg(&msg);

		if ((who_wanna_recv->special_msg & MSG_INTERRUPT) && (src != KERNEL)) {
			msg.source = INTERRUPT;
			msg.type = HARD_INT;
			who_wanna_recv->special_msg &= ~MSG_INTERRUPT;
		} else if ((who_wanna_recv->special_msg & MSG_KERNLOG) && (src != INTERRUPT)) {
			msg.source = KERNEL;
			msg.type = KERN_LOG;
			who_wanna_recv->special_msg &= ~MSG_KERNLOG;
		} else {
			goto normal_msg;
		}

		assert(m);
		vir_copy(proc2pid(who_wanna_recv), D, m, proc2pid(p_to_recv), D, &msg, sizeof(MESSAGE));

		return 0;
	}

normal_msg:
	/* Arrives here if no interrupt for who_wanna_recv. */
	if (src == ANY) {
		/* who_wanna_recv is ready to receive messages from
		 * ANY proc, we'll check the sending queue and pick the
		 * first proc in it.
		 */
		if (who_wanna_recv->q_sending) {
			from = who_wanna_recv->q_sending;
			copyok = 1;
		}
	}
	else {
		/* who_wanna_recv wants to receive a message from
		 * a certain proc: src.
		 */
		from = &proc_table[src];

		if ((PST_IS_SET(from, PST_SENDING)) &&
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
				assert(from->state & PST_SENDING);
				if (proc2pid(p) == src) { /* if p is the one */
					from = p;
					break;
				}
				prev = p;
				p = p->next_sending;
			}
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
		/* copy the message */
		vir_copy(proc2pid(who_wanna_recv), D, m, proc2pid(from), D, from->send_msg, sizeof(MESSAGE));
		/*phys_copy(va2la(proc2pid(who_wanna_recv), m),
			  va2la(proc2pid(from), from->msg),
			  sizeof(MESSAGE)); */

		from->send_msg = 0;
		from->sendto = NO_TASK;
		PST_UNSET(from, PST_SENDING);

		return 0;
	}

no_msg:
	/* nobody's sending any msg */
	/* Set state so that who_wanna_recv will not
	* be scheduled until it is unblocked.
	*/
	PST_SET(who_wanna_recv, PST_RECEIVING);

	who_wanna_recv->recv_msg = m;
	who_wanna_recv->recvfrom = src;

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

	if ((p->state & PST_RECEIVING) && /* dest is waiting for the msg */
	    ((p->recvfrom == INTERRUPT) || (p->recvfrom == ANY))) {
		p->recv_msg->source = INTERRUPT;
		p->recv_msg->type = HARD_INT;
		p->recv_msg = 0;
		p->special_msg &= ~MSG_INTERRUPT;
		PST_UNSET(p, PST_RECEIVING);
		p->recvfrom = NO_TASK;
	}
	else {
		p->special_msg |= MSG_INTERRUPT;
	}
}

/*****************************************************************************
 *                                inform_int
 *****************************************************************************/
/**
 * <Ring 0> Inform a proc that kernel wants to log.
 * 
 * @param task_nr  The task which will be informed.
 *****************************************************************************/
PUBLIC void inform_kernel_log(int task_nr)
{
	struct proc* p = proc_table + task_nr;

	if ((p->state & PST_RECEIVING) && /* dest is waiting for the msg */
	    ((p->recvfrom == KERNEL) || (p->recvfrom == ANY))) {
		p->recv_msg->source = KERNEL;
		p->recv_msg->type = KERN_LOG;
		p->recv_msg = 0;
		p->special_msg &= ~MSG_KERNLOG;
		PST_UNSET(p, PST_RECEIVING);
		p->recvfrom = NO_TASK;
	}
	else {
		p->special_msg |= MSG_KERNLOG;
	}
}

/*****************************************************************************
 *                                dumproc
 *****************************************************************************/
PUBLIC void dumproc(struct proc* p)
{
#if 0
	sprintf(info, "counter: 0x%x.  ", p->counter); disp_color_str(info, text_color);
	sprintf(info, "priority: 0x%x.  ", p->priority); disp_color_str(info, text_color); 

	sprintf(info, "name: %s.  ", p->name); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);
	sprintf(info, "state: 0x%x.  ", p->state); disp_color_str(info, text_color);
	sprintf(info, "recvfrom: 0x%x.  ", p->recvfrom); disp_color_str(info, text_color);
	sprintf(info, "sendto: 0x%x.  ", p->sendto); disp_color_str(info, text_color);

	disp_color_str("\n", text_color);
	sprintf(info, "special_msg: 0x%x.  ", p->special_msg); disp_color_str(info, text_color);
#endif
}


/*****************************************************************************
 *                                dummsg
 *****************************************************************************/
PUBLIC void dump_msg(const char * title, MESSAGE* m)
{
	int packed = 0;
	printk("\n\n%s<0x%x>{%ssrc:%s(%d),%stype:%d,%sm->u.m3:{0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}%s}%s",  //, (0x%x, 0x%x, 0x%x)}",
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

/**
 * <Ring 0> Insert a process into scheduling queue.
 */
PUBLIC void enqueue_proc(register struct proc * p)
{
	int prio = p->priority;
	struct proc ** rq_head, ** rq_tail;

	rq_head = get_cpu_var(p->cpu, run_queue_head);
  	rq_tail = get_cpu_var(p->cpu, run_queue_tail);

  	if (!rq_head[prio]) {
  		rq_head[prio] = rq_tail[prio] = p;
  		p->next_ready = NULL;
  	} else {
  		rq_tail[prio]->next_ready = p;
  		rq_tail[prio] = p;
  		p->next_ready = NULL;
  	}
}

/**
 * <Ring 0> Remove a process from scheduling queue.
 */
PUBLIC void dequeue_proc(register struct proc * p)
{
	int q = p->priority;	
  	struct proc ** xpp;
  	struct proc * prev_xp;

  	struct proc ** rq_tail;

  	rq_tail = get_cpu_var(p->cpu, run_queue_tail);

  	prev_xp = NULL;				
  	for (xpp = get_cpu_var_ptr(p->cpu, run_queue_head[q]); *xpp;
		  	xpp = &(*xpp)->next_ready) {
      	if (*xpp == p) {
          	*xpp = (*xpp)->next_ready;
          	if (p == rq_tail[q]) {
              	rq_tail[q] = prev_xp;
	  		}

        	break;
    	}
      	prev_xp = *xpp;
  	}
}

/**
 * <Ring 0> Called when a process has run out its counter.
 */
PRIVATE void proc_no_time(struct proc * p)
{
	p->counter = p->quantum_ms;
}
