/*
 * iperf, Copyright (c) 2014-2018, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject
 * to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE.  This software is owned by the U.S. Department of Energy.
 * As such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * This code is distributed under a BSD style license, see the LICENSE
 * file for complete information.
 */


#include <stddef.h>

#include "iperf_config.h"
#include "iperf_time.h"

#ifdef HAVE_CLOCK_GETTIME

#include <posix/time.h>

#if defined (CONFIG_FTA_IPERF3_FUNCTIONAL_CHANGES)
#if !defined (CONFIG_POSIX_API)
int z_impl_clock_gettime(clockid_t clock_id, struct timespec *ts)
{
	uint64_t elapsed_msecs;
	struct timespec base;

	switch (clock_id) {
	case CLOCK_MONOTONIC:
	case CLOCK_REALTIME:
		base.tv_sec = 0;
		base.tv_nsec = 0;
		break;


	default:
		errno = EINVAL;
		return -1;
	}

	elapsed_msecs = k_uptime_get();
	ts->tv_sec = (int32_t) (elapsed_msecs / MSEC_PER_SEC);
	ts->tv_nsec = (int32_t) ((elapsed_msecs % MSEC_PER_SEC) *
					USEC_PER_MSEC * NSEC_PER_USEC);

	ts->tv_sec += base.tv_sec;
	ts->tv_nsec += base.tv_nsec;
	if (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec++;
		ts->tv_nsec -= NSEC_PER_SEC;
	}

	return 0;
}
#ifdef NOT_IN_FTA_IPERF3_INTEGRATION
static inline int clock_gettime(clockid_t clock_id, struct timespec * ts)
{
	return mock_impl_clock_gettime(clock_id, ts);
}
#endif

#endif
#endif
int
iperf_time_now(struct iperf_time *time1)
{
    struct timespec ts;
    int result;
    result = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (result == 0) {
        time1->secs = (uint32_t) ts.tv_sec;
        time1->usecs = (uint32_t) ts.tv_nsec / 1000;
    }
    return result;
}

#else

#include <sys/time.h>

int
iperf_time_now(struct iperf_time *time1)
{
    struct timeval tv;
    int result;
    result = gettimeofday(&tv, NULL);
    time1->secs = tv.tv_sec;
    time1->usecs = tv.tv_usec;
    return result;
}

#endif

/* iperf_time_add_usecs
 *
 * Add a number of microseconds to a iperf_time.
 */
void
iperf_time_add_usecs(struct iperf_time *time1, uint64_t usecs)
{
    time1->secs += usecs / 1000000L;
    time1->usecs += usecs % 1000000L;
    if ( time1->usecs >= 1000000L ) {
        time1->secs += time1->usecs / 1000000L;
        time1->usecs %= 1000000L;
    }
}

uint64_t
iperf_time_in_usecs(struct iperf_time *time)
{
    return time->secs * 1000000LL + time->usecs;
}

double
iperf_time_in_secs(struct iperf_time *time)
{
    return time->secs + time->usecs / 1000000.0; 
}

/* iperf_time_compare
 *
 * Compare two timestamps
 * 
 * Returns -1 if time1 is earlier, 1 if time1 is later,
 * or 0 if the timestamps are equal.
 */
int
iperf_time_compare(struct iperf_time *time1, struct iperf_time *time2)
{
    if (time1->secs < time2->secs)
        return -1;
    if (time1->secs > time2->secs)
        return 1;
    if (time1->usecs < time2->usecs)
        return -1;
    if (time1->usecs > time2->usecs)
        return 1;
    return 0;
}

/* iperf_time_diff
 *
 * Calculates the time from time2 to time1, assuming time1 is later than time2.
 * The diff will always be positive, so the return value should be checked
 * to determine if time1 was earlier than time2.
 *
 * Returns 1 if the time1 is less than or equal to time2, otherwise 0.
 */
int 
iperf_time_diff(struct iperf_time *time1, struct iperf_time *time2, struct iperf_time *diff)
{
    int past = 0;
    int cmp = 0;

    cmp = iperf_time_compare(time1, time2);
    if (cmp == 0) {
        diff->secs = 0;
        diff->usecs = 0;
        past = 1;
    } 
    else if (cmp == 1) {
        diff->secs = time1->secs - time2->secs;
        diff->usecs = time1->usecs;
        if (diff->usecs < time2->usecs) {
            diff->secs -= 1;
            diff->usecs += 1000000;
        }
        diff->usecs = diff->usecs - time2->usecs;
    } else {
        diff->secs = time2->secs - time1->secs;
        diff->usecs = time2->usecs;
        if (diff->usecs < time1->usecs) {
            diff->secs -= 1;
            diff->usecs += 1000000;
        }
        diff->usecs = diff->usecs - time1->usecs;
        past = 1;
    }

    return past;
}
