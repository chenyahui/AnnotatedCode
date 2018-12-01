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

#ifndef FLINTER_THREAD_GROWABLE_THREAD_POOL_H
#define FLINTER_THREAD_GROWABLE_THREAD_POOL_H

#include <list>
#include <set>

#include <flinter/thread/abstract_thread_pool.h>
#include <flinter/thread/condition.h>
#include <flinter/thread/mutex.h>
#include <flinter/common.h>

namespace flinter {
namespace internal {
class Thread;
class ThreadJob;
} // namespace internal

class Runnable;

/// GrowableThreadPool holds threads.
class GrowableThreadPool : public AbstractThreadPool {
public:
    explicit GrowableThreadPool();  ///< Constructor.
    virtual ~GrowableThreadPool();  ///< Destructor.

    /// @param size thread count.
    /// @warning not thread-safe, only call in the main thread.
    virtual bool Initialize(size_t size);

    /// @param wait_for_jobs_done wait for running jobs to be done, or to terminate them.
    /// @warning not thread-safe, only call in the main thread.
    virtual bool Shutdown(bool wait_for_jobs_done = true);

    /// Append a job to the pool, will be scheduled immediately.
    /// @param runnable the job to run.
    /// @param auto_release to delete runnable when it's done.
    virtual bool AppendJob(Runnable *runnable, bool auto_release = false);

    /// Only implemented on *nix;
    virtual bool KillAll(int signum);

protected:
    /// Called by internal::Thread.
    /// @return NULL to exit the worker thread.
    virtual internal::ThreadJob *RequestJob(internal::Thread *thread);

private:
    typedef std::set<internal::Thread *> actives_t;
    typedef std::set<internal::Thread *> threads_t;
    typedef std::list<internal::Thread *> exited_t;

    /// Will not block. No lock.
    /// @return NULL if no jobs are available.
    bool TryRequestJob(internal::ThreadJob **job);

    /// Batch spawn threads. No lock.
    void Spawn(size_t size);

    /// Batch destroy threads. No lock.
    void MaybeShrink();

    /// Threads that have jobs running.
    actives_t _actives;

    /// Threads that are exited.
    exited_t _exited;

    /// Threads.
    threads_t _threads;

    /// How many threads should quit.
    size_t _exits;

    /// Batch spawn size;
    size_t _size;

    /// Critical lock.
    bool _initializing;

    /// Jobs that are not yet scheduled.
    job_list_t _pending_jobs;
    Condition _job_available;
    Condition _thread_exited;
    Mutex _mutex;

}; // class GrowableThreadPool

} // namespace flinter

#endif // FLINTER_THREAD_GROWABLE_THREAD_POOL_H
