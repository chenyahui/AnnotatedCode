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

#include <string.h>

extern char **environ;

void envvars_filter(const char *prefix)
{
    size_t len;
    char **p;
    char **q;

    len = strlen(prefix);
    for (p = environ, q = environ; *p; ++p) {
        if (strncmp(prefix, *p, len) == 0) {
            *q++ = *p;
        }
    }

    *q = NULL;
}

void envvars_filter_out(const char *prefix)
{
    size_t len;
    char **p;
    char **q;

    len = strlen(prefix);
    for (p = environ, q = environ; *p; ++p) {
        if (strncmp(prefix, *p, len)) {
            *q++ = *p;
        }
    }

    *q = NULL;
}

void envvars_filter_exact(const char *prefix)
{
    char **p;
    char **q;

    for (p = environ, q = environ; *p; ++p) {
        if (strcmp(prefix, *p) == 0) {
            *q++ = *p;
        }
    }

    *q = NULL;
}
