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

#ifndef FLINTER_PIDFILE_H
#define FLINTER_PIDFILE_H

#if defined(__unix__) || defined(__MACH__)

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open and lock a PID file.
 * The lock will be automatically removed on fork(2) or vfork(2).
 * The fd will be automatically closed on execve(2).
 * @return fd to the file, or -1 on error, -2 for locked by others.
 */
extern int pidfile_open(const char *filename);

/**
 * Check if the lock file we're holding is still the one on the disk.
 * Somebody might have wiped the file from the disk.
 * Content of file is not validated.
 */
extern int pidfile_validate(int fd, const char *filename);

/**
 * Close a PID file.
 */
extern int pidfile_close(int fd, const char *filename);

/**
 * @retval -1 if there're errors.
 * @retval  0 if pidfile is not locked.
 * @retval >0 pid of the processes locking it.
 */
extern pid_t pidfile_check(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* defined(__unix__) || defined(__MACH__) */

#endif /* FLINTER_PIDFILE_H */
