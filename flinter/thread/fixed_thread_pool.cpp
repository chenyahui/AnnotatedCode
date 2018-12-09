/* Copyright 2015 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#include "flinter/thread/fixed_thread_pool.h"

#include <algorithm>

#include "flinter/thread/mutex_locker.h"
#include "flinter/thread/thread.h"
#include "flinter/thread/thread_job.h"
#include "flinter/logger.h"
#include "flinter/runnable.h"

namespace flinter {

FixedThreadPool::FixedThreadPool() : _initializing(false)
{
    // Intended left blank.
}

FixedThreadPool::~FixedThreadPool()
{
    Shutdown(false);
}

bool FixedThreadPool::Initialize(size_t size)
{
    if (!size) {
        return false;
    }

    MutexLocker locker(&_mutex);
    if (_initializing || !_threads.empty()) {
        return false;
    }

    Spawn(size);
    return true;
}

void FixedThreadPool::Spawn(size_t size)
{
    assert(_threads.empty());

    CLOG.Verbose("ThreadPool: spawning %lu threads...", size);
    _threads.reserve(size);
    for (size_t i = 0; i < size; i++) {
        internal::Thread *thread = new internal::Thread(this, NULL);
        _threads.push_back(thread);
    }
}

bool FixedThreadPool::Shutdown(bool wait_for_jobs_done)
{
    MutexLocker locker(&_mutex);
    if (_initializing) {
        return false;
    } else if (_threads.empty()) {
        return true;
    }

    // Purge any jobs that are not yet scheduled.
    Purge(&_pending_jobs);

    // Kill active threads.
    if (!wait_for_jobs_done && !_actives.empty()) {
        CLOG.Verbose("ThreadPool: terminating %lu threads...", _actives.size());
        for (actives_t::iterator p = _actives.begin(); p != _actives.end(); ++p) {
            internal::Thread *thread = *p;
            thread->Terminate();
        }
    }

    _actives.clear();
    threads_t threads;
    threads.swap(_threads);
    _job_available.WakeAll();

    _initializing = true;
    locker.Unlock();

    CLOG.Verbose("ThreadPool: joining %lu threads...", threads.size());
    for (threads_t::iterator p = threads.begin(); p != threads.end(); ++p) {
        internal::Thread *thread = *p;
        thread->Join();
        delete thread;
    }

    locker.Relock();
    _initializing = false;
    return true;
}

bool FixedThreadPool::TryRequestJob(internal::ThreadJob **job)
{
    if (_initializing) {
        *job = NULL;
        return true;

    } else if (_pending_jobs.empty()) {
        return false;
    }

    *job = _pending_jobs.front();
    _pending_jobs.pop_front();
    return true;
}

internal::ThreadJob *FixedThreadPool::RequestJob(internal::Thread *thread)
{
    assert(thread);
    MutexLocker locker(&_mutex);
    _actives.erase(thread);

    internal::ThreadJob *job;
    while (!TryRequestJob(&job)) {
        _job_available.Wait(&_mutex);
    }

    if (job) {
        _actives.insert(thread);
    }

    return job;
}

bool FixedThreadPool::AppendJob(Runnable *runnable, bool auto_release)
{
    assert(runnable);
    if (!runnable) {
        return false;
    }

    MutexLocker locker(&_mutex);
    if (_initializing) {
        return false;
    }

    _pending_jobs.push_back(new internal::ThreadJob(runnable, auto_release));
    _job_available.WakeOne();
    return true;
}

bool FixedThreadPool::KillAll(int signum)
{
    MutexLocker locker(&_mutex);
    if (_initializing) {
        return false;
    }

    bool result = true;

    for (actives_t::iterator p = _actives.begin(); p != _actives.end(); ++p) {
        internal::Thread *thread = *p;
        result &= thread->Kill(signum);
    }

    return result;
}

} // namespace flinter
