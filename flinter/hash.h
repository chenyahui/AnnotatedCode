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

#ifndef FLINTER_HASH_H
#define FLINTER_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * seed is calculated as (uint32_t)(length * 0xdeadbeef)
 */
extern uint32_t hash_murmurhash3(const void *buffer, size_t length);

/**
 * Calculate MD5 with speed limit and timeout.
 *
 * @param filename to hash
 * @param limit_rate bytes per second, <1024 for no limit
 * @param milliseconds overall timeout, <=0 for no limit
 * @param output must be larger than 32 + 1 bytes
 */
extern int hash_md5(const char *filename, int limit_rate, int milliseconds, char *output);

/**
 * Calculate SHA1 with speed limit and timeout.
 *
 * @param filename to hash
 * @param limit_rate bytes per second, <1024 for no limit
 * @param milliseconds overall timeout, <=0 for no limit
 * @param output must be larger than 40 + 1 bytes
 */
extern int hash_sha1(const char *filename, int limit_rate, int milliseconds, char *output);

/**
 * Calculate SHA224 with speed limit and timeout.
 *
 * @param filename to hash
 * @param limit_rate bytes per second, <1024 for no limit
 * @param milliseconds overall timeout, <=0 for no limit
 * @param output must be larger than 56 + 1 bytes
 */
extern int hash_sha224(const char *filename, int limit_rate, int milliseconds, char *output);

/**
 * Calculate SHA256 with speed limit and timeout.
 *
 * @param filename to hash
 * @param limit_rate bytes per second, <1024 for no limit
 * @param milliseconds overall timeout, <=0 for no limit
 * @param output must be larger than 64 + 1 bytes
 */
extern int hash_sha256(const char *filename, int limit_rate, int milliseconds, char *output);

#ifdef __cplusplus
}
#endif

#endif /* FLINTER_HASH_H */
