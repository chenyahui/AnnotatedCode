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

#ifndef FLINTER_COMMON_H
#define FLINTER_COMMON_H

/** __attribute__ is used by GCC and can be safely ignored if not supported. */
#ifndef __GNUC__
# define __attribute__(x)
#endif

/** Define NULL. */
#ifndef NULL
# ifdef __cplusplus
#  define NULL 0
# else /* __cplusplus */
#  define NULL ((void *)0)
# endif /* __cplusplus */
#endif

/** ARRAYSIZEOF determines the count of elements in an array. */
#undef ARRAYSIZEOF
#define ARRAYSIZEOF(a) \
    ((sizeof(a) / sizeof(*(a))) / \
    (size_t)(!(sizeof(a) % sizeof(*(a)))))

#ifdef __cplusplus
namespace flinter {
namespace internal {
template <bool>
struct CompileAssert {
};
} // namespace internal
} // namespace flinter
#define COMPILE_TIME_ASSERT(expr, msg) \
    typedef ::flinter::internal::CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]
#else
#define COMPILE_TIME_ASSERT(expr, msg) typedef int msg[(int)(expr) ? 1 : -1]
#endif

#ifdef __cplusplus

/** NON_COPYABLE disables copy constructings. */
#undef NON_COPYABLE
#define NON_COPYABLE(klass) \
private: \
    explicit klass(const klass &); \
    klass &operator = (const klass &);

#endif /* __cplusplus */

#if defined(_MSC_VER)
# ifdef _WIN64
#  define ssize_t __int64
# else
#  define ssize_t _W64 int
# endif
# define socklen_t ssize_t
#endif

#if defined(_MSC_VER)
# define gmtime_r(t,s) (gmtime_s(s,t) ? NULL : s)
# define localtime_r(t,s) (localtime_s(s,t) ? NULL : s)
#elif defined(__MINGW__)
# define gmtime_r(t,s) (*t = *gmtime(s))
# define localtime_r(t,s) (*t = *localtime(s))
#endif

#if defined(_MSC_VER)
# pragma warning (disable: 4996)
# define snprintf _snprintf
# define strtok_r strtok_s
#endif

#if defined(WIN32)
typedef void *HANDLE;
typedef int uid_t;
typedef int gid_t;
typedef int pid_t;
#else
# define INVALID_HANDLE_VALUE -1
typedef int HANDLE;
typedef int SOCKET;
#endif

#endif /* FLINTER_COMMON_H */
