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

#include "flinter/mkdirs.h"

#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/** Same function as "mkdir -p" */
int mkdirs(const char *pathname, mode_t mode)
{
    char *copy;
    char *path;
    char *next;
    char *ptr;
    char *prev = NULL;
    size_t len;
    ssize_t pos;
    int err;
    int ret;

    if (!pathname || !*pathname) {
        errno = EINVAL;
        return -1;
    }

    copy = strdup(pathname);
    if (!copy) {
        errno = ENOMEM;
        return -1;
    }

    path = strdup(copy);
    if (!path) {
        free(copy);
        errno = ENOMEM;
        return -1;
    }

    next = strtok_r(path, "/", &ptr);
    pos = (ssize_t)(next - path - 1);

    while (next) {
        len = strlen(next);
        if (pos >= 0) {
            copy[pos] = '/';
        }

        if (prev) {
            pos += (next - prev) - 1 + (ssize_t)len;
        } else {
            pos += (ssize_t)len + 1;
        }

        copy[pos] = '\0';
        prev = next + len - 1;

        ret = mkdir(copy, mode);
        if (ret) {
            if (errno != EEXIST) {
                err = errno;
                free(path);
                free(copy);
                errno = err;
                return ret;
            }
        }

        next = strtok_r(NULL, "/", &ptr);
    }

    free(path);
    free(copy);
    return 0;
}
