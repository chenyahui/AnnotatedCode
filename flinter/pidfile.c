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

#include "flinter/pidfile.h"

#ifdef __unix__

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "flinter/safeio.h"

#define PIDFILE_LENGTH 11

pid_t pidfile_check(const char *filename)
{
    struct flock lock;
    int fd;

    if (!filename || !*filename) {
        errno = EINVAL;
        return -1;
    }

    fd = open(filename, O_RDWR | O_NOCTTY, DEFFILEMODE);
    if (fd < 0) {
        return -1;
    }

    /* Lock every single byte and set close-on-exec. */
    memset(&lock, 0, sizeof(lock));
    lock.l_whence = SEEK_SET;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_GETLK, &lock)) {
        safe_close(fd);
        return -1;
    }

    safe_close(fd);
    if (lock.l_type == F_UNLCK) {
        return 0;
    }

    return lock.l_pid;
}

int pidfile_open(const char *filename)
{
    struct flock lock;
    char buf[32];
    long arg;
    int ret;
    int fd;

    if (!filename || !*filename) {
        errno = EINVAL;
        return -1;
    }

    if (snprintf(buf, sizeof(buf), "%10d\n", getpid()) != PIDFILE_LENGTH) {
        errno = EACCES;
        return -1;
    }

    fd = open(filename, O_RDWR | O_CREAT | O_NOCTTY, DEFFILEMODE);
    if (fd < 0) {
        return -1;
    }

    /* Lock every single byte and set close-on-exec. */
    memset(&lock, 0, sizeof(lock));
    lock.l_whence = SEEK_SET;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_len = 0;

    arg = FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, arg)) {
        safe_close(fd);
        return -1;
    }

    if (fcntl(fd, F_SETLK, &lock)) {
        ret = errno;
        safe_close(fd);
        if (ret == EACCES || ret == EAGAIN) {
            return -2;
        }

        return -1;
    }

    /* Now write our information. */
    if (lseek(fd, SEEK_SET, 0)) {
        unlink(filename);
        safe_close(fd);
        return -1;
    }

    if (safe_timed_write_all(fd, buf, PIDFILE_LENGTH, 1000) != PIDFILE_LENGTH) {
        unlink(filename);
        safe_close(fd);
        return -1;
    }

    /* Truncate (or grow) it. */
    if (ftruncate(fd, PIDFILE_LENGTH)) {
        safe_close(fd);
        return -1;
    }

    return fd;
}

int pidfile_close(int fd, const char *filename)
{
    int ret;

    if (fd < 0 || !filename || !*filename) {
        errno = EINVAL;
        return -1;
    }

    ret = pidfile_validate(fd, filename);
    if (ret == 0) { /* Good, we still have the lock. */
        ret = unlink(filename);
    }

    /* Whether we have the lock or not, the fd is ours. */
    ret |= safe_close(fd);
    return ret;
}

int pidfile_validate(int fd, const char *filename)
{
    struct stat fst;
    struct stat sst;

    if (fd < 0 || !filename || !*filename) {
        errno = EINVAL;
        return -1;
    }

    if (fstat(fd, &fst) || lstat(filename, &sst)) {
        return -1;
    }

    if (fst.st_dev != sst.st_dev ||
        fst.st_ino != sst.st_ino ){

        errno = ENOENT;
        return -1;
    }

    return 0;
}

#endif /* __unix__ */
