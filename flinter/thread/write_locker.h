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

#ifndef FLINTER_THREAD_WRITE_LOCKER_H
#define FLINTER_THREAD_WRITE_LOCKER_H

#include <assert.h>

#include <flinter/thread/read_write_lock.h>
#include <flinter/common.h>

namespace flinter {

/// WriteLocker automatically read locks a rw lock when constructed and then unlocks it when
/// deconstructed.
/// @warning WriteLocker itself is not thread safe.
class WriteLocker {
public:
    /// Constructor.
    /// @warning Don't operate the rw lock since on.
    WriteLocker(ReadWriteLock *read_write_lock) : _read_write_lock(read_write_lock)
                                                , _locked(true)
    {
        assert(read_write_lock);
        _read_write_lock->WriterLock();
    }

    /// Destructor.
    ~WriteLocker()
    {
        Unlock();
    }

    /// Can be called regardless of locked or not.
    void Relock()
    {
        if (!_locked) {
            _read_write_lock->WriterLock();
            _locked = true;
        }
    }

    /// Can be called regardless of locked or not.
    void Unlock()
    {
        if (_locked) {
            _read_write_lock->Unlock();
            _locked = false;
        }
    }

private:
    NON_COPYABLE(WriteLocker);          ///< Don't copy me.

    ReadWriteLock *_read_write_lock;    ///< rw lock instance.
    bool _locked;                       ///< Remember state.

}; // class WriteLocker

} // namespace flinter

#endif // FLINTER_THREAD_WRITE_LOCKER_H
