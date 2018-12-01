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

#ifndef FLINTER_THREAD_SEMAPHORE_H
#define FLINTER_THREAD_SEMAPHORE_H

#include <flinter/common.h>

namespace flinter {

/// POSIX semaphore.
class Semaphore {
public:
    /// Not all implementations support maximum count.
    explicit Semaphore(int initial_count);

    /// Destructor.
    ~Semaphore();

    /// Immediately returns.
    bool TryAcquire();

    /// Acquire one.
    void Acquire();

    /// Release some.
    /// @warning Might loop internally for some implementations.
    void Release(int count = 1);

private:
    class Context;                          ///< Internally used.
    Context *_context;                      ///< Internally used.

    NON_COPYABLE(Semaphore);                ///< Don't copy me.

}; // class Semaphore

} // namespace flinter

#endif // FLINTER_THREAD_SEMAPHORE_H
