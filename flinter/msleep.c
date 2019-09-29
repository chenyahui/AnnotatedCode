/* Copyright 2014 yiyuanzhong@gmail.com (Yiyuan Zhong)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "flinter/msleep.h"

#include "flinter/utility.h"

#include "config.h"
#if defined(WIN32)
# include <Windows.h>
#elif HAVE_SYS_SELECT_H
# include <sys/select.h>
# include <errno.h>
# include <stdint.h>
# if HAVE_SCHED_H
#  include <sched.h>
# endif
#else
# error Unsupported: msleep
#endif

extern int pselect(int n,
                   fd_set *readfds,
                   fd_set *writefds,
                   fd_set *exceptfds,
                   const struct timespec *timeout,
                   const sigset_t *sigmask);

#ifdef WIN32
int msleep_signal(int milliseconds, int alertable, const sigset_t *sigset)
{
    (void)alertable;
    (void)sigset;

    if (milliseconds < 0) {
        milliseconds = 0;
    }

    Sleep((DWORD)milliseconds);
    return 0;
}
#else
int msleep_signal(int milliseconds, int alertable, const sigset_t *sigset)
{
    int ret;
    int64_t now;
    int64_t diff;
    int64_t deadline;
    struct timespec time;

    if (milliseconds <= 0) {
#if HAVE_SCHED_YIELD
        ret = sched_yield();
        if (ret) {
            return -1;
        }
#endif
        return 0;
    }

    diff = (int64_t)milliseconds * 1000000LL;
    deadline = get_monotonic_timestamp() + diff;
    for (;;) {
        time.tv_nsec = (long)(diff % 1000000000LL);
        time.tv_sec  = (long)(diff / 1000000000LL);
        ret = pselect(0, NULL, NULL, NULL, &time, sigset);
        if (ret == 0) {
            return 0;
        } else if (ret < 0) {
            if (errno != EINTR) {
                return -1;
            }
        }

        now = get_monotonic_timestamp();
        diff = deadline - now;
        if (diff <= 0) {
            return 0;
        }

        if (alertable) {
            break;
        }
    }

    /* Ceiling. */
    milliseconds = (int)(diff / 1000000LL);
    if ((diff % 1000000LL)) {
        ++milliseconds;
    }

    return milliseconds;
}
#endif

int msleep_ex(int milliseconds, int alertable)
{
    return msleep_signal(milliseconds, alertable, NULL);
}

void msleep(int milliseconds)
{
    msleep_signal(milliseconds, 0, NULL);
}
