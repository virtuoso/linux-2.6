#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define TFD_NOTIFY_CLOCK_CHANGE (1 << 1)
int main()
{
	struct itimerspec utmr;
	struct timespec now;
	uint64_t exp = 0x1234567890abcdef;
	int tfd;

	memset(&utmr, 0, sizeof(utmr));
	tfd = timerfd_create(CLOCK_REALTIME, 0);
	if (tfd == -1) {
		perror("timerfd_create");
		exit(EXIT_FAILURE);
	}
	if (timerfd_settime(tfd, TFD_NOTIFY_CLOCK_CHANGE,
			    &utmr, NULL) == -1) {
		perror("timerfd_settime");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		struct pollfd fds = { .fd = tfd, .events = POLLIN };
		struct tm tm;
		ssize_t l;
		int n;

		n = poll(&fds, 1, 10000);
		if (n == 1) {
			l = read(tfd, &exp, sizeof(uint64_t));
			assert(l == sizeof(exp));
		} else
			exp = 0;

		timerfd_gettime(tfd, &utmr);
		localtime_r(&utmr.it_value.tv_sec, &tm);
		printf("timerfd woke up %llu @ %02d:%02d:%02d.%lu\n", exp,
		       tm.tm_hour, tm.tm_min, tm.tm_sec, utmr.it_value.tv_nsec);
	}

	return 0;
}
