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

#ifndef FLINTER_THREAD_THREAD_JOB_H
#define FLINTER_THREAD_THREAD_JOB_H

namespace flinter {

class Runnable;

namespace internal {

class ThreadJob {
public:
    ThreadJob(Runnable *runnable, bool auto_release)
            : _runnable(runnable), _auto_release(auto_release)
    {
        // Intended left blank.
    }

    Runnable *runnable() const
    {
        return _runnable;
    }

    bool auto_release() const
    {
        return _auto_release;
    }

private:
    Runnable *_runnable;
    bool _auto_release;

}; // class ThreadJob

} // namespace internal
} // namespace flinter

#endif // FLINTER_THREAD_THREAD_JOB_H
