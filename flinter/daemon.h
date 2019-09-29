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

#ifndef FLINTER_DAEMON_H
#define FLINTER_DAEMON_H

#if defined(__unix__) || defined(__MACH__)

#ifdef __cplusplus
extern "C" {
#endif

#define DAEMON_FAST_RETURN          0x00000001
#define DAEMON_NO_CD_TO_ROOT        0x00000002
#define DAEMON_ATTACHED_SESSION     0x00000004

#define DAEMON_FLAGS_MASK           0x00000007

struct _babysitter_configure_t;

/**
 * Create a instance of a daemon.
 *
 * @warning This method can only be called single-threaded, not only locked.
 * @warning Caller MUST NOT block SIGCHLD.
 *          Caller MUST ignore SIGCHLD or handle it if fast returning.
 *
 * @param fd_to_start_closing the first fd to close, if there're some important fd
 *        that you want to keep in daemon, use this one.
 *
 * @param flags Bitwise OR of DAEMON_xxx flags.
 *        DAEMON_NO_CD_TO_ROOT      don't change current directory to root.
 *        DAEMON_FAST_RETURN        don't wait for the second fork(2), might be dangerous.
 *        DAEMON_ATTACHED_SESSION   don't call setsid(2) nor fork(2) again, just close fd
 *                                  and chdir. Implicits DAEMON_FAST_RETURN.
 *
 * @return Like fork(), but no pid is returned.
 * @retval -1 the calling process returns, failed to start the daemon.
 * @retval  1 the calling process returns, daemon started.
 * @retval  0 the daemon process returns.
 */
extern int daemon_spawn(int fd_to_start_closing, int flags);

typedef int (*daemon_main_t)(int argc, char **);
typedef struct {
    daemon_main_t                           callback;
    const char                             *full_name;
    const char                             *version;
    const char                             *name;
    const char                             *author;
    const char                             *mail;
    int                                     fd_to_start_closing;
    int                                     flags;
    const struct _babysitter_configure_t   *babysitter;
} daemon_configure_t;

/**
 * An ordinary daemon can simply delegate main() to this one.
 */
extern int daemon_main(const daemon_configure_t *configure, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* defined(__unix__) || defined(__MACH__) */

#endif /* FLINTER_DAEMON_H */
