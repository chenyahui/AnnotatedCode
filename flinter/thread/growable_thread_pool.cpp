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

#include "flinter/thread/growable_thread_pool.h"

#include <algorithm>

#include "flinter/thread/mutex_locker.h"
#include "flinter/thread/thread.h"
#include "flinter/thread/thread_job.h"
#include "flinter/logger.h"
#include "flinter/runnable.h"

namespace flinter {

GrowableThreadPool::GrowableThreadPool() : _exits(0)
                                         , _size(0)
                                         , _initializing(false)
{
    // Intended left blank.
}

GrowableThreadPool::~GrowableThreadPool()
{
    Shutdown(false);
}

bool GrowableThreadPool::Initialize(size_t size)
{
    if (!size) {
        return false;
    }

    MutexLocker locker(&_mutex);
    if (_initializing || !_threads.empty()) {
        return false;
    }

    _size = size;
    return true;
}

void GrowableThreadPool::Spawn(size_t size)
{
    CLOG.Verbose("ThreadPool: spawning %lu threads...", size);
    for (size_t i = 0; i < size; i++) {
        internal::Thread *thread = new internal::Thread(this, NULL);
        _threads.insert(thread);
    }
}

bool GrowableThreadPool::Shutdown(bool wait_for_jobs_done)
{
    MutexLocker locker(&_mutex);
    if (_initializing) {
        return false;
    } else if (_threads.empty()) {
        return true;
    }

    // Purge any jobs that are not yet scheduled.
    Purge(&_pending_jobs);
    _exits = _threads.size();

    // Kill active threads.
    if (!wait_for_jobs_done && !_actives.empty()) {
        CLOG.Verbose("ThreadPool: terminating %lu threads...", _actives.size());
        for (actives_t::iterator p = _actives.begin(); p != _actives.end(); ++p) {
            internal::Thread *thread = *p;
            thread->Terminate();
        }

        _exits -= _actives.size();
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

bool GrowableThreadPool::TryRequestJob(internal::ThreadJob **job)
{
    if (_exits) {
        --_exits;
        *job = NULL;
        return true;

    } else if (_pending_jobs.empty()) {
        return false;
    }

    *job = _pending_jobs.front();
    _pending_jobs.pop_front();
    return true;
}

internal::ThreadJob *GrowableThreadPool::RequestJob(internal::Thread *thread)
{
    assert(thread);
    MutexLocker locker(&_mutex);
    _actives.erase(thread);
    MaybeShrink();

    internal::ThreadJob *job;
    while (!TryRequestJob(&job)) {
        _job_available.Wait(&_mutex);
    }

    if (job) {
        _actives.insert(thread);

    } else if (!_initializing) {
        _exited.push_back(thread);
        _threads.erase(thread);
        _thread_exited.WakeOne();
    }

    return job;
}

void GrowableThreadPool::MaybeShrink()
{
    if (_initializing) {
        return;
    }

    size_t extra = _size / 2 + _size % 2;
    size_t threshold = _size + extra;
    size_t idles = _threads.size() - _actives.size()
                 - _pending_jobs.size() - _exits;

    if (idles < threshold) {
        return;
    }

    size_t exits = idles / _size * _size;
    if (idles % _size == 0) {
        exits -= _size;
    }

    _exits += exits;
    CLOG.Verbose("ThreadPool: %lu idle threads, gonna destroy %lu threads...",
                 idles, exits);

    // The calling thread will become idle after reaping other idle threads.
    for (size_t i = 0; i < _size; ++i) {
        _job_available.WakeOne();
    }

    size_t reaped = 0;
    while (true) {
        while (_exited.empty()) {
            _thread_exited.Wait(&_mutex);
        }

        internal::Thread *thread = _exited.front();
        _exited.pop_front();
        thread->Join();

        ++reaped;
        if (reaped == exits) {
            break;
        }
    }

    CLOG.Verbose("ThreadPool: reaped %lu threads.", reaped);
}

bool GrowableThreadPool::AppendJob(Runnable *runnable, bool auto_release)
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

    size_t remains = _threads.size() - _actives.size() - _exits;
    if (remains < _pending_jobs.size()) {
        Spawn(_size);
    }

    _job_available.WakeOne();
    return true;
}

bool GrowableThreadPool::KillAll(int signum)
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
