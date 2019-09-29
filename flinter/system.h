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

#ifndef FLINTER_SYSTEM_H
#define FLINTER_SYSTEM_H

#if defined(__unix__) || defined(__MACH__)

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __popen_t *popen_t;

/**
 * Like system(3) but allow timeout, kill when timed out.
 * @param string command to executor
 * @param timeout in milliseconds, <0 for infinite.
 * @return same as system(3), but you'll see SIGKILL returned if timed out.
 * @warning unlink system(3), all children are within a new process group to kill.
 * @warning don't set SIGCHLD to SIG_IGN since SIGKILL might be sent to wrong process.
 */
extern int system_timed(const char *string, int timeout);

/**
 * Like system(3) but allow timeout, keep the process when timed out.
 * @param string command to executor
 * @param timeout in milliseconds, <0 for infinite.
 * @return <0 and errno=ETIMEDOUT, or the same as system(3).
 */
extern int system_timed_nokill(const char *string, int timeout);

/**
 * Like popen(3) but allow timeout.
 * @param string command to executor
 * @param handle return a handle which is later used in pclose_timed().
 * @return STDIO fd, or <0 if there's an error. pclose_timed() will close the fd.
 * @warning unlink popen(3), all children are within a new process group to kill.
 * @warning don't set SIGCHLD to SIG_IGN since SIGKILL might be sent to wrong process.
 */
extern int popen_timed(const char *string, popen_t *handle);

/**
 * Like pclose(3) but allow timeout, kill when timed out.
 * @param handle previous instance to close.
 * @param timeout in milliseconds, <0 for infinite.
 * @return <0 if there's an error, or same as waitpid(2).
 * @warning unlink pclose(3), all children are within a new process group to kill.
 * @warning don't set SIGCHLD to SIG_IGN since SIGKILL might be sent to wrong process.
 */
extern int pclose_timed(popen_t handle, int timeout);

/**
 * If you fork(2) or clone(2) a child, you can wait for or to kill it.
 * @param pid to wait for.
 * @param status store exit status, can be NULL.
 * @param options for waiting.
 * @param timeout in milliseconds, <0 for infinite.
 * @return same as waitpid(2), but you'll see SIGKILL returned if timed out.
 * @warning don't set SIGCHLD to SIG_IGN since SIGKILL might be sent to wrong process.
 */
extern pid_t waitpid_timed(pid_t pid, int *status, int options, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* defined(__unix__) || defined(__MACH__) */

#endif /* FLINTER_SYSTEM_H */
