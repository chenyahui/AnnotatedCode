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

#ifndef FLINTER_SAFEIO_H
#define FLINTER_SAFEIO_H

#if defined(__unix__) || defined(__MACH__)

#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** close() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern int safe_close(int fd);

/** read() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern ssize_t safe_read(int fd, void *buf, size_t count);

/** write() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern ssize_t safe_write(int fd, const void *buf, size_t count);

/** listen() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern int safe_listen(int sockfd, int backlog);

/** accept() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern int safe_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

/** connect() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern int safe_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

/**
 * read(once) without interrupted by EINTR
 * @return bytes read. On error (including timed out), -1 is returned, and errno is set
 *         appropriately.
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern ssize_t safe_timed_read(int fd, void *buf, size_t count, int milliseconds);

/**
 * read(once) without interrupted by EINTR
 * @return bytes read. On error (including timed out), -1 is returned, and errno is set
 *         appropriately.
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern ssize_t safe_timed_recvfrom(int fd, void *buf, size_t count, int flags,
                                   struct sockaddr *addr,
                                   socklen_t *addrlen,
                                   int milliseconds);

/**
 * write(once) without interrupted by EINTR
 * @return bytes written. On error (including timed out), -1 is returned, and errno is set
 *         appropriately.
 * @warning SIGPIPE should be ignored or blocked before calling.
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern ssize_t safe_timed_write(int fd, const void *buf, size_t count, int milliseconds);

/**
 * write(once) without interrupted by EINTR
 * @return bytes written. On error (including timed out), -1 is returned, and errno is set
 *         appropriately.
 * @warning SIGPIPE should be ignored or blocked before calling.
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern ssize_t safe_timed_sendto(int fd, const void *buf, size_t count, int flags,
                                 const struct sockaddr *addr,
                                 socklen_t addrlen,
                                 int milliseconds);

/**
 * read(all the buffer) without interrupted by EINTR
 * @return Bytes read. On error, -1 is returned, and errno is set appropriately.
 *
 *         Note that if there're bytes read when errors occur (including read error,
 *         timed out, peer close or connection reset e.g.), the number of bytes will be
 *         returned, while errno is set appropriately. To detect such terms, set
 *         errno to 0 before calling. If nothing was read, -1 is returned.
 *
 *         As always, returning 0 means no byte read and peer close.
 *
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern ssize_t safe_timed_read_all(int fd, void *buf, size_t count, int milliseconds);

/**
 * write(all the buffer) without interrupted by EINTR
 * @return Bytes written. On error, -1 is returned, and errno is set appropriately.
 *
 *         Note that if there're bytes written when errors occur (including write error,
 *         timed out, peer close or connection reset e.g.), the number of bytes will be
 *         returned, while errno is set appropriately. To detect such terms, set
 *         errno to 0 before calling. If nothing was written, -1 is returned.
 *
 * @warning SIGPIPE should be ignored or blocked before calling.
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern ssize_t safe_timed_write_all(int fd, const void *buf, size_t count, int milliseconds);

/** accept() without interrupted by EINTR */
/** @warning use with caution: EINTR will be swallowed if sockfd is blocking. */
extern int safe_timed_accept(int sockfd, struct sockaddr *addr,
                             socklen_t *addrlen, int milliseconds);

/** connect() without interruptted by EINTR. */
/** @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not. */
extern int safe_timed_connect(int sockfd,
                              const struct sockaddr *addr,
                              socklen_t addrlen,
                              int milliseconds);

/**
 * @warning only call this method if sockfd was connected with EINPROGRESS and it's now
 *          writable for the first time.
 */
extern int safe_test_if_connected(int sockfd);

/**
 * @warning only call this method if sockfd was connected with EINPROGRESS.
 * @warning use with caution: EINTR will be swallowed whether sockfd is blocking or not.
 */
extern int safe_wait_until_connected(int sockfd, int milliseconds);

/**
 * Create a non-blocking socket of given domain, type and protocol, then connect() to
 * given host and port. The resulting socket is placed in sockfd.
 *
 * @retval <0 error
 * @retval  0 connected (rarely, unix socket?)
 * @retval >0 connecting in progress.
 */
extern int safe_resolve_and_connect(int domain, int type, int protocol,
                                    const char *host, uint16_t port,
                                    int *sockfd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* defined(__unix__) || defined(__MACH__) */

#endif /* FLINTER_SAFEIO_H */
