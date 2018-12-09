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

#include "flinter/babysitter.h"

#include <sys/select.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "flinter/msleep.h"
#include "flinter/safeio.h"
#include "flinter/signals.h"
#include "flinter/utility.h"

static volatile sig_atomic_t g_child_exited = 0;
static volatile size_t g_sigtail = 0;
static volatile int g_signals[16];
static int g_watchdog_fd = -1;

static void babysitter_signal_handler(int signum)
{
    if (signum == SIGCHLD) {
        g_child_exited = 1;
        return;
    }

    /* Signal queue is full, drop signals. */
    if (g_sigtail == sizeof(g_signals) / sizeof(*g_signals)) {
        return;
    }

    g_signals[g_sigtail++] = signum;
}

static int babysitter_suck_fd(int fd)
{
    char buffer[128];
    ssize_t ret;
    int r;

    r = 0;
    for (;;) {
        ret = safe_read(fd, buffer, sizeof(buffer));
        if (ret < 0) {
            return errno == EAGAIN ? r : -1;
        } else if (ret == 0) {
            return -1;
        }

        r = 1;
    }
}

static void babysitter_dump_signals(void)
{
    g_sigtail = 0;
}

static int babysitter_relay_signals(pid_t pid)
{
    size_t i;
    int ret;

    ret = 0;
    for (i = 0; i < g_sigtail; ++i) {
        if (kill(pid, g_signals[i])) {
            ret = -1;
        }
    }

    g_sigtail = 0;
    return ret;
}

static int babysitter_waitpid(pid_t pid, int *status)
{
    pid_t ret;
    assert(status);

    g_child_exited = 0;
    for (;;) {
        ret = waitpid(-1, status, WNOHANG);
        if (ret < 0) {
            if (errno == EINTR) { /* Really? */
                continue;
            } else if (errno == ECHILD) {
                return 0;
            } else {
                return -1;
            }

        } else if (ret == 0) {
            return 0;
        } else if (ret != pid) { /* I don't care about it. */
            continue;
        }

        return 1;
    }
}

static int babysitter_check_watchdog(const babysitter_configure_t *configure,
                                     int64_t last_feed)
{
    int64_t now;
    int64_t elapsed;

    if (!configure->watchdog_timer) {
        return 0;
    }

    now = get_monotonic_timestamp();
    elapsed = (now - last_feed) / 1000000;
    if (elapsed >= configure->watchdog_timer) {
        return -1;
    }

    return 0;
}

/**
 * @retval -1 error occurred.
 * @retval  0 child exited normally.
 * @retval  1 child exited abnormally.
 * @retval  2 child exited abnormally with core dumped.
 */
static int babysitter_wait(const babysitter_configure_t *configure,
                           pid_t pid, int fd, int *status)
{
    struct timespec timeout;
    int64_t last_feed;
    sigset_t empty;
    fd_set rset;
    int ret;

    last_feed = get_monotonic_timestamp();
    memset(&timeout, 0, sizeof(timeout));
    if (sigemptyset(&empty)) {
        return -1;
    }

    for (;;) {
        FD_ZERO(&rset);
        if (fd >= 0) {
            FD_SET(fd, &rset);
        }

        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;
        ret = pselect(fd + 1, &rset, NULL, NULL, &timeout, &empty);
        if (ret < 0 && errno != EINTR) {
            return -1;
        }

        if (g_child_exited) {
            ret = babysitter_waitpid(pid, status);
            if (ret < 0) {
                _exit(EXIT_FAILURE);
            } else if (ret > 0) {
                babysitter_dump_signals();
                if (WIFEXITED(*status)) {
                    return *status == 0 ? 0 : 1;

                } else if (WIFSIGNALED(*status)) {
#ifdef WCOREDUMP
                    if (WCOREDUMP(*status)) {
                        return 2;
                    }
#endif
                    return 1;

                } else { /* WTF? */
                    return -1;
                }
            }
        }

        if (babysitter_check_watchdog(configure, last_feed)) {
            fd = -1;
            if (configure->watchdog_signal) {
                kill(pid, configure->watchdog_signal);
            } else {
                kill(pid, SIGKILL);
            }

        } else if (babysitter_relay_signals(pid)) {
            return -1;
        }

        if (fd < 0) {
            continue;
        }

        ret = babysitter_suck_fd(fd);
        if (ret < 0) {
            fd = -1;

        } else if (ret > 0) {
            last_feed = get_monotonic_timestamp();
        }
    }
}

static void babysitter_destroy_pipes(int pipefd[])
{
    safe_close(pipefd[0]);
    safe_close(pipefd[1]);
    pipefd[0] = -1;
    pipefd[1] = -1;
}

static int babysitter_create_pipes(const babysitter_configure_t *configure,
                                   int pipefd[])
{
    pipefd[0] = -1;
    pipefd[1] = -1;
    if (!configure->watchdog_timer) {
        return 0;
    }

    if (pipe(pipefd)) {
        return -1;
    }

    if (set_non_blocking_mode(pipefd[0])) {
        babysitter_destroy_pipes(pipefd);
        return -1;
    }

    return 0;
}

static int babysitter_delay(int milliseconds)
{
    sigset_t empty;

    if (milliseconds <= 0) {
        return 0;
    }

    if (sigemptyset(&empty)) {
        return -1;
    }

    if (msleep_signal(milliseconds, 0, &empty)) {
        return -1;
    }

    babysitter_dump_signals();
    return 0;
}

static int babysitter_initialize(const babysitter_configure_t *configure,
                                 int pipefd[])
{
    int i;

    if (!configure) {
        errno = EINVAL;
        return -1;
    }

    if (babysitter_create_pipes(configure, pipefd)) {
        return -1;
    }

    if (signals_block_all()) {
        babysitter_destroy_pipes(pipefd);
        return -1;
    }

    for (i = 1; i < NSIG; ++i) {
        if (signals_set_handler(i, babysitter_signal_handler)) {
            if (errno != EINVAL) {
                babysitter_destroy_pipes(pipefd);
                return -1;
            }
        }
    }

    return 0;
}

int babysitter_spawn(const babysitter_configure_t *configure)
{
    int pipefd[2];
    int status;
    pid_t pid;
    int times;
    int delay;
    int ret;

    if (babysitter_initialize(configure, pipefd)) {
        return -1;
    }

    times = 0;
    for (;;) {
        pid = fork();
        if (pid < 0) {
            babysitter_destroy_pipes(pipefd);
            return -1;

        } else if (pid == 0) {
            if (signals_default_all() || signals_unblock_all()) {
                _exit(EXIT_FAILURE);
            }

            g_watchdog_fd = pipefd[1];
            safe_close(pipefd[0]);
            return 0;
        }

        /* Say hello to the babysitter. */

        safe_close(pipefd[1]);
        pipefd[1] = -1;

        ret = babysitter_wait(configure, pid, pipefd[0], &status);
        safe_close(pipefd[0]);
        pipefd[0] = -1;

        delay = configure->normal_delay;
        if (ret < 0) {
            _exit(EXIT_FAILURE);

        } else if (ret == 0) {
            if (!configure->unconditional) {
                break; /* Cool, now quit. */
            }

        } else if (ret == 2) {
            delay = configure->coredump_delay;
        }

        if (configure->spawn) {
            ret = configure->spawn(status, configure->sparam);
            if (ret) {
                break; /* User cancelled. */
            }
        }

        if (babysitter_delay(delay)) {
            babysitter_dump_signals();
            _exit(EXIT_FAILURE);
        }

        babysitter_dump_signals();
        if (babysitter_create_pipes(configure, pipefd)) {
            _exit(EXIT_FAILURE);
        }

        if (configure->restart_times) {
            ++times;
            /* The first spawn is not counted as "restart" so add 1 here. */
            if (times > configure->restart_times) {
                _exit(EXIT_FAILURE);
            }
        }
    }

    babysitter_dump_signals();
    if (configure->cleanup) {
        configure->cleanup(configure->cparam);
    }

    _exit(EXIT_SUCCESS);
}

void babysitter_feed(void)
{
    static const char c = 0;
    ssize_t ret;

    if (g_watchdog_fd < 0) {
        return;
    }

    ret = safe_write(g_watchdog_fd, &c, (size_t)1);
    if (ret < 0) {
        /* Either way, we're going to die. */
        safe_close(g_watchdog_fd);
        g_watchdog_fd = -1;
    }
}
