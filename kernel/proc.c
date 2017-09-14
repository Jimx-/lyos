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
#include "string.h"
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
#include <lyos/time.h>
#include <lyos/ipc.h>
    
PUBLIC struct proc * pick_proc();
PUBLIC void proc_no_time(struct proc * p);

PRIVATE void idle();
PRIVATE int  msg_receive(struct proc* p_to_recv, int src, MESSAGE* m, int flags);
PRIVATE int receive_async(struct proc* p);
PRIVATE int receive_async_from(struct proc* p, struct proc* sender);
PRIVATE int  has_pending_notify(struct proc * p, endpoint_t src);
PRIVATE int  has_pending_async(struct proc * p, endpoint_t src);
PRIVATE void unset_notify_pending(struct proc * p, int id);
PRIVATE void set_notify_msg(struct proc * sender, MESSAGE * m, endpoint_t src);
PRIVATE int  deadlock(int src, int dest);

PUBLIC void init_proc()
{
	int i;
	struct proc * p = proc_table;
	struct priv * priv;
	int prio, quantum;

	for (i = -NR_TASKS; i < NR_PROCS; i++,p++) {
		spinlock_init(&p->lock);
		INIT_LIST_HEAD(&p->run_list);
		
		if (i >= (NR_BOOT_PROCS - NR_TASKS)) {
			p->state = PST_FREE_SLOT;
			continue;
		}

		if (i == INIT) {
			prio    = USER_PRIO;
			quantum = USER_QUANTUM;
		} else {
			prio    = TASK_PRIO;
			quantum = TASK_QUANTUM;
		}

		p->quantum_ms = quantum;
		p->counter_ns = p->quantum_ms * NSEC_PER_MSEC;
		p->priority = prio;

		p->p_parent = NO_TASK;
		p->endpoint = make_endpoint(0, i);	/* generation 0 */

		reset_msg(&p->send_msg);
		p->recv_msg = 0;
		p->recvfrom = NO_TASK;
		p->sendto = NO_TASK;
		p->q_sending = 0;
		p->next_sending = 0;

		p->state = 0;
		p->flags = 0;

		if (i != TASK_MM) {
			PST_SET(p, PST_BOOTINHIBIT);
			PST_SET(p, PST_MMINHIBIT);		/* process is not ready until MM allows it to run */
		}

		PST_SET(p, PST_STOPPED);

		p->sched_policy = SCHED_NORMAL;
		proc_no_time(p);

		arch_reset_proc(p);
	}

	int id = 0;
	for (priv = &FIRST_PRIV; priv < &LAST_PRIV; priv++) {
		memset(priv, 0, sizeof(struct priv));
		priv->proc_nr = NO_TASK;
		priv->id = id++;
		priv->timer.expire_time = TIMER_UNSET;

		sigemptyset(&priv->sig_pending);
		priv->notify_pending = 0;
		priv->async_pending = 0;
	}

	/* prepare idle process struct */
	
	for (i = 0; i < CONFIG_SMP_MAX_CPUS; i++) {
		struct proc * p = get_cpu_var_ptr(i, idle_proc);
		p->state |= PST_STOPPED;
		sprintf(p->name, "idle%d", i);
	}

	mmrequest = NULL;
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

	switch_address_space(p);

no_schedule:

	get_cpulocal_var(proc_ptr) = p;

	if (p->flags & PF_RESUME_SYSCALL) {
		resume_sys_call(p);

		/* actually leave the syscall */
		if (p->flags & PF_TRACE_SYSCALL) p->flags |= PF_LEAVE_SYSCALL;
	}

	/* syscall leave stop */
	if ((p->flags & PF_LEAVE_SYSCALL) && (p->flags & PF_TRACE_SYSCALL) &&
			proc_is_runnable(p)) {
		p->flags &= ~(PF_TRACE_SYSCALL | PF_LEAVE_SYSCALL);

		ksig_proc(p->endpoint, SIGTRAP);
	}

	if (!proc_is_runnable(p)) goto reschedule;
	if (p->counter_ns <= 0) {
		PST_SET(p, PST_NO_QUANTUM);
		proc_no_time(p);
		goto reschedule;
	}

	p = arch_switch_to_user();

#if CONFIG_SMP
	p->last_cpu = p->cpu;
	p->cpu = cpuid;
#endif

	stop_context(proc_addr(KERNEL));

	restart_local_timer();
	restore_user_context(p);
}

PRIVATE void switch_address_space_idle()
{
#if CONFIG_SMP
	switch_address_space(proc_addr(TASK_MM));
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

	//restart_local_timer();
	if (cpuid == bsp_cpu_id)
		restart_local_timer();
	else
		stop_local_timer();
	
#else
	restart_local_timer();
#endif

	stop_context(proc_addr(KERNEL));
	
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
	async_message_t* async_msg = (async_message_t*) m->SR_TABLE;
	size_t msg_len = m->SR_LEN;

	int ret = 0;
	int flags = 0;
	int caller = p->endpoint;
	MESSAGE * mla = (MESSAGE * )msg;
	
	if (function != SEND_ASYNC) {
		mla->source = caller;
		if (!verify_endpt(src_dest, NULL)) return EINVAL;

		if (mla->source == src_dest) return EINVAL;
	}

	switch (function) {
		case BOTH:
			/* fall through */
		case SEND:
			ret = msg_send(p, src_dest, msg, flags);
			if (ret != 0 || function == SEND) break;
			/* fall through for BOTH */
		case RECEIVE:
			ret = msg_receive(p, src_dest, msg, flags);
			break;
		case SEND_NONBLOCK:
			ret = msg_send(p, src_dest, msg, flags | IPCF_NONBLOCK);
			break;
		case SEND_ASYNC:
			ret = msg_senda(p, async_msg, msg_len);
			break;
        case RECEIVE_ASYNC:
			ret = msg_receive(p, src_dest, msg, flags | IPCF_ASYNC);
			break;
		default:
			ret = EINVAL;
			break;
	}

	return ret;
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

PUBLIC struct proc * endpt_proc(endpoint_t ep)
{
	int n;
	if (!verify_endpt(ep, &n)) return NULL;

	return proc_addr(n);
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
PRIVATE int deadlock(endpoint_t src, endpoint_t dest)
{
	struct proc * p = endpt_proc(dest);

	while (TRUE) {
		if (p->state & PST_SENDING) {
			if (p->sendto == src) {
				p = endpt_proc(dest);
                printk("kernel: deadlock detected, chain: %d ", dest);
				do {
					p = endpt_proc(p->sendto);
                    printk("-> %d ", p->endpoint);
				} while (p != endpt_proc(src));
				return 1;
			}
			p = endpt_proc(p->sendto);
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
PUBLIC int msg_send(struct proc* p_to_send, int dest, MESSAGE* m, int flags)
{
	struct proc * sender = p_to_send;
	struct proc * p_dest = endpt_proc(dest); /* proc dest */
	if (p_dest == NULL) return EINVAL;
	int retval = 0;

	lock_proc(sender);
	lock_proc(p_dest);

	/* check for deadlock here */
	if (deadlock(sender->endpoint, dest)) {
		dump_msg("deadlock sender", m);
		dump_msg("deadlock receiver", &p_dest->send_msg);
		retval = EDEADLK;
		goto out;
	}

	if (!PST_IS_SET(p_dest, PST_SENDING) && PST_IS_SET(p_dest, PST_RECEIVING) && /* p_dest is waiting for the msg */
	    (p_dest->recvfrom == sender->endpoint ||
	     p_dest->recvfrom == ANY)) {

		retval = data_vir_copy_check(p_to_send, dest, p_dest->recv_msg, sender->endpoint, m, sizeof(MESSAGE));
		if (retval != 0) goto out;

		p_dest->recv_msg = 0;
		PST_UNSET_LOCKED(p_dest, PST_RECEIVING);
		p_dest->recvfrom = NO_TASK;
	} else { /* p_dest is not waiting for the msg */
		if (flags & IPCF_NONBLOCK) {
			retval = EBUSY;
			goto out;
		}
		
		PST_SET_LOCKED(sender, PST_SENDING);
		sender->sendto = dest;
		retval = data_vir_copy_check(p_to_send, KERNEL, &sender->send_msg, KERNEL, m, sizeof(MESSAGE));
		if (retval != 0) goto out;

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

out:
	unlock_proc(sender);
	unlock_proc(p_dest);

	return retval;
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
PRIVATE int msg_receive(struct proc* p_to_recv, int src, MESSAGE* m, int flags)
{
	struct proc* who_wanna_recv = p_to_recv; /**
						  * This name is a little bit
						  * wierd, but it makes me
						  * think clearly, so I keep
						  * it.
						  */
	struct proc* from = NULL; /* from which the message will be fetched */
	struct proc* prev = NULL;
	int copyok = 0;
	int retval = 0;

	lock_proc(who_wanna_recv);
	/* PST_SENDING is set means that the process failed to send a 
	 * message in sendrec(BOTH), simply block it.
	 */
	if (PST_IS_SET(who_wanna_recv, PST_SENDING)) goto no_msg;

	/* check pending notifications */
	int notify_id;
	if ((notify_id = has_pending_notify(who_wanna_recv, src)) != PRIV_ID_NULL) {
		MESSAGE msg;
		reset_msg(&msg);

		int proc_nr = priv_addr(notify_id)->proc_nr;
		struct proc * notifier = proc_addr(proc_nr);
		set_notify_msg(who_wanna_recv, &msg, notifier->endpoint);

		unset_notify_pending(who_wanna_recv, notify_id);
		retval = data_vir_copy_check(who_wanna_recv, KERNEL, m, KERNEL, &msg, sizeof(MESSAGE));
		if (retval != 0) goto out;

		who_wanna_recv->recv_msg = NULL;
		PST_UNSET_LOCKED(who_wanna_recv, PST_RECEIVING);
		who_wanna_recv->recvfrom = NO_TASK;

		goto out;
	}

	/* check for async messages */
	int async_id;
    if (flags & IPCF_ASYNC) {
        if ((async_id = has_pending_async(who_wanna_recv, src)) != PRIV_ID_NULL) {
            int proc_nr = priv_addr(async_id)->proc_nr;
            struct proc * async_sender = proc_addr(proc_nr);

            who_wanna_recv->recv_msg = m;

            if (src == ANY) retval = receive_async(who_wanna_recv);
            else retval = receive_async_from(who_wanna_recv, async_sender);

            if (retval == 0) goto out;
            else who_wanna_recv->recv_msg = NULL;
        }
    }

	/* Arrives here if no interrupt for who_wanna_recv. */
	if (src == ANY) {
		/* who_wanna_recv is ready to receive messages from
		 * ANY proc, we'll check the sending queue and pick the
		 * first proc in it.
		 */
		if (who_wanna_recv->q_sending) {
			from = who_wanna_recv->q_sending;
			lock_proc(from);
			copyok = 1;
		}
	}
	else {
		/* who_wanna_recv wants to receive a message from
		 * a certain proc: src.
		 */
		from = endpt_proc(src);
		if (from == NULL) {
			retval = EINVAL;
			goto out;
		}

		if ((PST_IS_SET(from, PST_SENDING)) &&
		    (from->sendto == who_wanna_recv->endpoint)) {
			/* Perfect, src is sending a message to
			 * who_wanna_recv.
			 */
			copyok = 1;

			struct proc* p = who_wanna_recv->q_sending;
			assert(p); /* from must have been appended to the
				    * queue, so the queue must not be NULL
				    */
			while (p) {
				if (p->endpoint == src) { /* if p is the one */
					from = p;
					break;
				}
				prev = p;
				p = p->next_sending;
			}

			lock_proc(from);
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
		retval = data_vir_copy_check(who_wanna_recv, KERNEL, m, KERNEL, &from->send_msg, sizeof(MESSAGE));
		if (retval != 0) goto out;

		reset_msg(&from->send_msg);
		from->sendto = NO_TASK;
		PST_UNSET_LOCKED(from, PST_SENDING);

		unlock_proc(from);

		goto out;
	}

no_msg:
	/* nobody's sending any msg */
	/* Set state so that who_wanna_recv will not
	* be scheduled until it is unblocked.
	*/
	PST_SET_LOCKED(who_wanna_recv, PST_RECEIVING);

	who_wanna_recv->recv_msg = m;
	who_wanna_recv->recvfrom = src;

out:
	unlock_proc(who_wanna_recv);

	return retval;
}

PRIVATE int receive_async_from(struct proc* p, struct proc* sender)
{
	struct priv* priv = sender->priv;
	if (!(priv->flags & PRF_PRIV_PROC)) {	/* only privilege processes can send async messages */
        return EPERM;
    }

    lock_proc(sender);
    p->priv->async_pending &= ~(1 << priv->id);

    int i, retval = ESRCH, flags, done = TRUE;
    endpoint_t dest;
    async_message_t amsg;
    /* process all async messages */
    for (i = 0; i < priv->async_len; i++) {
    	if (data_vir_copy(KERNEL, &amsg, sender->endpoint, (void*)((vir_bytes)priv->async_table + i * sizeof(amsg)), sizeof(amsg)) != 0) {
    		retval = EFAULT;
    		goto async_error;
    	}

    	flags = amsg.flags;
    	dest = amsg.dest;
    	amsg.msg.source = sender->endpoint;

    	if (!flags) continue;
    	if (flags & ASMF_DONE) continue;

    	done = FALSE;
    	if (dest != p->endpoint) continue;
		retval = data_vir_copy_check(p, KERNEL, p->recv_msg, KERNEL, &amsg.msg, sizeof(MESSAGE));
		if (retval != 0) {
			goto async_error;
		}

		p->recv_msg = 0;
		PST_UNSET_LOCKED(p, PST_RECEIVING);
		p->recvfrom = NO_TASK;

		amsg.result = retval;
		amsg.flags |= ASMF_DONE;

		if (data_vir_copy(sender->endpoint, (void*)((vir_bytes)priv->async_table + i * sizeof(amsg)), KERNEL, &amsg, sizeof(amsg)) != 0) {
			retval = EFAULT;
    		goto async_error;
    	}

    	break;
    }

    if (done) {
    	priv->async_table = NULL;
    	priv->async_len = 0;
    } else {	/* make kernel rescan the table next time the receiver try to receive and set priv->async_table properly */
    	p->priv->async_pending |= (1 << priv->id);
    }

async_error:
	unlock_proc(sender);
    return retval;
}

PRIVATE int receive_async(struct proc* p)
{
	int retval;
	priv_map_t async_pending = p->priv->async_pending;
	struct priv* priv;
	
	for (priv = &FIRST_PRIV; priv < &LAST_PRIV; priv++) {
		if (priv->proc_nr == NO_TASK) continue;
		if (!(async_pending & (1 << priv->id))) continue;

		struct proc* src = proc_addr(priv->proc_nr);

		if ((retval = receive_async_from(p, src)) == 0) return 0;
	}

	return ESRCH;
}

PRIVATE int has_pending_async(struct proc * p, endpoint_t src)
{
	priv_map_t async_pending = p->priv->async_pending;
	int i;

	if (async_pending == 0) return PRIV_ID_NULL;

	if (src != ANY) {
		struct proc * sender = endpt_proc(src);
		if (!sender) return PRIV_ID_NULL;

		if (async_pending & (1 << sender->priv->id)) return sender->priv->id; 
		else return PRIV_ID_NULL;
	}

	for (i = 0; i < NR_PRIV_PROCS; i++) {
		if (async_pending & (1 << i)) return i;
	}

	return PRIV_ID_NULL;
}

PRIVATE int has_pending_notify(struct proc * p, endpoint_t src)
{
	priv_map_t notify_pending = p->priv->notify_pending;
	int i;

	if (notify_pending == 0) return PRIV_ID_NULL;

	if (src != ANY) {
		struct proc * sender = endpt_proc(src);
		if (!sender) return PRIV_ID_NULL;

		if (notify_pending & (1 << sender->priv->id)) return sender->priv->id; 
		else return PRIV_ID_NULL;
	}

	for (i = 0; i < NR_PRIV_PROCS; i++) {
		if (notify_pending & (1 << i)) return i;
	}

	return PRIV_ID_NULL;
}

PRIVATE void unset_notify_pending(struct proc * p, int id)
{
	p->priv->notify_pending &= ~(1 << id);
}

PRIVATE void set_notify_msg(struct proc * dest, MESSAGE * m, endpoint_t src)
{
	memset(m, 0, sizeof(MESSAGE));
	m->source = src;
	m->type = NOTIFY_MSG;
	switch (src) {
	case INTERRUPT:
		m->INTERRUPTS = dest->priv->int_pending;
		dest->priv->int_pending = 0;
		break;
	case SYSTEM:
		m->SIGSET = dest->priv->sig_pending;
		sigemptyset(&dest->priv->sig_pending);
		break;
	case CLOCK:
		m->TIMESTAMP = jiffies;
		break;
	default:
		break;
	}
}

/*****************************************************************************
 *                                msg_notify
 *****************************************************************************/
/**
 * @brief Send a notification to the dest proc.
 * 
 * @param p_to_send	Who wants to send the notification.
 * @param dest The dest proc.
 * 
 * @return Zero on success, otherwise errcode.
 */
PUBLIC int msg_notify(struct proc * p_to_send, endpoint_t dest)
{
	struct proc * p_dest = endpt_proc(dest);
	if (!p_dest) return EINVAL;
	int retval = 0;

	lock_proc(p_dest);

	if (!PST_IS_SET(p_dest, PST_SENDING) && PST_IS_SET(p_dest, PST_RECEIVING) && /* p_dest is waiting for the msg */
	    (p_dest->recvfrom == p_to_send->endpoint ||
	     p_dest->recvfrom == ANY)) {
		
		MESSAGE m;
		set_notify_msg(p_dest, &m, p_to_send->endpoint);
		retval = data_vir_copy_check(p_to_send, dest, p_dest->recv_msg, p_to_send->endpoint, &m, sizeof(MESSAGE));
		if (retval != 0) {
			unlock_proc(p_dest);
			return retval;
		}

		p_dest->recv_msg = NULL;
		PST_UNSET_LOCKED(p_dest, PST_RECEIVING);
		p_dest->recvfrom = NO_TASK;

		unlock_proc(p_dest);
		return retval;
	}

	/* p_dest is not waiting for this notification, set pending bit */
	p_dest->priv->notify_pending |= (1 << p_to_send->priv->id);
	unlock_proc(p_dest);
	return 0;
}

/*****************************************************************************
 *                                verify_endpt
 *****************************************************************************/
/**
 * @brief Verify if an endpoint number is valid and convert it to proc nr.
 * 
 * @param ep Endpoint number.
 * @param proc_nr [out] Ptr to proc nr.
 * 
 * @return Non-zero if the endpoint number is valid.
 */
PUBLIC int verify_endpt(endpoint_t ep, int * proc_nr)
{
	if (ep < -NR_TASKS) return 0;
	if (ep == NO_TASK || ep == ANY || ep == SELF) return 1;

	int n = ENDPOINT_P(ep);
	int slot = proc_slot(n);

	if (slot < 0 || slot > NR_PROCS) return 0;

	if (proc_nr) *proc_nr = n;

	return 1;
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
	printk("\n\n%s<0x%x>{%ssrc:%d,%stype:%d,%sm->u.m3:{0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}%s}%s",  //, (0x%x, 0x%x, 0x%x)}",
	       title,
	       (int)m,
	       packed ? "" : "\n        ",
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

/*****************************************************************************
 *                                msg_senda
 *****************************************************************************/
/**
 * <Ring 0> Send asynchronous messages.
 * 
 * @param p_to_send  The caller, the sender.
 * @param table 	 The table containing all async messages to be sent.
 * @param len        How many messages to be sent.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int msg_senda(struct proc* p_to_send, async_message_t* table, size_t len)
{
    struct priv* priv = p_to_send->priv;
    if (!(priv->flags & PRF_PRIV_PROC)) {	/* only privilege processes can send async messages */
        return EPERM;
    }

    /* clear async message table */
    priv->async_table = NULL;
    priv->async_len = 0;

    if (len == 0) return 0;

    lock_proc(p_to_send);

    int i, retval, flags, done = TRUE;
    endpoint_t dest;
    async_message_t amsg;
    struct proc* p_dest;
    /* process all async messages */
    for (i = 0; i < len; i++) {
    	retval = data_vir_copy(KERNEL, &amsg, KERNEL, (void*)((vir_bytes)table + i * sizeof(amsg)), sizeof(amsg));
    	if (retval) goto async_error;

    	flags = amsg.flags;
    	dest = amsg.dest;
    	amsg.msg.source = p_to_send->endpoint;
    	
    	if (dest == p_to_send->endpoint) {
    		retval = EINVAL;
    		goto async_error;
    	}

    	if (!flags) continue;
    	if (flags & ASMF_DONE) continue;

    	p_dest = endpt_proc(dest);
    	if (!p_dest) {
    		retval = EINVAL;
    		goto async_error;
    	}

    	lock_proc(p_dest);

    	if (!PST_IS_SET(p_dest, PST_SENDING) && PST_IS_SET(p_dest, PST_RECEIVING) && /* p_dest is waiting for the msg */
	    	(p_dest->recvfrom == p_to_send->endpoint ||
	     	p_dest->recvfrom == ANY)) {
			retval = data_vir_copy_check(p_to_send, dest, p_dest->recv_msg, KERNEL, &amsg.msg, sizeof(MESSAGE));
			if (retval != 0) {
				unlock_proc(p_dest);
				goto async_error;
			}

			p_dest->recv_msg = 0;
			PST_UNSET_LOCKED(p_dest, PST_RECEIVING);
			p_dest->recvfrom = NO_TASK;
		} else {	/* tell dest that it has a pending async message */
			p_dest->priv->async_pending |= (1 << priv->id);
			done = FALSE;
			unlock_proc(p_dest);
			continue;
		}

		amsg.result = retval;
		amsg.flags |= ASMF_DONE;

		retval = data_vir_copy(KERNEL, (void*)((vir_bytes)table + i * sizeof(amsg)), KERNEL, &amsg, sizeof(amsg));
    	if (retval) goto async_error;

    	unlock_proc(p_dest);
    	continue;
async_error:
		printk("kernel: msg_senda failed(%d)\n", retval);
    }

    /* save the table if not done */
    if (!done) {
    	priv->async_table = table;
    	priv->async_len = len;
    }

    unlock_proc(p_to_send);

    return 0;
}
