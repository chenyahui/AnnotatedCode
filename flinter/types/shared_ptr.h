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

#ifndef FLINTER_TYPES_SHARED_PTR_H
#define FLINTER_TYPES_SHARED_PTR_H

#include <flinter/types/atomic.h>

namespace flinter {

template <class T>
class shared_ptr {
public:
    explicit shared_ptr(T *t) : _c(new atomic_t), _t(t)
    {
        _c->AddAndFetch(1);
    }

    shared_ptr(const shared_ptr<T> &other) : _c(other._c), _t(other._t)
    {
        _c->AddAndFetch(1);
    }

    ~shared_ptr()
    {
        if (_c->SubAndFetch(1) == 0) {
            delete _t;
            delete _c;
        }
    }

    const T *operator -> () const
    {
        return _t;
    }

    T *operator -> ()
    {
        return _t;
    }

    const T &operator * () const
    {
        return *_t;
    }

    T &operator * ()
    {
        return *_t;
    }

    const T *Get() const
    {
        return _t;
    }

    T *Get()
    {
        return _t;
    }

private:
    shared_ptr<T> &operator = (const shared_ptr<T> &);
    atomic_t *const _c;
    T *const _t;

}; // class shared_ptr

} // namespace flinter

#endif // FLINTER_TYPES_SHARED_PTR_H
