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

#ifndef FLINTER_THREAD_MUTEX_H
#define FLINTER_THREAD_MUTEX_H

#include <flinter/common.h>

namespace flinter {

/// POSIX mutex, might or might not be recursive.
class Mutex {
public:
    friend class Condition;

    Mutex();                            ///< Constructor.
    ~Mutex();                           ///< Destructor.

    void Lock();                        ///< Lock, not recursive.
    void Unlock();                      ///< Unlock, must be locked.

    bool TryLock();                     ///< Immediately returns.

private:
    class Context;                      ///< Internally used.
    Context *_context;                  ///< Internally used.

    NON_COPYABLE(Mutex);                ///< Don't copy me.

}; // class Mutex

} // namespace flinter

#endif // FLINTER_THREAD_MUTEX_H
