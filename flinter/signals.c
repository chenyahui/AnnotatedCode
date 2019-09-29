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

#include "flinter/signals.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

static int signals_do_mask_some(int mask, int signum, va_list ap)
{
    sigset_t set;
    int sig;

    if (sigemptyset(&set) || sigaddset(&set, signum)) {
        return -1;
    }

    for (;;) {
        sig = va_arg(ap, int);
        if (!sig) {
            break;
        }

        if (sigaddset(&set, sig)) {
            return -1;
        }
    }

    if (sigprocmask(mask, &set, NULL)) {
        return -1;
    }

    return 0;
}

static int signals_do_mask_all(int mask)
{
    sigset_t set;

    if (sigfillset(&set) || sigprocmask(mask, &set, NULL)) {
        return -1;
    }

    return 0;
}

static int signals_do_mask(int mask, int signum, ...)
{
    va_list ap;
    int ret;

    va_start(ap, signum);
    ret = signals_do_mask_some(mask, signum, ap);
    va_end(ap);

    return ret;
}

static int signals_do_set_some(void (*handler)(int), int signum, va_list ap)
{
    int sig;

    if (signals_set_handler(signum, handler)) {
        return -1;
    }

    for (;;) {
        sig = va_arg(ap, int);
        if (!sig) {
            va_end(ap);
            return 0;
        }

        if (signals_set_handler(sig, handler)) {
            if (errno != EINVAL) {
                va_end(ap);
                return -1;
            }
        }
    }

    return 0;
}

static int signals_do_set_all(void (*handler)(int))
{
    int i;

    /* 0 is not a valid signal number. */
    for (i = 1; i < NSIG; ++i) {
        if (signals_set_handler(i, handler)) {
            if (errno != EINVAL) {
                return -1;
            }
        }
    }

    return 0;
}

int signals_ignore_all(void)
{
    return signals_do_set_all(SIG_IGN);
}

int signals_default_all(void)
{
    return signals_do_set_all(SIG_DFL);
}

int signals_unblock_all(void)
{
    return signals_do_mask_all(SIG_UNBLOCK);
}

int signals_block_all(void)
{
    return signals_do_mask_all(SIG_BLOCK);
}

int signals_block_some(int signum, ...)
{
    va_list ap;
    int ret;

    va_start(ap, signum);
    ret = signals_do_mask_some(SIG_BLOCK, signum, ap);
    va_end(ap);

    return ret;
}

int signals_unblock_some(int signum, ...)
{
    va_list ap;
    int ret;

    va_start(ap, signum);
    ret = signals_do_mask_some(SIG_UNBLOCK, signum, ap);
    va_end(ap);

    return ret;
}

int signals_ignore_some(int signum, ...)
{
    va_list ap;
    int ret;

    va_start(ap, signum);
    ret = signals_do_set_some(SIG_IGN, signum, ap);
    va_end(ap);

    return ret;
}

int signals_default_some(int signum, ...)
{
    va_list ap;
    int ret;

    va_start(ap, signum);
    ret = signals_do_set_some(SIG_DFL, signum, ap);
    va_end(ap);

    return ret;
}

int signals_block(int signum)
{
    return signals_do_mask(SIG_BLOCK, signum, 0);
}

int signals_unblock(int signum)
{
    return signals_do_mask(SIG_UNBLOCK, signum, 0);
}

int signals_ignore(int signum)
{
    return signals_set_handler(signum, SIG_IGN);
}

int signals_default(int signum)
{
    return signals_set_handler(signum, SIG_DFL);
}

int signals_set_handler(int signum, void (*handler)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    if (sigemptyset(&sa.sa_mask)) {
        return -1;
    }

    sa.sa_handler = handler;
    return sigaction(signum, &sa, NULL);
}

int signals_set_action(int signum, void (*action)(int, siginfo_t *, void *))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    if (sigemptyset(&sa.sa_mask)) {
        return -1;
    }

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = action;
    return sigaction(signum, &sa, NULL);
}
