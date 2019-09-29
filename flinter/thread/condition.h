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

#ifndef FLINTER_THREAD_CONDITION_H
#define FLINTER_THREAD_CONDITION_H

#include <stdint.h>

#include <flinter/common.h>

namespace flinter {

class Mutex;

/// POSIX condition.
class Condition {
public:
    Condition();                                ///< Constructor.
    ~Condition();                               ///< Destructor.

    void WakeOne();                             ///< Wake one waiter.
    void WakeAll();                             ///< Wake all waiters.

    bool Wait(Mutex *mutex);                    ///< Mutex must be locked.
    bool Wait(Mutex *mutex, int64_t timeout);   ///< Mutex must be locked, <0 means infinity.

private:
    class Context;                              ///< Internally used.
    Context *_context;                          ///< Internally used.

    NON_COPYABLE(Condition);                    ///< Don't copy me.

}; // class Condition

} // namespace flinter

#endif // FLINTER_THREAD_CONDITION_H
