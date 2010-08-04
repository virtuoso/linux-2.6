/*
 * Simple program to catch system time changes
 *
 * written by Alexander Shishkin <virtuoso@slind.org>
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#ifndef SYS_time_change_notify
# include "asm/unistd.h"
# ifdef __NR_time_change_notify
#  define SYS_time_change_notify __NR_time_change_notify
# else
#  error Cannot figure out time_change_notify syscall number.
# endif
#endif

static int time_change_notify(clockid_t clockid, int fd, unsigned int flags)
{
	return syscall(SYS_time_change_notify, clockid, fd, flags);
}

int main(int argc, char **argv)
{
	struct pollfd fds = { .events = POLLIN };

	fds.fd = eventfd(0, 0);
	if (fds.fd < 0) {
		perror("eventfd");
		return EXIT_FAILURE;
	}

	/* subscribe to all events from all sources */
	if (time_change_notify(CLOCK_REALTIME, fds.fd, 0)) {
		perror("time_change_notify");
		return EXIT_FAILURE;
	}

	while (poll(&fds, 1, -1) > 0) {
		eventfd_t data;
		ssize_t r;

		r = read(fds.fd, &data, sizeof data);
		if (r == -1) {
			if (errno == EINTR)
				continue;

			break;
		}

		printf("system time has changed %llu times\n", data);
	}

	puts("Done polling system time changes.\n");

	return EXIT_SUCCESS;
}

