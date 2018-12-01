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

#ifndef FLINTER_TYPES_READ_ONLY_MAP_H
#define FLINTER_TYPES_READ_ONLY_MAP_H

#include <map>

namespace flinter {

template <class K, class V, class C = std::less<K>,
          class A = std::allocator<std::pair<const K, V> > >
class ReadOnlyMap {
public:
    /* explicit */ ReadOnlyMap(const std::map<K, V, C, A> &map) : _map(map), _v(V()) {}

    typedef typename std::map<K, V, C, A>::const_iterator iterator;
    typedef typename std::map<K, V, C, A>::const_iterator const_iterator;
    typedef typename std::map<K, V, C, A>::const_reverse_iterator reverse_iterator;
    typedef typename std::map<K, V, C, A>::const_reverse_iterator const_reverse_iterator;

              bool empty()  const { return _map.empty();    }
            size_t size()   const { return _map.size();     }
    const_iterator begin()  const { return _map.begin();    }
    const_iterator end()    const { return _map.end();      }
    const_iterator rbegin() const { return _map.rbegin();   }
    const_iterator rend()   const { return _map.rend();     }

    const V &Get(const K &k) const
    {
        typename std::map<K, V, C, A>::const_iterator p = _map.find(k);
        if (p == _map.end()) {
            return _v;
        }
        return p->second;
    }

    const V &operator [](const K &k) const
    {
        return Get(k);
    }

    bool Has(const K &k) const
    {
        return _map.find(k) != _map.end();
    }

private:
    const std::map<K, V, C, A> &_map;
    const V _v;

}; // class ReadOnlyMap

} // namespace flinter

#endif // FLINTER_TYPES_READ_ONLY_MAP_H
