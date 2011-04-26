#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/timerfd.h>
#include <poll.h>

#ifndef SYS_clock_rtoffset
# ifdef __arm__
#  define SYS_clock_rtoffset 374
# elif defined(__x86_64__)
#  define SYS_clock_rtoffset 307
# endif
#endif

#ifndef TFD_CANCEL_ON_CLOCK_SET
# define TFD_CANCEL_ON_CLOCK_SET (1 << 1)
#endif
#ifndef TIMER_CANCEL_ON_CLOCK_SET
# define TIMER_CANCEL_ON_CLOCK_SET 0x02
#endif

static int clock_rtoffset(struct timespec *offset)
{
	int ret;

	ret = syscall(SYS_clock_rtoffset, offset);
	if (ret) {
		fprintf(stderr, "clock_rtoffset: %m\n");
		ret = 0;
		memset(offset, 0, sizeof(*offset));
	}

	return ret;
}

static int _clock_nanosleep(clockid_t clockid, int flags,
			    const struct timespec *request,
			    struct timespec *rmtp)
{
	int ret = syscall(SYS_clock_nanosleep, clockid, flags, request,
		      rmtp);
	if (ret)
		fprintf(stderr, "clock_rtoffset: %m\n");

	return ret;
}

/* set up a timerfd 3 seconds from now */
static void timer_setup(struct itimerspec *utmr, struct itimerspec *otmr)
{
	memset(utmr, 0, sizeof(*utmr));
	clock_gettime(CLOCK_REALTIME, &utmr->it_value);
	utmr->it_value.tv_sec += 3;

	memset(otmr, 0, sizeof(*otmr));
	clock_rtoffset(&otmr->it_interval);

}

static int waitforit(int tfd)
{
	struct pollfd fds = { .fd = tfd, .events = POLLIN | POLLHUP };
	int ret;

	ret = poll(&fds, 1, -1);
	if (ret < 1) {
		perror("poll");
		return -1;
	}

	return fds.revents;
}

static pid_t xfork(void)
{
	pid_t pid = fork();

	if (pid == -1) {
		fprintf(stderr, "fork() failed: %m\n");
		exit(EXIT_FAILURE);
	}

	return pid;
}

int main(int argc, char **argv)
{
	struct itimerspec utmr, otmr;
	struct timespec ts, sleeptime;
	struct tm tm;
	int ret, tfd;

	ret = clock_rtoffset(&ts);
	if (ret) {
		perror("clock_rtoffset");
		return EXIT_FAILURE;
	}

	localtime_r(&ts.tv_sec, &tm);
	printf("=> %04d/%02d/%02d %02d:%02d:%02d.%lu\n",
	       tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,
	       tm.tm_sec, ts.tv_nsec);

	tfd = timerfd_create(CLOCK_REALTIME, 0);
	if (tfd == -1) {
		perror("timerfd_create");
		return EXIT_FAILURE;
	}

#if 1
	/* test 0: nanosleep cancellation */
	timer_setup(&utmr, &otmr);

	if (!xfork()) {
		sleep(1);
		clock_settime(CLOCK_REALTIME, &utmr.it_value);
		exit(EXIT_SUCCESS);
	}
	ret = _clock_nanosleep(CLOCK_REALTIME,
			      TIMER_CANCEL_ON_CLOCK_SET | TIMER_ABSTIME,
			      &utmr.it_value, &ts);
	fprintf(stderr, "test 0 %s, ret=%d.%ld (%m)\n", ret == -1 ? "OK" : "FAILED",
		ret, ts.tv_sec, ts.tv_nsec);
	(void)wait(NULL);
#endif

	/* test 1: normal timerfd expiration w/o flag */
	timer_setup(&utmr, &otmr);
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME,
			      &utmr, &otmr);
	if (ret == -1) {
		perror("timerfd_settime");
		return EXIT_FAILURE;
	}

	ret = waitforit(tfd);
	fprintf(stderr, "test 1 %s, ret=%d\n", ret == POLLIN ? "OK" : "FAILED",
		ret);

	/* test 2: normal timerfd expiration with the flag */
	timer_setup(&utmr, &otmr);
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_CANCEL_ON_CLOCK_SET,
			      &utmr, &otmr);
	if (ret == -1) {
		perror("timerfd_settime");
		return EXIT_FAILURE;
	}

	ret = waitforit(tfd);
	fprintf(stderr, "test 2 %s, ret=%d\n", ret == POLLIN ? "OK" : "FAILED",
		ret);

	/* test 3: cancellation, time set backwards */
	timer_setup(&utmr, &otmr);
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_CANCEL_ON_CLOCK_SET,
			      &utmr, &otmr);
	if (ret == -1) {
		perror("timerfd_settime");
		return EXIT_FAILURE;
	}

	if (!xfork()) {
		sleep(1);
		clock_settime(CLOCK_REALTIME, &utmr.it_value);
		exit(EXIT_SUCCESS);
	}

	ret = waitforit(tfd);
	fprintf(stderr, "test 3 %s, ret=%d\n", ret == POLLHUP ? "OK" : "FAILED",
		ret);
	(void)wait(NULL);

	/* test 4: time set backwards before timerfd_settime() */
	timer_setup(&utmr, &otmr);
	clock_settime(CLOCK_REALTIME, &utmr.it_value);
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_CANCEL_ON_CLOCK_SET,
			      &utmr, &otmr);
	fprintf(stderr, "test 4 %s, ret=%d (%m)\n", ret == -1 ? "OK" : "FAILED",
		ret);

	/* test 5: cancellation, time set forward */
	timer_setup(&utmr, &otmr);
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_CANCEL_ON_CLOCK_SET,
			      &utmr, &otmr);
	if (ret == -1) {
		perror("timerfd_settime");
		return EXIT_FAILURE;
	}

	if (!xfork()) {
		sleep(1);
		utmr.it_value.tv_sec += 60;
		clock_settime(CLOCK_REALTIME, &utmr.it_value);
		exit(EXIT_SUCCESS);
	}

	ret = waitforit(tfd);
	fprintf(stderr, "test 5 %s, ret=%d\n", ret & POLLHUP ? "OK" : "FAILED",
		ret);
	(void)wait(NULL);

	/* test 6: invalid parameter */
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_CANCEL_ON_CLOCK_SET,
			      &utmr, NULL);
	fprintf(stderr, "test 6 %s, ret=%d (%m)\n", ret == -1 ? "OK" : "FAILED",
		ret);

	/* test 7: nanosleep normal expiration */
	clock_rtoffset(&ts);
	sleeptime.tv_sec = 1;
	sleeptime.tv_nsec = 0;
	ret = _clock_nanosleep(CLOCK_REALTIME,
			      TIMER_CANCEL_ON_CLOCK_SET | TIMER_ABSTIME,
			      &sleeptime, &ts);
	fprintf(stderr, "test 7 %s [%d], ret=%d.%ld\n", ret == 0 ? "OK" : "FAILED",
		ret, ts.tv_sec, ts.tv_nsec);

	return EXIT_SUCCESS;
}
