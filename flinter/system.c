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

#include "flinter/system.h"

#ifdef __unix__

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "flinter/msleep.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

#if STDIN_FILENO > STDOUT_FILENO
# if STDIN_FILENO > STDERR_FILENO
#  define MAX_STANDARD_FILENO STDIN_FILENO
# else
#  define MAX_STANDARD_FILENO STDERR_FILENO
# endif
#else
# if STDOUT_FILENO > STDERR_FILENO
#  define MAX_STANDARD_FILENO STDOUT_FILENO
# else
#  define MAX_STANDARD_FILENO STDERR_FILENO
# endif
#endif

extern char **environ;

static void system_timed_report(int fd, int ret)
{
    ssize_t result = safe_write(fd, &ret, sizeof(int));
    safe_close(fd);
    if (result != sizeof(int)) {
        _exit(EXIT_FAILURE);
    } else {
        _exit(EXIT_SUCCESS);
    }
}

static int64_t get_deadline(int timeout)
{
    int64_t deadline;

    assert(timeout >= 0);

    deadline = get_monotonic_timestamp();
    if (deadline < 0) {
        return -1;
    }

    deadline += (int64_t)timeout * 1000000;
    return deadline;
}

static int wait_for_child_or_kill(pid_t pid, int options, int64_t deadline)
{
    int64_t now;
    pid_t child;
    int ret;

    for (;;) {
        child = waitpid(pid, &ret, options | WNOHANG);
        if (child < 0) {
            return -1;
        } else if (child == pid) {
            return ret;
        } else if (child != 0) {
            errno = EACCES;
            return -1;
        }

        now = get_monotonic_timestamp();
        if (now < 0) {
            return -1;
        }

        if (now < deadline) {
            msleep(100);
            continue;
        }

        /* Dirty work. */
        if (killpg(pid, SIGKILL)) {
            if (errno == ESRCH) {
                msleep(10);
                continue;
            }
            return -1;
        }

        if (killpg(pid, SIGCONT)) {
            if (errno == ESRCH) {
                msleep(10);
                continue;
            }
            return -1;
        }

        msleep(10);
    }
}

int system_timed(const char *string, int timeout)
{
    /* Comform to ANSI C, use /bin/sh to start programs. */

    pid_t pid;
    int64_t deadline;
    char *const argv[] = {
        "sh",
        "-c",
        (char *)string, /* Don't worry about this conversion. */
        NULL
    };

    if (timeout < 0) {
        return system(string);
    }

    /* Comform to POSIX.2, always assume that the shell is present. */
    if (!string) {
        return 1;
    }

    deadline = get_deadline(timeout);
    if (deadline < 0) {
        return -1;
    }

    /* Although execve() immediately, vfork() can break a register based `deadline`. */
    pid = fork(); /* execve() immediately, vfork() capable. */
    if (pid < 0) {
        return -1;
    }

    if (!pid) {
        if (setpgid(0, 0)) {
            _exit(127);
        }

        execve("/bin/sh", argv, environ);
        _exit(127);
    }

    return wait_for_child_or_kill(pid, 0, deadline);
}

int system_timed_nokill(const char *string, int timeout)
{
    /* Comform to ANSI C, use /bin/sh to start programs. */

    int ret;
    int flag;
    int fd[2];
    pid_t pid;
    pid_t child;
    int64_t now;
    ssize_t length;
    int64_t deadline;
    char *const argv[] = {
        "sh",
        "-c",
        (char *)string, /* Don't worry about this conversion. */
        NULL
    };

    if (timeout < 0) {
        return system(string);
    }

    /* Comform to POSIX.2, always assume that the shell is present. */
    if (!string) {
        return 1;
    }

    deadline = get_deadline(timeout);
    if (deadline < 0) {
        return -1;
    }

    if (pipe(fd)) {
        return -1;
    }

    if ((flag = fcntl(fd[1], F_GETFD))< 0               ||
        fcntl(fd[1], F_SETFD, flag | FD_CLOEXEC) < 0    ||
        (pid = fork()) < 0                              ){

        ret = errno;
        safe_close(fd[0]);
        safe_close(fd[1]);
        errno = ret;
        return -1;
    }

    if (pid) { /* The calling process. */
        safe_close(fd[1]);
        child = waitpid(pid, &ret, 0);
        if (child < 0) {
            safe_close(fd[0]);
            return -1;
        } else if (ret != EXIT_SUCCESS) {
            safe_close(fd[0]);
            errno = EACCES;
            return -1;
        }

        length = safe_read(fd[0], &ret, sizeof(int));
        if (length < 0) {
            ret = errno;
            safe_close(fd[0]);
            errno = ret;
            return -1;

        } else if (length >= 0 && length != sizeof(int)) {
            safe_close(fd[0]);
            errno = EACCES;
            return -1;
        }

        safe_close(fd[0]);
        if (ret < 0) {
            errno = -ret;
            return -1;
        } else {
            return ret;
        }
    }

    signal(SIGCHLD, SIG_DFL);
    safe_close(fd[0]);

    /* Although execve() immediately, vfork() can break a register based `deadline`. */
    pid = fork(); /* execve() immediately, vfork() capable. */
    if (pid < 0) {
        system_timed_report(fd[1], -errno);
    }

    if (!pid) {
        execve("/bin/sh", argv, environ);
        _exit(127);
    }

    for (;;) {
        child = waitpid(pid, &ret, WNOHANG);
        if (child < 0) {
            system_timed_report(fd[1], -errno);
        } else if (child == pid) {
            system_timed_report(fd[1], ret);
        } else if (child != 0) {
            system_timed_report(fd[1], -EACCES);
        }

        now = get_monotonic_timestamp();
        if (now < 0) {
            system_timed_report(fd[1], -EACCES);
        }

        if (now < deadline) {
            msleep(100);
            continue;
        }

        /* As we return, the child will be taken by init(1). */
        system_timed_report(fd[1], -ETIMEDOUT);
    }
}

struct __popen_t {
    int sockfd;
    pid_t pid;
}; /* struct __popen_t */

int popen_timed(const char *string, struct __popen_t **handle)
{
    int ret;
    int sv[2];
    pid_t pid;
    struct __popen_t *h;
    char *const argv[] = {
        "sh",
        "-c",
        (char *)string, /* Don't worry about this conversion. */
        NULL
    };


    if (!string || !*string || !handle) {
        errno = EINVAL;
        return -1;
    }

    h = (struct __popen_t *)malloc(sizeof(struct __popen_t));
    if (!h) {
        errno = ENOMEM;
        return -1;
    }

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv)) {
        ret = errno;
        free(h);
        errno = ret;
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        ret = errno;
        safe_close(sv[1]);
        safe_close(sv[0]);
        free(h);
        errno = ret;
        return -1;
    }

    if (pid) {
        safe_close(sv[1]);
        h->pid = pid;
        h->sockfd = sv[0];
        *handle = h;
        return sv[0];
    }

    if (setpgid(0, 0)) {
        _exit(127);
    }

    safe_close(sv[0]);
    dup2(sv[1], STDIN_FILENO );
    dup2(sv[1], STDOUT_FILENO);
    dup2(sv[1], STDERR_FILENO);
    if (sv[1] > MAX_STANDARD_FILENO) {
        safe_close(sv[1]);
    }

    execve("/bin/sh", argv, environ);
    _exit(127);
}

int pclose_timed(struct __popen_t *handle, int timeout)
{
    int ret;
    pid_t pid;
    pid_t child;
    int64_t deadline;

    if (!handle) {
        errno = EINVAL;
        return -1;
    }

    if (handle->sockfd >= 0) {
        safe_close(handle->sockfd);
    }

    pid = handle->pid;
    free(handle);
    if (pid <= 1) {
        free(handle);
        errno = EINVAL;
        return -1;
    }

    if (timeout < 0) {
        child = waitpid(pid, &ret, 0);
        if (child != pid) {
            return -1;
        }
        return 0;
    }

    deadline = get_deadline(timeout);
    if (deadline < 0) {
        return -1;
    }

    return wait_for_child_or_kill(pid, 0, deadline);
}

pid_t waitpid_timed(pid_t pid, int *status, int options, int timeout)
{
    int64_t deadline;
    int ret;

    if (timeout < 0) {
        return waitpid(pid, status, options);
    }

    deadline = get_deadline(timeout);
    if (deadline < 0) {
        return -1;
    }

    ret = wait_for_child_or_kill(pid, options, deadline);
    if (ret < 0) {
        return -1;
    }

    if (status) {
        *status = ret;
    }

    return pid;
}

#endif /* __unix__ */
