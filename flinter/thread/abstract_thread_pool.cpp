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

#include "flinter/thread/abstract_thread_pool.h"

#include "flinter/thread/thread.h"
#include "flinter/thread/thread_job.h"
#include "flinter/runnable.h"

namespace flinter {

void AbstractThreadPool::Purge(job_list_t *jobs)
{
    for (job_list_t::iterator p = jobs->begin(); p != jobs->end(); ++p) {
        internal::ThreadJob *job = *p;
        if (job->auto_release()) {
            delete job->runnable();
        }

        delete job;
    }

    jobs->clear();
}

} // namespace flinter
