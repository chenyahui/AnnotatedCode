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

#ifndef FLINTER_TYPES_UNORDERED_MAP_H
#define FLINTER_TYPES_UNORDERED_MAP_H

#include <flinter/types/unordered_hash.h>

#ifdef __GNUC__
# if __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 5 || defined(__clang__)
#  include <unordered_map>
# elif __GNUC__ == 4
#  include <tr1/unordered_map>
namespace std {
using tr1::unordered_multimap;
using tr1::unordered_map;
} // namespace std
# else
#  if __GNUC__ == 3
#   include <ext/hash_map>
#  else
#   include <hash_map>
#  endif
namespace std {
template <class Key, class T,
          class Hash = __gnu_cxx::hash<Key>,
          class Pred = __gnu_cxx::equal_to<Key>,
          class Alloc = __gnu_cxx::allocator<__gnu_cxx::pair<const Key, T> > >
class unordered_multimap : public __gnu_cxx::hash_multimap<Key, T, Hash, Pred, Alloc> {
public:
    typedef __gnu_cxx::hash_multimap<Key, T, Hash, Pred, Alloc> _Base;
    template <class InputIterator>
    unordered_multimap(InputIterator first, InputIterator last) : _Base(first, last) {}
    unordered_multimap(const _Base &ump) : _Base(ump) {}
    unordered_multimap() : _Base() {}
}; // class unordered_multimap

template <class Key, class T,
          class Hash = __gnu_cxx::hash<Key>,
          class Pred = __gnu_cxx::equal_to<Key>,
          class Alloc = __gnu_cxx::allocator<__gnu_cxx::pair<const Key, T> > >
class unordered_map : public __gnu_cxx::hash_map<Key, T, Hash, Pred, Alloc> {
public:
    typedef __gnu_cxx::hash_map<Key, T, Hash, Pred, Alloc> _Base;
    template <class InputIterator>
    unordered_map(InputIterator first, InputIterator last) : _Base(first, last) {}
    unordered_map(const _Base &ump) : _Base(ump) {}
    unordered_map() : _Base() {}
}; // class unordered_map
} // namespace std
# endif
#elif defined(_MSC_VER)
# if _MSC_VER >= 1600
#  include <unordered_map>
# elif _MSC_VER >= 1310 // 2002 and prior don't have std::hash_map<>
#  include <hash_map>
namespace std {
template <class Key, class T,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>,
          class Alloc = std::allocator<std::pair<const Key, T> > >
class unordered_multimap
        : public stdext::hash_multimap<Key, T, __unordered_hash<Key, Hash, Pred>, Alloc> {
public:
    template <class InputIterator>
    unordered_multimap(InputIterator first, InputIterator last) : hash_multimap(first, last) {}
    unordered_multimap(const unordered_multimap &ump) : hash_multimap(ump) {}
    unordered_multimap() : hash_multimap() {}
}; // class unordered_multimap

template <class Key, class T,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>,
          class Alloc = std::allocator<std::pair<const Key, T> > >
class unordered_map : public stdext::hash_map<Key, T, __unordered_hash<Key, Hash, Pred>, Alloc> {
public:
    template <class InputIterator>
    unordered_map(InputIterator first, InputIterator last) : hash_map(first, last) {}
    unordered_map(const unordered_map &ump) : hash_map(ump) {}
    unordered_map() : hash_map() {}
}; // class unordered_map
} // namespace std
# else
#  error Unsupported: std::unordered_map
# endif
#endif
#endif // FLINTER_TYPES_UNORDERED_MAP_H
