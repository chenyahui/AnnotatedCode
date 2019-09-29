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

#include "flinter/rmdirs.h"

#define _GNU_SOURCE 1
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

/*
 * Recent enough Linux kernel supports openat() family methods which are
 * preferred when available, use old school method otherwise.
 */
#define RMDIRS_COMPAT (!(defined(AT_FDCWD) && HAVE_OPENAT && HAVE_FDOPENDIR))

/*
 * On linux, there is a kernel bug that will lead inode numbers in tmpfs
 * overflowed to 0, which cause readdir_r() skip that file. And then, rmdirs
 * will failed since the directory is not empty. So here I use syscall
 * getdents() to avoid this bug.
 */
#define RMDIRS_GETDENTS defined(SYS_getdents)

#if RMDIRS_GETDENTS
static volatile sig_atomic_t g_disable_getdents = 0;
struct linux_dirent {
    ino_t       d_ino;
    off_t       d_off;
    uint16_t    d_reclen;
    char        d_name[1];
}; /* struct linux_dirent */

#if RMDIRS_COMPAT
static int rmdirs_with_getdents(const char *pathname)
#else
static int rmdirs_with_getdents(int dirfd, const char *name)
#endif
{
    struct linux_dirent *d;
    char dbuf[4096];
    struct stat st;
    int recursive;
    char type;
    long dlen;
    int ret;
    int fd;
    int i;

#if RMDIRS_COMPAT
    char buffer[PATH_MAX + 1 + NAME_MAX + 1];
    size_t offset;

    offset = strlen(pathname);
    memcpy(buffer, pathname, offset);
    buffer[offset++] = '/';
    fd = open(pathname, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
#else
    fd = openat(dirfd, name, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
#endif
    if (fd < 0) {
        return -1;
    }

    ret = -1;
    for (;;) {
        dlen = syscall(SYS_getdents, fd, dbuf, sizeof(dbuf));
        if (dlen < 0) {
            if (errno == ENOSYS) {
                ret = 1;
            }

            break;

        } else if (dlen == 0) {
            ret = 0;
            break;
        }

        i = 0;
        do {
            d = (struct linux_dirent *)(dbuf + i);
            i += d->d_reclen;
            if (strcmp(d->d_name, "." ) == 0 ||
                strcmp(d->d_name, "..") == 0 ){
                continue;
            }

#if RMDIRS_COMPAT
            strcpy(buffer + offset, d->d_name);
#endif
            recursive = 0;
            type = *((char *)d + d->d_reclen - 1);
            if (type == DT_UNKNOWN) {
#if RMDIRS_COMPAT
                if (lstat(buffer, &st)) {
#else
                if (fstatat(fd, d->d_name, &st, AT_SYMLINK_NOFOLLOW)) {
#endif
                    break;
                }

                if (S_ISDIR(st.st_mode)) {
                    recursive = 1;
                }

            } else if (type == DT_DIR) {
                recursive = 1;
            }

            if (!recursive) {
#if RMDIRS_COMPAT
                if (unlink(buffer)) {
#else
                if (unlinkat(fd, d->d_name, 0)) {
#endif
                    break;
                }

                continue;
            }

#if RMDIRS_COMPAT
            if (rmdirs_with_getdents(buffer) || rmdir(buffer)) {
#else
            if (rmdirs_with_getdents(fd, d->d_name)   ||
                unlinkat(fd, d->d_name, AT_REMOVEDIR) ){
#endif
                break;
            }

        } while (i < dlen);
    }

    close(fd);
    return ret;
}
#endif /* RMDIRS_GETDENTS */

#if RMDIRS_COMPAT
static int rmdirs_with_readdir(const char *pathname)
#else
static int rmdirs_with_readdir(int dirfd, const char *name)
#endif
{
    struct dirent dirent;
    struct dirent *p;
    struct dirent *d;
    struct stat st;
    DIR *dir;
    int ret;

#if RMDIRS_COMPAT
    char buffer[PATH_MAX + 1 + NAME_MAX + 1];
    size_t offset;

    offset = strlen(pathname);
    memcpy(buffer, pathname, offset);
    buffer[offset++] = '/';

    dir = opendir(pathname);
#else
    int fd;

    fd = openat(dirfd, name, O_RDONLY | O_NOCTTY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) {
        return -1;
    }

    dir = fdopendir(fd);
#endif

    if (!dir) {
        return -1;
    }

    d = &dirent;
    p = NULL;
    ret = -1;

    for (;;) {
        readdir_r(dir, d, &p);
        if (!p) {
            ret = 0;
            break;
        }

        if (strcmp(d->d_name, "." ) == 0 ||
            strcmp(d->d_name, "..") == 0 ){
            continue;
        }

#if RMDIRS_COMPAT
        strcpy(buffer + offset, dirent.d_name);
        if (lstat(buffer, &st)) {
#else
        if (fstatat(fd, d->d_name, &st, AT_SYMLINK_NOFOLLOW)) {
#endif
            break;
        }

        if (S_ISDIR(st.st_mode)) {
#if RMDIRS_COMPAT
            if (rmdirs_with_readdir(buffer) || rmdir(buffer)) {
#else
            if (rmdirs_with_readdir(fd, d->d_name)    ||
                unlinkat(fd, d->d_name, AT_REMOVEDIR) ){
#endif
                break;
            }
        } else {
#if RMDIRS_COMPAT
            if (unlink(buffer)) {
#else
            if (unlinkat(fd, d->d_name, 0)) {
#endif
                break;
            }
        }
    }

    closedir(dir);
    return ret;
}

static int rmdirs_internal(const char *pathname, int leave_dir_along)
{
    struct stat st;
    int ret;

#if RMDIRS_COMPAT
    if (lstat(pathname, &st)) {
#else
    if (fstatat(AT_FDCWD, pathname, &st, AT_SYMLINK_NOFOLLOW)) {
#endif
        if (errno == ENOENT) {
            return 0;
        }

        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        if (leave_dir_along) {
            return 0;
        }

#if RMDIRS_COMPAT
        return unlink(pathname);
#else
        return unlinkat(AT_FDCWD, pathname, 0);
#endif
    }

    do {

#if RMDIRS_GETDENTS
        if (!g_disable_getdents) {
#if RMDIRS_COMPAT
            ret = rmdirs_with_getdents(pathname);
#else
            ret = rmdirs_with_getdents(AT_FDCWD, pathname);
#endif
            if (ret < 0) {
                return -1;
            } else if (ret > 0) {
                g_disable_getdents = 1;
            } else {
                break;
            }
        }
#endif /* RMDIRS_GETDENTS */

#if RMDIRS_COMPAT
        ret = rmdirs_with_readdir(pathname);
#else
        ret = rmdirs_with_readdir(AT_FDCWD, pathname);
#endif
        if (ret) {
            return -1;
        }

    } while (0);

    if (leave_dir_along) {
        return 0;
    }

#if RMDIRS_COMPAT
    return rmdir(pathname);
#else
    return unlinkat(AT_FDCWD, pathname, AT_REMOVEDIR);
#endif
}

int rmdirs(const char *pathname)
{
    return rmdirs_internal(pathname, 0);
}

int rmdirs_inside(const char *pathname)
{
    return rmdirs_internal(pathname, 1);
}
