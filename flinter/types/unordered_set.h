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

#ifndef FLINTER_TYPES_UNORDERED_SET_H
#define FLINTER_TYPES_UNORDERED_SET_H

#include <flinter/types/unordered_hash.h>

#ifdef __GNUC__
# if __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 5 || defined(__clang__)
#  include <unordered_set>
# elif __GNUC__ == 4
#  include <tr1/unordered_set>
namespace std {
using tr1::unordered_multiset;
using tr1::unordered_set;
} // namespace std
# else
#  if __GNUC__ == 3
#   include <ext/hash_set>
#  else
#   include <hash_set>
#  endif
namespace std {
template <class Key,
          class Hash = __gnu_cxx::hash<Key>,
          class Pred = __gnu_cxx::equal_to<Key>,
          class Alloc = __gnu_cxx::allocator<const Key> >
class unordered_multiset : public __gnu_cxx::hash_multiset<Key, Hash, Pred, Alloc> {
public:
    typedef __gnu_cxx::hash_multiset<Key, Hash, Pred, Alloc> _Base;
    template <class InputIterator>
    unordered_multiset(InputIterator first, InputIterator last) : _Base(first, last) {}
    unordered_multiset(const _Base &ums) : _Base(ums) {}
    unordered_multiset() : _Base() {}
}; // class unordered_multiset

template <class Key,
          class Hash = __gnu_cxx::hash<Key>,
          class Pred = __gnu_cxx::equal_to<Key>,
          class Alloc = __gnu_cxx::allocator<const Key> >
class unordered_set : public __gnu_cxx::hash_set<Key, Hash, Pred, Alloc> {
public:
    typedef __gnu_cxx::hash_set<Key, Hash, Pred, Alloc> _Base;
    template <class InputIterator>
    unordered_set(InputIterator first, InputIterator last) : _Base(first, last) {}
    unordered_set(const _Base &ums) : _Base(ums) {}
    unordered_set() : _Base() {}
}; // class unordered_set
} // namespace std
# endif
#elif defined(_MSC_VER)
# if _MSC_VER >= 1600
#  include <unordered_set>
# elif _MSC_VER >= 1310 // 2002 and prior don't have std::hash_set<>
#  include <hash_set>
namespace std {
template <class Key,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>,
          class Alloc = std::allocator<const Key> >
class unordered_multiset
        : public stdext::hash_multiset<Key, __unordered_hash<Key, Hash, Pred>, Alloc> {
public:
    template <class InputIterator>
    unordered_multiset(InputIterator first, InputIterator last) : hash_multiset(first, last) {}
    unordered_multiset(const unordered_multiset &ums) : hash_multiset(ums) {}
    unordered_multiset() : hash_multiset() {}
}; // class unordered_multiset

template <class Key,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>,
          class Alloc = std::allocator<const Key> >
class unordered_set : public stdext::hash_set<Key, __unordered_hash<Key, Hash, Pred>, Alloc> {
public:
    template <class InputIterator>
    unordered_set(InputIterator first, InputIterator last) : hash_set(first, last) {}
    unordered_set(const unordered_set &ums) : hash_set(ums) {}
    unordered_set() : hash_set() {}
}; // class unordered_set
} // namespace std
# else
#  error Unsupported: std::unordered_set
# endif
#endif
#endif // FLINTER_TYPES_UNORDERED_SET_H
