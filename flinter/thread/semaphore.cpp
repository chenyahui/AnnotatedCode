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

#include "flinter/thread/semaphore.h"

#include <assert.h>
#include <errno.h>

#include <stdexcept>

#include "config.h"
#if defined(WIN32)
# include <Windows.h>
# define H reinterpret_cast<HANDLE *>(_context)
#elif defined(__MACH__)
# include <dispatch/dispatch.h>
# define H reinterpret_cast<dispatch_semaphore_t *>(_context)
#elif HAVE_SEMAPHORE_H
#include <semaphore.h>
# define H reinterpret_cast<sem_t *>(_context)
#else
# error Unsupported: Semaphore
#endif

namespace flinter {

Semaphore::Semaphore(int initial_count) : _context(NULL)
{
    assert(initial_count >= 0);
#if defined(WIN32)
    HANDLE *context = new HANDLE;
    *context = CreateSemaphore(NULL, static_cast<LONG>(initial_count), LONG_MAX, NULL);
    if (!*context || *context == INVALID_HANDLE_VALUE) {
#elif defined(__MACH__)
    dispatch_semaphore_t *context = new dispatch_semaphore_t;
    *context = dispatch_semaphore_create(static_cast<long>(initial_count));
    if (!*context) {
#else
    sem_t *context = new sem_t;
    if (sem_init(context, 0, static_cast<unsigned int>(initial_count)) < 0) {
#endif
        delete context;
        throw std::runtime_error("Semaphore::Semaphore()");
    }

    _context = reinterpret_cast<Context *>(context);
}

Semaphore::~Semaphore()
{
#if defined(WIN32)
    CloseHandle(*H);
    delete H;
#elif defined(__MACH__)
    dispatch_release(*H);
    delete H;
#else
    sem_destroy(H);
    delete H;
#endif
}

void Semaphore::Acquire()
{
#if defined(WIN32)
    if (WaitForSingleObject(*H, INFINITE) == WAIT_OBJECT_0) {
        return;
    }
#elif defined(__MACH__)
    if (dispatch_semaphore_wait(*H, DISPATCH_TIME_FOREVER) == 0) {
        return;
    }
#else
    do {
        if (sem_wait(H) == 0) {
            return;
        }
    } while (errno == EINTR);
#endif
    throw std::runtime_error("Semaphore::Acquire()");
}

void Semaphore::Release(int count)
{
    assert(count > 0);
    if (!count) {
        return;
    }

#if defined(WIN32)
    if (!ReleaseSemaphore(*H, static_cast<LONG>(count), NULL)) {
        throw std::runtime_error("Semaphore::Release()");
    }
#elif defined(__MACH__)
    for (int i = 0; i < count; ++i) {
        dispatch_semaphore_signal(*H);
    }
#else
    for (int i = 0; i < count; ++i) {
        int ret = sem_post(H);
        if (ret < 0) {
            throw std::runtime_error("Semaphore::Release()");
        }
    }
#endif
}

bool Semaphore::TryAcquire()
{
#if defined(WIN32)
    DWORD ret = WaitForSingleObject(*H, 0);
    if (ret == WAIT_OBJECT_0) {
        return true;
    } else if (ret == WAIT_TIMEOUT) {
#elif defined(__MACH__)
    if (dispatch_semaphore_wait(*H, DISPATCH_TIME_NOW) == 0) {
        return true;
    } else if (true) {
#else
    if (sem_trywait(H) == 0) {
        return true;
    } else if (errno == EAGAIN) {
#endif
        return false;
    } else {
        throw std::runtime_error("Semaphore::TryAcquire()");
    }
}

} // namespace flinter
