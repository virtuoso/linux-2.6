/*
 * linux/kernel/time/notify.c
 *
 * Copyright (C) 2010 Nokia Corporation
 * Alexander Shishkin
 *
 * This file implements an interface to communicate time changes to userspace.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/eventfd.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/err.h>

/*
 * A process can "subscribe" to receive a notification via eventfd that
 * some other process has called stime/settimeofday/adjtimex.
 */
struct time_event {
	struct eventfd_ctx	*eventfd;
	clockid_t		clockid;
	unsigned int		type;
	struct work_struct	remove;
	wait_queue_t		wq;
	wait_queue_head_t	*wqh;
	poll_table		pt;
	struct list_head	list;
};

static LIST_HEAD(event_list);
static DEFINE_SPINLOCK(event_lock);

/*
 * Do the necessary cleanup when the eventfd is being closed
 */
static void time_event_remove(struct work_struct *work)
{
	struct time_event *evt = container_of(work, struct time_event, remove);
	__u64 cnt;

	eventfd_ctx_remove_wait_queue(evt->eventfd, &evt->wq, &cnt);
	kfree(evt);
}

static int time_event_wakeup(wait_queue_t *wq, unsigned int mode, int sync,
			     void *key)
{
	struct time_event *evt = container_of(wq, struct time_event, wq);
	unsigned long flags = (unsigned long)key;

	if (flags & POLLHUP) {
		spin_lock(&event_lock);
		list_del(&evt->list);
		spin_unlock(&event_lock);

		schedule_work(&evt->remove);
	}

	return 0;
}

static void time_event_ptable_queue_proc(struct file *file,
					 wait_queue_head_t *wqh, poll_table *pt)
{
	struct time_event *evt = container_of(pt, struct time_event, pt);

	evt->wqh = wqh;
	add_wait_queue(wqh, &evt->wq);
}

/*
 * time_change_notify() registers a given eventfd to receive time change
 * notifications
 */
SYSCALL_DEFINE3(time_change_notify, clockid_t, clockid, int, fd, unsigned int,
		type)
{
	struct time_event *evt;
	struct file *file;
	int ret;

	/*
	 * for now only accept CLOCK_REALTIME clockid; in future CLOCK_RTC
	 * will make use of this interface as well; with dynamic clockids
	 * it may find even more users
	 */
	if ((type != TIME_EVENT_SET && type != TIME_EVENT_ADJ) ||
	    clockid != CLOCK_REALTIME)
		return -EINVAL;

	evt = kmalloc(sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return -ENOMEM;

	evt->type = type;
	evt->clockid = clockid;

	file = eventfd_fget(fd);
	if (IS_ERR(file)) {
		ret = -EINVAL;
		goto out_free;
	}

	evt->eventfd = eventfd_ctx_fileget(file);
	if (IS_ERR(evt->eventfd)) {
		ret = PTR_ERR(evt->eventfd);
		goto out_fput;
	}

	INIT_LIST_HEAD(&evt->list);
	INIT_WORK(&evt->remove, time_event_remove);

	init_waitqueue_func_entry(&evt->wq, time_event_wakeup);
	init_poll_funcptr(&evt->pt, time_event_ptable_queue_proc);

	if (file->f_op->poll(file, &evt->pt) & POLLHUP) {
		ret = 0;
		goto out_ctxput;
	}

	spin_lock(&event_lock);
	list_add(&evt->list, &event_list);
	spin_unlock(&event_lock);

	fput(file);

	return 0;

out_ctxput:
	eventfd_ctx_put(evt->eventfd);

out_fput:
	fput(file);

out_free:
	kfree(evt);

	return ret;
}

void time_notify_all(clockid_t clockid, int type)
{
	struct list_head *tmp;

	spin_lock(&event_lock);
	list_for_each(tmp, &event_list) {
		struct time_event *e = container_of(tmp, struct time_event,
						    list);

		if (e->type != type || e->clockid != clockid)
			continue;

		eventfd_signal(e->eventfd, 1);
	}
	spin_unlock(&event_lock);
}

