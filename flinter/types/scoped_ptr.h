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

#ifndef FLINTER_TYPES_SCOPED_PTR_H
#define FLINTER_TYPES_SCOPED_PTR_H

namespace flinter {

template <class T>
class scoped_ptr {
public:
    explicit scoped_ptr(T *t) : _t(t) {}
    ~scoped_ptr()
    {
        delete _t;
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
    scoped_ptr<T> &operator = (const scoped_ptr<T> &);
    explicit scoped_ptr(const scoped_ptr<T> &);
    T *const _t;

}; // class scoped_ptr

} // namespace flinter

#endif // FLINTER_TYPES_SCOPED_PTR_H
