/* Copyright 2017 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_THREAD_SPINLOCK_H
#define FLINTER_THREAD_SPINLOCK_H

#include <assert.h>

#include <flinter/types/atomic.h>
#include <flinter/common.h>

#if defined(__linux__) || defined(__MACH__)
#include <sched.h>
#endif

namespace flinter {

class Spinlock {
public:
    class Locker;

    Spinlock() : _spinlock(0) {}

    void Lock(bool yield = false)
    {
        if (yield) {
            while (!_spinlock.Lock()) {
#if defined(__linux__)
                sched_yield();
#endif
            }
        } else {
            while (!_spinlock.Lock());
        }
    }

    void Unlock()
    {
        _spinlock.Unlock();
    }

private:
    atomic_t _spinlock;

}; // class Spinlock

/// Locker automatically locks a mutex when constructed and then unlocks it when
/// deconstructed.
/// @warning Locker itself is not thread safe.
class Spinlock::Locker {
public:
    /// Constructor.
    /// @warning Don't operate the spinlock since on.
    explicit Locker(Spinlock *spinlock, bool yield = false)
            : _spinlock(spinlock), _locked(true), _yield(yield)
    {
        assert(_spinlock);
        _spinlock->Lock(_yield);
    }

    /// Destructor.
    ~Locker()
    {
        if (_locked) {
            Unlock();
        }
    }

    /// Can be called regardless of mutex locked or not.
    void Unlock()
    {
        if (_locked) {
            _spinlock->Unlock();
            _locked = false;
        }
    }

    /// Can be called regardless of mutex locked or not.
    void Relock()
    {
        if (!_locked) {
            _spinlock->Lock(_yield);
            _locked = true;
        }
    }

private:
    NON_COPYABLE(Locker);   ///< Don't copy me.
    Spinlock *_spinlock;    ///< Spinlock instance.
    bool _locked;           ///< Remember state.
    bool _yield;            ///< Yield while locking.

}; // class Spinlock::Locker

} // namespace flinter

#endif // FLINTER_THREAD_SPINLOCK_H
