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

#include "flinter/hash.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

#include "flinter/msleep.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

#include "config.h"
#if HAVE_OPENSSL_OPENSSLV_H
#include <openssl/md5.h>
#include <openssl/sha.h>
#endif

/* Avoid EVP since it requires explicit initialization. */
typedef int (*hash_init_t)(void *);
typedef int (*hash_final_t)(unsigned char *, void *);
typedef int (*hash_update_t)(void *, const void *, unsigned long);

static int do_hash(hash_init_t hash_init,
                   hash_update_t hash_update,
                   hash_final_t hash_final,
                   void *hash_context,
                   size_t hash_length,
                   const char *filename,
                   int limit_rate,
                   int milliseconds,
                   char *output)
{
    static const char TABLE[] = "0123456789abcdef";
    static const int64_t TICK = 20000000LL; /**< 20ms */
    static const int TICK_MS = 20LL;

    int64_t deadline;
    int64_t start;
    int64_t turn;
    int64_t now;

    ssize_t turn_remain;
    ssize_t turn_max;
    ssize_t turn_now;

    unsigned char buffer[8192];
    ssize_t ret;
    int result;
    size_t i;
    int tick;
    char *p;
    int fd;

    if (!filename || !*filename || !output) {
        errno = EINVAL;
        return -1;
    }

    start = get_monotonic_timestamp();

    deadline = -1;
    if (milliseconds > 0) {
        deadline = get_monotonic_timestamp();
        deadline += 1000000LL * milliseconds;
    }

    if (limit_rate < 1024) {
        turn_max = -1;
    } else {
        turn_max = limit_rate / (1000000000LL / TICK);
    }

    if (!hash_init(hash_context)) {
        errno = EACCES;
        return -1;
    }

    fd = open(filename, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    turn_remain = turn_max;
    result = -1;
    for (;;) {
        turn = get_monotonic_timestamp();
        if (milliseconds > 0 && turn >= deadline) {
            errno = ETIMEDOUT;
            break;
        }

        turn_now = sizeof(buffer);
        if (turn_max >= 0 && (size_t)turn_remain < sizeof(buffer)) {
            turn_now = turn_remain;
        }

        ret = safe_timed_read(fd, buffer, (size_t)turn_now, TICK_MS);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
                continue;
            }
            break;

        } else if (ret == 0) {
            result = 0;
            break;
        }

        if (!hash_update(hash_context, buffer, (unsigned long)ret)) {
            errno = EACCES;
            break;
        }

        if (turn_max < 0) {
            continue;
        }

        /* Limit rate related. */
        turn_remain -= ret;
        now = get_monotonic_timestamp();
        if ((turn - start) / TICK != (now - start) / TICK) {
            /* We've crossed a limit tick border. */
            turn_remain = turn_max;
            continue;
        }

        if (turn_remain == 0) {
            tick = (int)((now - start) / 1000000LL % TICK_MS);
            tick = TICK_MS - tick;
            msleep(tick);

            turn_remain = turn_max;
        }
    }

    safe_close(fd);
    if (result) {
        return -1;
    }

    if (!hash_final(buffer, hash_context)) {
        errno = EACCES;
        return -1;
    }

    p = output;
    for (i = 0; i < hash_length; ++i) {
        *p++ = TABLE[buffer[i] / 16];
        *p++ = TABLE[buffer[i] % 16];
    }

    *p = '\0';
    return 0;
}

#define HASH(n,i,u,f,t,l) \
int n(const char *filename, int limit_rate, int milliseconds, char *output) \
{ \
    t ctx; \
    return do_hash((hash_init_t)i, (hash_update_t)u, (hash_final_t)f, &ctx, l, \
                   filename, limit_rate, milliseconds, output); \
}

#ifndef OPENSSL_NO_MD5
HASH(hash_md5,    MD5_Init,    MD5_Update,    MD5_Final,    MD5_CTX,    MD5_DIGEST_LENGTH   );
#endif

#ifndef OPENSSL_NO_SHA1
HASH(hash_sha1,   SHA1_Init,   SHA1_Update,   SHA1_Final,   SHA_CTX,    SHA_DIGEST_LENGTH   );
#endif

#ifndef OPENSSL_NO_SHA256
HASH(hash_sha224, SHA224_Init, SHA224_Update, SHA224_Final, SHA256_CTX, SHA224_DIGEST_LENGTH);
HASH(hash_sha256, SHA256_Init, SHA256_Update, SHA256_Final, SHA256_CTX, SHA256_DIGEST_LENGTH);
#endif
