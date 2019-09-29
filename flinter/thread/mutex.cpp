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

#include "flinter/thread/mutex.h"

#include <errno.h>

#include <stdexcept>

#include "config.h"
#if defined(WIN32)
# include <Windows.h>
#elif HAVE_PTHREAD_H
# include <pthread.h>
#else
# error Unsupported: Mutex
#endif

namespace flinter {

Mutex::Mutex() : _context(NULL)
{
#ifdef WIN32
    HANDLE *context = new HANDLE;
    *context = CreateMutex(NULL, FALSE, NULL);
    if (!*context || *context == INVALID_HANDLE_VALUE) {
#else
    pthread_mutex_t *context = new pthread_mutex_t;
    if (pthread_mutex_init(context, NULL)) {
#endif
        delete context;
        throw std::runtime_error("Mutex::Mutex()");
    }

    _context = reinterpret_cast<Context *>(context);
}

Mutex::~Mutex()
{
#ifdef WIN32
    CloseHandle(*reinterpret_cast<HANDLE *>(_context));
    delete reinterpret_cast<HANDLE *>(_context);
#else
    pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(_context));
    delete reinterpret_cast<pthread_mutex_t *>(_context);
#endif
}

void Mutex::Lock()
{
#ifdef WIN32
    if (WaitForSingleObject(*reinterpret_cast<HANDLE *>(_context), INFINITE) != WAIT_OBJECT_0) {
#else
    if (pthread_mutex_lock(reinterpret_cast<pthread_mutex_t *>(_context))) {
#endif
        throw std::runtime_error("Mutex::Lock()");
    }
}

void Mutex::Unlock()
{
#ifdef WIN32
    if (!ReleaseMutex(*reinterpret_cast<HANDLE *>(_context))) {
#else
    if (pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t *>(_context))) {
#endif
        throw std::runtime_error("Mutex::Unlock()");
    }
}

bool Mutex::TryLock()
{
#ifdef WIN32
    DWORD ret = WaitForSingleObject(*reinterpret_cast<HANDLE *>(_context), 0);
    if (ret == WAIT_OBJECT_0) {
        return true;
    } else if (ret == WAIT_TIMEOUT) {
#else
    int ret = pthread_mutex_trylock(reinterpret_cast<pthread_mutex_t *>(_context));
    if (ret == 0) {
        return true;
    } else if (ret == EBUSY) {
#endif
        return false;
    } else {
        throw std::runtime_error("Mutex::TryLock()");
    }
}

} // namespace flinter
