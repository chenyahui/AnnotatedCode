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

#include "flinter/types/uint128_t.h"

#include <ctype.h>
#include <stdio.h>

namespace flinter {

typedef int __compile_assertion__[sizeof(uint64_t) == sizeof(long long) ? 1 : -1];
typedef int __compile_assertion__[sizeof(uint128_t) == sizeof(uint64_t) * 2 ? 1 : -1];

/// Implement the standard math operator.
int uint128_t::Compare(const uint128_t &other) const
{
    if (this == &other) {
        return 0;
    }

    if (_high < other._high) {
        return -1;
    } else if (_high == other._high) {
        if (_low < other._low) {
            return -1;
        } else if (_low == other._low) {
            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
}

/// Serialize as hex string.
std::string uint128_t::ToHexString() const
{
    char buffer[40];
    long long high = static_cast<long long>(_high);
    long long low  = static_cast<long long>(_low);
    snprintf(buffer, sizeof(buffer), "%016llx%016llx", high, low);
    return buffer;
}

/// Parse from hex string.
/// Any characters beyond 32 characters are not checked.
bool uint128_t::FromHexString(const char *hex)
{
    if (!hex) {
        return false;
    }

    for (size_t i = 0; i < HEX_STRING_LENGTH; ++i) {
        if (!isxdigit(hex[i])) {
            return false;
        }
    }

    long long high;
    long long low;
    if (sscanf(hex, "%016llx%016llx", &high, &low) != 2) {
        return false;
    }

    _high = static_cast<uint64_t>(high);
    _low  = static_cast<uint64_t>(low);
    return true;
}

} // namespace flinter
