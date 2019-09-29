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

#include "flinter/thread/tls.h"

#include <pthread.h>

#include <stdexcept>

namespace flinter {

TLS::TLS(const void *value)
{
    pthread_key_t *key = new pthread_key_t;
    if (pthread_key_create(key, NULL)) {
        throw std::runtime_error("pthread_key_create(P)");
    }

    _key = key;

    if (value) {
        Set(value);
    }
}

TLS::~TLS()
{
    pthread_key_t *key = reinterpret_cast<pthread_key_t *>(_key);
    pthread_key_delete(*key);
    delete key;
}

void TLS::Set(const void *value)
{
    pthread_key_t *key = reinterpret_cast<pthread_key_t *>(_key);
    if (pthread_setspecific(*key, value)) {
        throw std::runtime_error("pthread_setspecific(P)");
    }
}

void *TLS::Get()
{
    pthread_key_t *key = reinterpret_cast<pthread_key_t *>(_key);
    return pthread_getspecific(*key);
}

} // namespace flinter
