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

#include "flinter/thread/condition.h"

#include <assert.h>
#include <errno.h>
#include <time.h>

#include <stdexcept>

#include "flinter/thread/mutex.h"
#include "flinter/utility.h"

#include "config.h"
#if defined(WIN32)
# include <Windows.h>
#elif HAVE_PTHREAD_H
# include <pthread.h>
#else
# error Unsupported: Condition
#endif

namespace flinter {

#ifdef WIN32
# define CALL(x,s) if (!(x)) { throw std::runtime_error("Condition::" #s); }

class Condition::Context {
public:
    Context(HANDLE mutex, HANDLE sem) : _mutex(mutex), _sem(sem)
                                      , _wake(0), _waiting(0), _generation(0) {}

    HANDLE _mutex;
    HANDLE _sem;
    size_t _wake;
    size_t _waiting;
    size_t _generation;
}; // class Condition::Context
#endif

Condition::Condition() : _context(NULL)
{
#ifdef WIN32
    HANDLE mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex || mutex == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Condition::Condition()");
    }

    HANDLE sem = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
    if (!sem || sem == INVALID_HANDLE_VALUE) {
        CloseHandle(mutex);
        throw std::runtime_error("Condition::Condition()");
    }

    _context = new Context(mutex, sem);
#else
    pthread_cond_t *context = new pthread_cond_t;
    if (pthread_cond_init(context, NULL)) {
        delete context;
        throw std::runtime_error("Condition::Condition()");
    }
    _context = reinterpret_cast<Context *>(context);
#endif
}

Condition::~Condition()
{
#ifdef WIN32
    CloseHandle(_context->_sem);
    CloseHandle(_context->_mutex);
    delete _context;
#else
    pthread_cond_destroy(reinterpret_cast<pthread_cond_t *>(_context));
    delete reinterpret_cast<pthread_cond_t *>(_context);
#endif
}

bool Condition::Wait(Mutex *mutex)
{
    assert(mutex);
#ifdef WIN32
    return Wait(mutex, -1);
#else
    pthread_mutex_t *mutex_handle = reinterpret_cast<pthread_mutex_t *>(mutex->_context);
    if (pthread_cond_wait(reinterpret_cast<pthread_cond_t *>(_context), mutex_handle)) {
        throw std::runtime_error("Condition::Wait()");
    }
    return true;
#endif
}

bool Condition::Wait(Mutex *mutex, int64_t timeout)
{
#ifdef WIN32
    HANDLE mutex_handle = *reinterpret_cast<HANDLE *>(mutex->_context);
    DWORD timeout = timeout >= 0 ? static_cast<int>(timeout / 1000000LL) : INFINITE;
    size_t generation;
    bool result;
    bool wake;

    CALL(WaitForSingleObject(_context->_mutex, INFINITE) == WAIT_OBJECT_0, "Wait()");
    generation = _context->_generation;
    ++_context->_waiting;
    CALL(ReleaseMutex(_context->_mutex), "Wait()");

    CALL(ReleaseMutex(mutex_handle), "Wait()");
    while (true) {
        DWORD ret = WaitForSingleObject(_context->_sem, timeout);
        CALL(WaitForSingleObject(_context->_mutex, INFINITE) == WAIT_OBJECT_0, "Wait()");
        if (_context->_generation != generation) {
            if (_context->_wake) {
                --_context->_waiting;
                --_context->_wake;
                result = true;
                break;
            } else {
                wake = true;
            }
        } else if (ret == WAIT_TIMEOUT) {
            --_context->_waiting;
            result = false;
            break;
        } else if (ret != WAIT_OBJECT_0) {
            throw std::runtime_error("Condition::Wait()");
        }

        CALL(ReleaseMutex(_context->_mutex), "Wait()");
        if (wake) {
            wake = false;
            CALL(ReleaseSemaphore(_context->_sem, 1, NULL), "Wait()");
        }
    }

    CALL(ReleaseMutex(_context->_mutex), "Wait()");
    CALL(WaitForSingleObject(mutex_handle, INFINITE) == WAIT_OBJECT_0, "Wait()");
    return result;
#else
    if (timeout < 0) {
        return Wait(mutex);
    }

    struct timespec abstime;
    int64_t deadline = get_wall_clock_timestamp() + timeout;
    abstime.tv_nsec = deadline % 1000000000LL;
    abstime.tv_sec = deadline / 1000000000LL;

    pthread_mutex_t *mutex_handle = reinterpret_cast<pthread_mutex_t *>(mutex->_context);
    int ret = pthread_cond_timedwait(reinterpret_cast<pthread_cond_t *>(_context),
                                     mutex_handle, &abstime);

    if (ret == 0) {
        return true;
    } else if (ret == ETIMEDOUT) {
        return false;
    }
    throw std::runtime_error("Condition::Wait()");
#endif
}

void Condition::WakeOne()
{
#ifdef WIN32
    bool wake = false;
    CALL(WaitForSingleObject(_context->_mutex, INFINITE) == WAIT_OBJECT_0, "WakeOne()");
    if (_context->_waiting > _context->_wake) {
        wake = true;
        ++_context->_wake;
        ++_context->_generation;
    }

    CALL(ReleaseMutex(_context->_mutex), "WakeOne()");
    if (wake) {
        CALL(ReleaseSemaphore(_context->_sem, 1, NULL), "WakeOne()");
    }
#else
    if (pthread_cond_signal(reinterpret_cast<pthread_cond_t *>(_context))) {
        throw std::runtime_error("Condition::WakeOne()");
    }
#endif
}

void Condition::WakeAll()
{
#ifdef WIN32
    LONG wake = 0;
    CALL(WaitForSingleObject(_context->_mutex, INFINITE) == WAIT_OBJECT_0, "WakeAll()");
    if (_context->_waiting > _context->_wake) {
        wake = static_cast<LONG>(_context->_waiting - _context->_wake);
        _context->_wake = _context->_waiting;
        ++_context->_generation;
    }

    CALL(ReleaseMutex(_context->_mutex), "WakeAll()");
    if (wake) {
        CALL(ReleaseSemaphore(_context->_sem, wake, NULL), "WakeAll()");
    }
#else
    if (pthread_cond_broadcast(reinterpret_cast<pthread_cond_t *>(_context))) {
        throw std::runtime_error("Condition::WakeAll()");
    }
#endif
}

} // namespace flinter
