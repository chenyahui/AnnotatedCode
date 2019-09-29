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

#ifndef FLINTER_TIMEOUT_POOL_H
#define FLINTER_TIMEOUT_POOL_H

#include <stdint.h>

#include <list>
#include <map>

#include <flinter/thread/mutex.h>
#include <flinter/thread/mutex_locker.h>
#include <flinter/utility.h>

namespace flinter {

template <class K, class V>
class TimeoutPool {
    class Value {
    public:
        Value(int64_t d, const V &v) : _d(d), _v(v) {}
        int64_t _d;
        V _v;
    }; // class Value

public:
    explicit TimeoutPool(int64_t default_timeout,
                         bool auto_release = true)
            : _default_timeout(default_timeout)
            , _auto_release(auto_release) {}

    ~TimeoutPool()
    {
        Clear();
    }

    void Insert(const K &k, const V &v, int64_t timeout = 0)
    {
        MutexLocker locker(&_mutex);
        int64_t now = get_monotonic_timestamp();
        int64_t deadline = now + (timeout > 0 ? timeout : _default_timeout);
        std::pair<typename std::map<K, Value>::iterator, bool> ret =
                _pool.insert(std::make_pair(k, Value(deadline, v)));

        if (!ret.second) {
            Value &value = ret.first->second;
            if (_auto_release) {
                delete value._v;
            }

            value._v = v;
            value._d = deadline;
        }
    }

    V Erase(const K &k)
    {
        MutexLocker locker(&_mutex);
        typename std::map<K, Value>::iterator p = _pool.find(k);
        if (p == _pool.end()) {
            return NULL;
        }

        V v = p->second._v;
        _pool.erase(p);
        return v;
    }

    void Check()
    {
        MutexLocker locker(&_mutex);
        std::list<K> drop;
        int64_t now = get_monotonic_timestamp();
        for (typename std::map<K, Value>::iterator
             p = _pool.begin(); p != _pool.end(); ++p) {

            if (p->second._d <= now) {
                if (_auto_release) {
                    delete p->second._v;
                }

                drop.push_back(p->first);
            }
        }

        for (typename std::list<K>::iterator p = drop.begin();
             p != drop.end(); ++p) {

            _pool.erase(*p);
        }
    }

    void Clear()
    {
        MutexLocker locker(&_mutex);
        if (_auto_release) {
            for (typename std::map<K, Value>::iterator
                 p = _pool.begin(); p != _pool.end(); ++p) {

                delete p->second._v;
            }
        }

        _pool.clear();
    }

private:
    const int64_t _default_timeout;
    const bool _auto_release;
    std::map<K, Value> _pool;
    mutable Mutex _mutex;

}; // class TimeoutPool

} // namespace flinter

#endif // FLINTER_TIMEOUT_POOL_H
