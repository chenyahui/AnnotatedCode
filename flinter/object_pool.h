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

#ifndef FLINTER_OBJECT_POOL_H
#define FLINTER_OBJECT_POOL_H

#include <assert.h>

#include <flinter/thread/mutex.h>
#include <flinter/thread/mutex_locker.h>
#include <flinter/utility.h>

#include <list>

namespace flinter {

template <class T>
class ObjectPool {
public:
    virtual ~ObjectPool();

    T *Grab();
    T *Exchange(T *object);
    void Remove(T *object);
    void Release(T *object);
    void Shrink();

    size_t size() const;

protected:
    // Nanoseconds.
    virtual int64_t max_idle_time() = 0;
    virtual void Destroy(T *object) = 0;
    virtual T *Create() = 0;

    /// Subclasses must call this in their deconstructors.
    void Clear(bool all = true);

private:
    typedef struct {
        T *object;
        bool in_use;
        int64_t timestamp;
    } object_t;

    std::list<object_t> _pool;
    mutable Mutex _mutex;

}; // class ObjectPool

template <class T>
inline ObjectPool<T>::~ObjectPool()
{
    assert(_pool.empty());
}

template <class T>
inline size_t ObjectPool<T>::size() const
{
    MutexLocker locker(&_mutex);
    return _pool.size();
}

template <class T>
inline T *ObjectPool<T>::Grab()
{
    MutexLocker locker(&_mutex);
    for (typename std::list<object_t>::iterator p = _pool.begin();
         p != _pool.end(); ++p) {

        if (!p->in_use) {
            p->in_use = true;
            return p->object;
        }
    }

    locker.Unlock();

    object_t n;
    n.object = Create();
    if (!n.object) {
        return NULL;
    }

    n.in_use = true;
    n.timestamp = -1;

    locker.Relock();
    _pool.push_back(n);
    return n.object;
}

template <class T>
inline T *ObjectPool<T>::Exchange(T *object)
{
    Remove(object);
    return Grab();
}

template <class T>
inline void ObjectPool<T>::Remove(T *object)
{
    MutexLocker locker(&_mutex);
    for (typename std::list<object_t>::iterator p = _pool.begin();
         p != _pool.end(); ++p) {

        if (p->object == object) {
            _pool.erase(p);
            break;
        }
    }

    locker.Unlock();
    Destroy(object);
}

template <class T>
inline void ObjectPool<T>::Release(T *object)
{
    MutexLocker locker(&_mutex);
    for (typename std::list<object_t>::iterator p = _pool.begin();
         p != _pool.end(); ++p) {

        if (p->object == object) {
            p->in_use = false;
            p->timestamp = get_monotonic_timestamp();
            break;
        }
    }
}

template <class T>
inline void ObjectPool<T>::Shrink()
{
    Clear(false);
}

template <class T>
inline void ObjectPool<T>::Clear(bool all)
{
    typename std::list<T *> gone;
    MutexLocker locker(&_mutex);
    int64_t now = get_monotonic_timestamp();
    int64_t deadline = now - max_idle_time();
    for (typename std::list<object_t>::iterator p = _pool.begin();
         p != _pool.end();) {

        if (all || (!p->in_use && p->timestamp < deadline)) {
            gone.push_back(p->object);
            p = _pool.erase(p);
        } else {
            ++p;
        }
    }

    locker.Unlock();
    for (typename std::list<T *>::iterator p = gone.begin();
         p != gone.end(); ++p) {

        Destroy(*p);
    }
}

} // namespace flinter

#endif // FLINTER_OBJECT_POOL_H
