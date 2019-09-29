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

#ifndef FLINTER_UTILITY_H
#define FLINTER_UTILITY_H

#include <sys/select.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Convert a non-negative integer out of a string. -1 for error. */
extern int32_t atoi32(const char *value);

/** Initialize pesudo random number generator aka srand(3). */
extern unsigned int randomize_r(void);

/** Initialize pesudo random number generator aka srand(3). */
extern void randomize(void);

/**
 * rand() % range can cause slightly unequal distribution.
 * @return equally distributed integer from [0,range]
 * @warning range must be lesser or equal to RAND_MAX.
 */
extern int ranged_rand(int range);

/**
 * rand_r() % range can cause slightly unequal distribution.
 * @return equally distributed integer from [0,range]
 * @warning range must be lesser or equal to RAND_MAX.
 */
extern int ranged_rand_r(int range, unsigned int *seedp);

/**
 * Get wall clock timestamp in nanoseconds since epoch.
 * Don't worry, it won't overflow.
 */
extern int64_t get_wall_clock_timestamp(void);

/**
 * Get monotonic timestamp in nanoseconds since some time unknown.
 * Don't worry, it won't overflow.
 */
extern int64_t get_monotonic_timestamp(void);

/**
 * Get monotonic timestamp in seconds since some time unknown.
 * Wall clock counterpart? Try time(3).
 */
extern time_t get_monotonic_time(void);

/**
 * Clean a path, like " //a/b///c/" to "/a/b/c".
 * @param path can't be NULL, empty string treated as "."
 * @param trim whether to trim path before processing.
 * @return NULL if terribly wrong, or cleaned path. Caller free the buffer via free(3).
 */
extern char *make_clean_path(const char *path, int trim);

#if defined(__unix__) || defined(__MACH__)
extern int64_t get_current_thread_id(void);

/** Add fd to set, return new max_fd */
extern int add_to_set(int max_fd, int fd, fd_set *set);

/** Set file descriptor to non-blocking mode. */
extern int set_non_blocking_mode(int fd);

/** Set file descriptor to blocking mode. */
extern int set_blocking_mode(int fd);

/** Allow socket to re-use binding addresses. */
extern int set_socket_reuse_address(int sockfd);

/** Allow TCP defered accept. */
extern int set_tcp_defer_accept(int sockfd);

/** Allow TCP no delay. */
extern int set_tcp_nodelay(int sockfd);

/** Allow socket to send broadcast packets. */
extern int set_socket_broadcast(int sockfd);

/** Allow socket to send keep-alive packets. */
extern int set_socket_keepalive(int sockfd);

/** Set CLOEXEC on fd. */
extern int set_close_on_exec(int fd);

/** Enable timer which uses SIGALRM to notify. <=0 to disable. */
extern int set_alarm_timer(int milliseconds);

/**
 * Change maximum files the process can open.
 * @return actual limit after calling. -1 for error.
 */
extern ssize_t set_maximum_files(size_t nofile);

#endif /* defined(__unix__) || defined(__MACH__) */

#ifdef __cplusplus
}
#endif

#endif /* FLINTER_UTILITY_H */
