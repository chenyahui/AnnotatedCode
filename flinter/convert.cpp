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

#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS
#include "flinter/convert.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include <algorithm>

namespace flinter {

#define DEFINE_S(type,min,max) \
template <> \
type convert(const std::string &from, const type &defval, bool *valid) \
{ \
    if (valid) { *valid = false; } \
    if (from.empty()) { return defval; } \
    const char *v = from.c_str(); \
    char *p; \
    errno = 0; \
    long long r = strtoll(v, &p, 0); \
    if (errno || p != v + from.length()) { return defval; } \
    if (r < (min) || r > (max)) { return defval; } \
    if (valid) { *valid = true; } \
    return static_cast<type>(r); \
}

#define DEFINE_U(type,max) \
template <> \
type convert(const std::string &from, const type &defval, bool *valid) \
{ \
    if (valid) { *valid = false; } \
    if (from.empty()) { return defval; } \
    const char *v = from.c_str(); \
    char *p; \
    errno = 0; \
    unsigned long long r = strtoull(v, &p, 0); \
    if (errno || p != v + from.length()) { return defval; } \
    if (r > (max)) { return defval; } \
    if (valid) { *valid = true; } \
    return static_cast<type>(r); \
}

#define DEFINE_D(type,min,max) \
template <> \
type convert(const std::string &from, const type &defval, bool *valid) \
{ \
    if (valid) { *valid = false; } \
    if (from.empty()) { return defval; } \
    const char *v = from.c_str(); \
    char *p; \
    errno = 0; \
    long double r = strtold(v, &p); \
    if (errno || p != v + from.length()) { return defval; } \
    if (r < (min) || r > (max)) { return defval; } \
    if (valid) { *valid = true; } \
    return static_cast<type>(r); \
}

#define DEFINE_TO_S(type,min,max) DEFINE_S(type,min,max)
#define DEFINE_TO_U(type,max)     DEFINE_U(type,max)
#define DEFINE_TO_D(type,min,max) DEFINE_D(type,min,max)

DEFINE_TO_S(         char     ,  SCHAR_MIN, SCHAR_MAX);
DEFINE_TO_S(         short    ,   SHRT_MIN,  SHRT_MAX);
DEFINE_TO_S(         int      ,    INT_MIN,   INT_MAX);
DEFINE_TO_S(         long     ,   LONG_MIN,  LONG_MAX);
DEFINE_TO_S(         long long,  LLONG_MIN, LLONG_MAX);
DEFINE_TO_U(unsigned char     ,  UCHAR_MAX           );
DEFINE_TO_U(unsigned short    ,  USHRT_MAX           );
DEFINE_TO_U(unsigned int      ,   UINT_MAX           );
DEFINE_TO_U(unsigned long     ,  ULONG_MAX           );
DEFINE_TO_U(unsigned long long, ULLONG_MAX           );

DEFINE_TO_D(float      ,  FLT_MIN,  FLT_MAX);
DEFINE_TO_D(double     ,  DBL_MIN,  DBL_MAX);
DEFINE_TO_D(long double, LDBL_MIN, LDBL_MAX);

const char *convert(const std::string &from,
                    const char *defval,
                    bool *valid)
{
    if (valid) {
        *valid = !from.empty();
    }

    return from.empty() ? defval : from.c_str();
}

template <>
const char *convert(const std::string &from,
                    const char *const &defval,
                    bool *valid)
{
    return convert(from, defval, valid);
}

template <>
std::string convert(const std::string &from,
                    const std::string &defval,
                    bool *valid)
{
    if (valid) {
        *valid = !from.empty();
    }

    return from.empty() ? defval : from;
}

template <>
bool convert(const std::string &from, const bool &defval, bool *valid)
{
    if (valid) {
        *valid = true;
    }

    std::string v(from);
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    if (v.compare("true") == 0 ||
        v.compare("1")    == 0 ){

        return true;

    } else if (v.compare("false") == 0 ||
               v.compare("0")     == 0 ){

        return false;
    }

    if (valid) {
        *valid = false;
    }

    return defval;
}

} // namespace flinter
