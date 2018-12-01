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

#ifndef FLINTER_THREAD_ABSTRACT_THREAD_POOL_H
#define FLINTER_THREAD_ABSTRACT_THREAD_POOL_H

#include <stddef.h>

#include <list>

namespace flinter {
namespace internal {
class Thread;
class ThreadJob;
} // namespace internal

class Runnable;

class AbstractThreadPool {
public:
    friend class internal::Thread;
    virtual ~AbstractThreadPool() {}

    virtual bool AppendJob(Runnable *runnable, bool auto_release = false) = 0;
    virtual bool Shutdown(bool wait_for_jobs_done = true) = 0;
    virtual bool Initialize(size_t size) = 0;
    virtual bool KillAll(int signum) = 0;

protected:
    typedef std::list<internal::ThreadJob *> job_list_t;

    AbstractThreadPool() {}
    virtual internal::ThreadJob *RequestJob(internal::Thread *thread) = 0;

    /// Will delete jobs if auto releasing.
    static void Purge(job_list_t *jobs);

private:
    AbstractThreadPool(const AbstractThreadPool &);
    AbstractThreadPool &operator = (const AbstractThreadPool &);

}; // class AbstractThreadPool

} // namespace flinter

#endif // FLINTER_THREAD_ABSTRACT_THREAD_POOL_H
