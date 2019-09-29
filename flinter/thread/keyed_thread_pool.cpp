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

#include "flinter/thread/keyed_thread_pool.h"

#include <algorithm>

#include "flinter/thread/mutex_locker.h"
#include "flinter/thread/thread.h"
#include "flinter/thread/thread_job.h"
#include "flinter/logger.h"
#include "flinter/runnable.h"

namespace flinter {

KeyedThreadPool::KeyedThreadPool() : _initializing(false)
{
    // Intended left blank.
}

KeyedThreadPool::~KeyedThreadPool()
{
    Shutdown(false);
}

bool KeyedThreadPool::Initialize(size_t size)
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

void KeyedThreadPool::Spawn(size_t size)
{
    assert(_threads.empty());

    CLOG.Verbose("ThreadPool: spawning %lu threads...", size);
    _threads.reserve(size);
    for (size_t i = 0; i < size; i++) {
        Context *context = new Context;
        internal::Thread *thread = new internal::Thread(this, context);
        context->_thread = thread;
        _threads.push_back(context);
    }
}

bool KeyedThreadPool::Shutdown(bool wait_for_jobs_done)
{
    MutexLocker locker(&_mutex);
    if (_initializing) {
        return false;
    } else if (_threads.empty()) {
        return true;
    }

    // Purge any jobs that are not yet scheduled.
    for (threads_t::iterator p = _threads.begin(); p != _threads.end(); ++p) {
        Context *context = *p;
        Purge(&context->_pending_jobs);
        context->_job_available.WakeOne();
    }

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

    _initializing = true;
    locker.Unlock();

    CLOG.Verbose("ThreadPool: joining %lu threads...", threads.size());
    for (threads_t::iterator p = threads.begin(); p != threads.end(); ++p) {
        Context *context = *p;
        context->_thread->Join();
        delete context->_thread;
        delete context;
    }

    locker.Relock();
    _initializing = false;
    return true;
}

bool KeyedThreadPool::TryRequestJob(Context *context,
                                    internal::ThreadJob **job)
{
    if (_initializing) {
        *job = NULL;
        return true;
    }

    if (context->_pending_jobs.empty()) {
        return false;
    }

    *job = context->_pending_jobs.front();
    context->_pending_jobs.pop_front();
    return true;
}

internal::ThreadJob *KeyedThreadPool::RequestJob(internal::Thread *thread)
{
    assert(thread);
    MutexLocker locker(&_mutex);
    _actives.erase(thread);

    Context *context = reinterpret_cast<Context *>(thread->parameter());
    assert(context);

    internal::ThreadJob *job;
    while (!TryRequestJob(context, &job)) {
        context->_job_available.Wait(&_mutex);
    }

    if (job) {
        _actives.insert(thread);
    }

    return job;
}

bool KeyedThreadPool::AppendJobWithKey(size_t key,
                                       Runnable *runnable,
                                       bool auto_release)
{
    assert(runnable);
    if (!runnable) {
        return false;
    }

    MutexLocker locker(&_mutex);
    if (_initializing) {
        return false;
    }

    size_t size = _threads.size();
    size_t index = key % size;
    Context *context = _threads[index];

    internal::ThreadJob *job = new internal::ThreadJob(runnable, auto_release);
    context->_pending_jobs.push_back(job);
    context->_job_available.WakeOne();
    return true;
}

bool KeyedThreadPool::KillAll(int signum)
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
