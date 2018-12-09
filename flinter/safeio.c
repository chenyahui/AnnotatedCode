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

#include "flinter/safeio.h"

#ifdef __unix__

#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "flinter/utility.h"

/** If anything goes wrong, simply return 0 to time out immediately. */
static int safe_calculate_timeout(int64_t start, int milliseconds)
{
    int timeout;
    int64_t now;

    assert(start >= 0);
    assert(milliseconds >= 0);

    now = get_monotonic_timestamp();
    if (now < 0) {
        return 0;
    }

    timeout = (int)((now - start) / 1000000);
    if (timeout < 0 || timeout > milliseconds) {
        return 0;
    }

    return milliseconds - timeout;
}

static ssize_t safe_timed_read_ex(int fd, void *buf, size_t count, int flags,
                                  struct sockaddr *addr, socklen_t *addrlen,
                                  int milliseconds, int all)
{
    struct pollfd fds;
    ssize_t result;
    int64_t start;
    size_t remain;
    ssize_t len;
    int timeout;
    char *ptr;
    int flag;
    int ret;
    int hup;

    if (milliseconds < 0) {
        return safe_read(fd, buf, count);
    }

    start = get_monotonic_timestamp();
    if (start < 0) {
        return -1;
    }

    flag = fcntl(fd, F_GETFL);
    if (flag < 0) {
        return -1;
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(fd, F_SETFL, flag | O_NONBLOCK)) {
            return -1;
        }
    }

    fds.fd = fd;
    fds.events = POLLIN;
    timeout = milliseconds;

    ptr = (char *)buf;
    remain = count;
    result = 0;
    hup = 0;
    for (;;) {
        ret = poll(&fds, 1, timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                if (result == 0) {
                    return -1;
                }
                return result;
            }

            timeout = safe_calculate_timeout(start, milliseconds);
            continue;
        }

        if (ret == 0) { /* timeout */
            errno = ETIMEDOUT;
            if (result == 0) {
                return -1;
            }
            return result;
        }

        if (addr && addrlen) {
            len = recvfrom(fd, ptr, remain, flags, addr, addrlen);
        } else if (flags) {
            len = recv(fd, ptr, remain, flags);
        } else {
            len = read(fd, ptr, remain);
        }

        if (len < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                timeout = safe_calculate_timeout(start, milliseconds);
                continue;
            }

            if (result == 0) {
                return -1;
            }
            return result;
        }

        /* Read 0 byte for peer close. */
        if (len == 0) {
            hup = 1;
            break;
        }

        ptr += len;
        result += len;
        remain -= (size_t)len;
        if (!remain || !all) {
            break;
        }

        timeout = safe_calculate_timeout(start, milliseconds);
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(fd, F_SETFL, flag)) {
            return -1;
        }
    }

    if (result == 0 && !hup) {
        return -1;
    }

    return result;
}

static ssize_t safe_timed_write_ex(int fd, const void *buf, size_t count, int flags,
                                   const struct sockaddr *addr, socklen_t addrlen,
                                   int milliseconds, int all)
{
    struct pollfd fds;
    const char *ptr;
    ssize_t result;
    int64_t start;
    size_t remain;
    ssize_t len;
    int timeout;
    int flag;
    int ret;

    if (milliseconds < 0) {
        return safe_write(fd, buf, count);
    }

    start = get_monotonic_timestamp();
    if (start < 0) {
        return -1;
    }

    flag = fcntl(fd, F_GETFL);
    if (flag < 0) {
        return -1;
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(fd, F_SETFL, flag | O_NONBLOCK)) {
            return -1;
        }
    }

    fds.fd = fd;
    fds.events = POLLOUT | POLLERR | POLLHUP;
    timeout = milliseconds;

    ptr = (const char *)buf;
    remain = count;
    result = 0;
    for (;;) {
        ret = poll(&fds, 1, timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                if (result == 0) {
                    return -1;
                }
                return result;
            }

            timeout = safe_calculate_timeout(start, milliseconds);
            continue;
        }

        if (ret == 0) { /* timeout */
            errno = ETIMEDOUT;
            if (result == 0) {
                return -1;
            }
            return result;
        }

        if (addr && addrlen) {
            len = sendto(fd, ptr, remain, flags, addr, addrlen);
        } else if (flags) {
            len = send(fd, ptr, remain, flags);
        } else {
            len = write(fd, ptr, remain);
        }

        if (len < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                timeout = safe_calculate_timeout(start, milliseconds);
                continue;
            } else if (errno == EPIPE) {
                break;
            }

            if (result == 0) {
                return -1;
            }
            return result;
        }

        ptr += len;
        result += len;
        remain -= (size_t)len;
        if (!remain || !all) {
            break;
        }

        timeout = safe_calculate_timeout(start, milliseconds);
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(fd, F_SETFL, flag)) {
            return -1;
        }
    }

    if (result == 0) {
        return -1;
    }

    return result;
}

int safe_close(int fd)
{
    int ret;
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }

    do {
        ret = close(fd);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

ssize_t safe_read(int fd, void *buf, size_t count)
{
    ssize_t ret;
    if (fd < 0 || !buf) {
        errno = EINVAL;
        return -1;
    }

    do {
        ret = read(fd, buf, count);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

ssize_t safe_write(int fd, const void *buf, size_t count)
{
    ssize_t ret;
    if (fd < 0 || !buf) {
        errno = EINVAL;
        return -1;
    }

    do {
        ret = write(fd, buf, count);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

int safe_listen(int sockfd, int backlog)
{
    /* listen() will not be interrupted by EINTR. */
    return listen(sockfd, backlog);
}

int safe_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int ret;
    if (sockfd < 0) {
        errno = EINVAL;
        return -1;
    }

    do {
        ret = accept(sockfd, addr, addrlen);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

int safe_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;
    if (sockfd < 0 || !addr || addrlen <= 0) {
        errno = EINVAL;
        return -1;
    }

    do {
        ret = connect(sockfd, addr, addrlen);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

ssize_t safe_timed_read(int fd, void *buf, size_t count, int milliseconds)
{
    return safe_timed_read_ex(fd, buf, count, 0, NULL, NULL, milliseconds, 0);
}

ssize_t safe_timed_read_all(int fd, void *buf, size_t count, int milliseconds)
{
    return safe_timed_read_ex(fd, buf, count, 0, NULL, NULL, milliseconds, 1);
}

ssize_t safe_timed_recvfrom(int fd, void *buf, size_t count, int flags,
                            struct sockaddr *addr,
                            socklen_t *addrlen,
                            int milliseconds)
{
    return safe_timed_read_ex(fd, buf, count, flags, addr, addrlen, milliseconds, 0);
}

ssize_t safe_timed_write(int fd, const void *buf, size_t count, int milliseconds)
{
    return safe_timed_write_ex(fd, buf, count, 0, NULL, 0, milliseconds, 0);
}

ssize_t safe_timed_write_all(int fd, const void *buf, size_t count, int milliseconds)
{
    return safe_timed_write_ex(fd, buf, count, 0, NULL, 0, milliseconds, 1);
}

ssize_t safe_timed_sendto(int fd, const void *buf, size_t count, int flags,
                          const struct sockaddr *addr,
                          socklen_t addrlen,
                          int milliseconds)
{
    return safe_timed_write_ex(fd, buf, count, flags, addr, addrlen, milliseconds, 0);
}

int safe_timed_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int milliseconds)
{
    struct pollfd fds;
    int64_t start;
    int timeout;
    int result;
    int flag;
    int ret;

    if (milliseconds < 0) {
        return safe_accept(sockfd, addr, addrlen);
    }

    start = get_monotonic_timestamp();
    if (start < 0) {
        return -1;
    }

    flag = fcntl(sockfd, F_GETFL);
    if (flag < 0) {
        return -1;
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK)) {
            return -1;
        }
    }

    fds.fd = sockfd;
    fds.events = POLLIN;
    timeout = milliseconds;
    result = -1;
    for (;;) {
        ret = poll(&fds, 1, timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                return -1;
            }

            timeout = safe_calculate_timeout(start, milliseconds);
            continue;
        }

        if (ret == 0) { /* timeout */
            errno = ETIMEDOUT;
            return -1;
        }

        ret = safe_accept(sockfd, addr, addrlen);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                timeout = safe_calculate_timeout(start, milliseconds);
                continue;
            }

            return -1;
        }

        result = ret;
        break;
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(sockfd, F_SETFL, flag)) {
            return -1;
        }
    }

    return result;
}

int safe_test_if_connected(int sockfd)
{
    socklen_t len;
    int error;

    len = sizeof(error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }

    if (error) {
        errno = error;
        return -1;
    }

    return 0;
}

int safe_wait_until_connected(int sockfd, int milliseconds)
{
    struct pollfd fds;
    int64_t start;
    int timeout;
    int ret;

    start = get_monotonic_timestamp();
    if (start < 0) {
        return -1;
    }

    fds.fd = sockfd;
    fds.events = POLLOUT | POLLERR | POLLHUP;
    timeout = milliseconds;

    for (;;) {
        ret = poll(&fds, 1, timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                return -1;
            }

            timeout = safe_calculate_timeout(start, milliseconds);
            continue;
        }

        if (ret == 0) { /* timeout */
            errno = ETIMEDOUT;
            return -1;
        }

        if (safe_test_if_connected(sockfd)) {
            return -1;
        }

        break;
    }

    return 0;
}

int safe_timed_connect(int sockfd,
                       const struct sockaddr *addr,
                       socklen_t addrlen,
                       int milliseconds)
{
    int saved_errno;
    int flag;
    int ret;

    if (milliseconds < 0) {
        return safe_connect(sockfd, addr, addrlen);
    }

    flag = fcntl(sockfd, F_GETFL);
    if (flag < 0) {
        return -1;
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK)) {
            return -1;
        }
    }

    /* connect(2) might change errno to EINPROGRESS, but we don't want it. */
    saved_errno = errno;
    ret = connect(sockfd, addr, addrlen);
    if (ret) {
        if (errno != EINPROGRESS) {
            return -1;
        }

        errno = saved_errno;
        if (safe_wait_until_connected(sockfd, milliseconds)) {
            return -1;
        }

    } else {
        errno = saved_errno;
    }

    if (!(flag & O_NONBLOCK)) {
        if (fcntl(sockfd, F_SETFL, flag)) {
            return -1;
        }
    }

    return 0;
}

int safe_resolve_and_connect(int domain, int type, int protocol,
                             const char *host, uint16_t port,
                             int *sockfd)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char buffer[16];
    int ret;
    int fd;

    assert(host && *host);
    assert(sockfd);

    if (port == 0) {
        errno = EINVAL;
        return -1;
    }

    ret = snprintf(buffer, sizeof(buffer), "%u", port);
    if (ret < 0 || (size_t)ret >= sizeof(buffer)) {
        errno = EINVAL;
        return -1;
    }

    fd = socket(domain, type, protocol);
    if (fd < 0) {
        return -1;
    }

    if (set_non_blocking_mode(fd)) {
        safe_close(fd);
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_protocol = protocol;
    hints.ai_socktype = type;
    hints.ai_family = domain;

    if (getaddrinfo(host, buffer, &hints, &res)) {
        safe_close(fd);
        return -1;
    }

    ret = safe_connect(fd, res->ai_addr, res->ai_addrlen);
    if (ret < 0) {
        if (errno == EINPROGRESS) {
            ret = 1;
        } else {
            freeaddrinfo(res);
            safe_close(fd);
            return -1;
        }
    }

    freeaddrinfo(res);
    *sockfd = fd;
    return ret;
}

#endif /* __unix__ */
