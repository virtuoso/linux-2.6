/*
 * linux/kernel/time_notify.c
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

/* sysctl tunable to limit the number of users */
unsigned int time_change_notify_max_users = TIME_CHANGE_NOTIFY_MAX_USERS;

/*
 * A process can "subscribe" to receive a notification via eventfd that
 * some other process has called stime/settimeofday/adjtimex.
 */
struct time_event {
	struct eventfd_ctx	*eventfd;
	struct task_struct	*watcher;
	unsigned int		want_others:1;
	unsigned int		want_own:1;
	unsigned int		want_set:1;
	unsigned int		want_adj:1;
	struct work_struct	remove;
	wait_queue_t		wq;
	wait_queue_head_t	*wqh;
	poll_table		pt;
	struct list_head	list;
};

static LIST_HEAD(event_list);
static int nevents;
static DEFINE_SPINLOCK(event_lock);

/*
 * Do the necessary cleanup when the eventfd is being closed
 */
static void time_event_remove(struct work_struct *work)
{
	struct time_event *evt = container_of(work, struct time_event, remove);

	BUG_ON(nevents <= 0);

	kfree(evt);
	nevents--;
}

static int time_event_wakeup(wait_queue_t *wq, unsigned int mode, int sync,
			     void *key)
{
	struct time_event *evt = container_of(wq, struct time_event, wq);
	unsigned long flags = (unsigned long)key;

	if (flags & POLLHUP) {
		__remove_wait_queue(evt->wqh, &evt->wq);
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
SYSCALL_DEFINE2(time_change_notify, int, fd, unsigned int, flags)
{
	int ret;
	struct file *file;
	struct time_event *evt;

	evt = kmalloc(sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return -ENOMEM;

	evt->want_others = !!(flags & TIME_CHANGE_NOTIFY_OTHERS);
	evt->want_own = !!(flags & TIME_CHANGE_NOTIFY_OWN);
	evt->want_set = !!(flags & TIME_CHANGE_NOTIFY_SET);
	evt->want_adj = !!(flags & TIME_CHANGE_NOTIFY_ADJUST);

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

	evt->watcher = current;

	spin_lock(&event_lock);
	if (nevents == time_change_notify_max_users) {
		spin_unlock(&event_lock);
		ret = -EBUSY;
		goto out_fput;
	}

	nevents++;
	list_add(&evt->list, &event_list);
	spin_unlock(&event_lock);

	if (file->f_op->poll(file, &evt->pt) & POLLHUP) {
		ret = 0;
		goto out_fput;
	}

	fput(file);

	return 0;

out_fput:
	fput(file);

out_free:
	kfree(evt);

	return ret;
}

void time_notify_all(int type)
{
	struct list_head *tmp;

	spin_lock(&event_lock);
	list_for_each(tmp, &event_list) {
		struct time_event *e = container_of(tmp, struct time_event,
						    list);

		if (type == TIME_EVENT_SET && !e->want_set)
			continue;
		else if (type == TIME_EVENT_ADJ && !e->want_adj)
			continue;

		if (e->watcher == current && !e->want_own)
			continue;
		else if (e->watcher != current && !e->want_others)
			continue;

		eventfd_signal(e->eventfd, 1);
	}
	spin_unlock(&event_lock);
}

static int time_notify_init(void)
{
	return 0;
}

core_initcall(time_notify_init);
