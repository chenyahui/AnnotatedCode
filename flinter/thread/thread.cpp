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

#include "flinter/thread/thread.h"

#include <signal.h>

#include "flinter/thread/abstract_thread_pool.h"
#include "flinter/thread/mutex_locker.h"
#include "flinter/thread/thread_job.h"
#include "flinter/logger.h"
#include "flinter/runnable.h"

#include "config.h"
#if defined(WIN32)
# include <Windows.h>
#elif HAVE_PTHREAD_H
#include <pthread.h>
#else
# error Unsupported: Thread
#endif

namespace flinter {
namespace internal {

#ifdef WIN32
static DWORD WINAPI ThreadRoutine(void *thread_ptr)
{
    assert(thread_ptr);
    Thread *thread = reinterpret_cast<Thread *>(thread_ptr);
    thread->ThreadDaemon();
    return 0;
}
#else
static void *ThreadRoutine(void *thread_ptr)
{
    assert(thread_ptr);
    Thread *thread = reinterpret_cast<Thread *>(thread_ptr);
    thread->ThreadDaemon();
    return NULL;
}
#endif

Thread::Thread(AbstractThreadPool *pool, void *parameter)
        : _parameter(parameter)
        , _context(NULL)
        , _pool(pool)
{
    assert(pool);

#ifdef WIN32
    DWORD tid;
    HANDLE *thread = new HANDLE;
    *thread = CreateThread(NULL, 0, ThreadRoutine, this, 0, &tid);
    if (!*thread || *thread == INVALID_HANDLE_VALUE) {
#else
    pthread_t *thread = new pthread_t;
    if (pthread_create(thread, NULL, ThreadRoutine, this)) {
#endif
        delete thread;
        throw std::runtime_error("Thread::Thread()");
    }

    _context = reinterpret_cast<internal::ThreadContext *>(thread);
}

Thread::~Thread()
{
    assert(_context);
#ifdef WIN32
    delete reinterpret_cast<HANDLE *>(_context);
#else
    delete reinterpret_cast<pthread_t *>(_context);
#endif
}

void Thread::Terminate()
{
    assert(_context);
#ifdef WIN32
    TerminateThread(*reinterpret_cast<HANDLE *>(_context), 0);
#else
    pthread_cancel(*reinterpret_cast<pthread_t *>(_context));
#endif
}

void Thread::Join()
{
    assert(_context);
#ifdef WIN32
    HANDLE thread = *reinterpret_cast<HANDLE *>(_context);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(*reinterpret_cast<pthread_t *>(_context), NULL);
#endif
}

bool Thread::Kill(int signum)
{
    assert(_context);
#ifdef WIN32
    return true;
#else
    return pthread_kill(*reinterpret_cast<pthread_t *>(_context), signum) == 0;
#endif
}

void Thread::ThreadDaemon()
{
    ThreadJob *job;
    Logger::ThreadAttach();

    while ((job = RequestJob())) {
        CLOG.Verbose("Thread: executing job [%p]", job->runnable());
        job->runnable()->Run();
        if (job->auto_release()) {
            delete job->runnable();
            CLOG.Verbose("Thread: done exeuting job [%p], auto released.", job);
        } else {
            CLOG.Verbose("Thread: done exeuting job [%p], not released.", job);
        }

        delete job;
    }

    Logger::ThreadDetach();
}

ThreadJob *Thread::RequestJob()
{
    return _pool->RequestJob(this);
}

} // namespace internal
} // namespace flinter
