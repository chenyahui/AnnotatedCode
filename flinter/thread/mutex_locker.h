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

#ifndef FLINTER_THREAD_MUTEX_LOCKER_H
#define FLINTER_THREAD_MUTEX_LOCKER_H

#include <assert.h>

#include <flinter/thread/mutex.h>
#include <flinter/common.h>

namespace flinter {

/// MutexLocker automatically locks a mutex when constructed and then unlocks it when
/// deconstructed.
/// @warning MutexLocker itself is not thread safe.
class MutexLocker {
public:
    /// Constructor.
    /// @warning Don't operate the mutex since on.
    explicit MutexLocker(Mutex *mutex) : _mutex(mutex), _locked(true)
    {
        assert(mutex);
        mutex->Lock();
    }

    /// Destructor.
    ~MutexLocker()
    {
        if (_locked) {
            Unlock();
        }
    }

    /// Can be called regardless of mutex locked or not.
    void Unlock()
    {
        if (_locked) {
            _mutex->Unlock();
            _locked = false;
        }
    }

    /// Can be called regardless of mutex locked or not.
    void Relock()
    {
        if (!_locked) {
            _mutex->Lock();
            _locked = true;
        }
    }

private:
    NON_COPYABLE(MutexLocker);  ///< Don't copy me.

    Mutex *_mutex;              ///< Mutex instance.
    bool _locked;               ///< Remember state.

}; // class MutexLocker

} // namespace flinter

#endif // FLINTER_THREAD_MUTEX_LOCKER_H
