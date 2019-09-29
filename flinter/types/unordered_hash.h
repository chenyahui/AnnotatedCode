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

#ifndef FLINTER_TYPES_UNORDERED_HASH_H
#define FLINTER_TYPES_UNORDERED_HASH_H

#if defined(_MSC_VER)
# if _MSC_VER < 1310 // 2002 and prior don't have std::hash_xxx<>
#  error Unsupported: std::unordered_hash
# endif
# if _MSC_VER < 1500 // 2005 and prior don't have std::hash<>
namespace std {
template <class Key>
class hash {
public:
    size_t operator()(const Key &key) const
    {
        return stdext::hash_value(key);
    }
}; // class hash
} // namespace std
# endif
# if _MSC_VER < 1600 // 2008 and prior don't have std::unordered_xxx<>
namespace std {
template<class Key, class Hash, class Pred>
class __unordered_hash {
public:
    enum {
        bucket_size = 4,
        min_buckets = 8,
    };

    size_t operator()(const Key &key) const
    {
        return Hash()(key);
    }

    bool operator()(const Key &a, const Key &b) const
    {
        return Pred()(a, b);
    }

}; // class _unordered_hash
} // namespace std
# endif
#endif

#endif // FLINTER_TYPES_UNORDERED_HASH_H
