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

#ifndef FLINTER_TYPES_UINT128_T_H
#define FLINTER_TYPES_UINT128_T_H

#include <stdint.h>

#include <string>

namespace flinter {

/// This helper class is not of any namespaces.
class uint128_t {
public:

    /// Constructor.
    uint128_t() : _high(0), _low(0) {}

    /// Constructor.
    uint128_t(uint64_t high, uint64_t low) : _high(high), _low(low) {}

    /// Constructor.
    uint128_t(int64_t high, uint64_t low) : _high(*reinterpret_cast<uint64_t *>(&high))
                                          , _low(low) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(uint64_t low) : _high(0), _low(low) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(uint32_t low) : _high(0), _low(low) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(uint16_t low) : _high(0), _low(low) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(uint8_t  low) : _high(0), _low(low) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(int64_t  low) : _high(low < 0 ? static_cast<uint64_t>(-1) : 0)
                                          , _low(static_cast<uint64_t>(low)) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(int32_t  low) : _high(low < 0 ? static_cast<uint64_t>(-1) : 0)
                                          , _low(static_cast<uint64_t>(low)) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(int16_t  low) : _high(low < 0 ? static_cast<uint64_t>(-1) : 0)
                                          , _low(static_cast<uint64_t>(low)) {}

    /// Up conversion for ordinary types.
    /* explicit */uint128_t(int8_t   low) : _high(low < 0 ? static_cast<uint64_t>(-1) : 0)
                                          , _low(static_cast<uint64_t>(low)) {}

    /// Up conversion for ordinary types.
    uint128_t &operator = (uint64_t low)
    {
        _high = 0;
        _low = low;
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (uint32_t low)
    {
        _high = 0;
        _low = low;
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (uint16_t low)
    {
        _high = 0;
        _low = low;
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (uint8_t low)
    {
        _high = 0;
        _low = low;
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (int64_t low)
    {
        _high = low < 0 ? static_cast<uint64_t>(-1) : 0;
        _low = static_cast<uint64_t>(low);
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (int32_t low)
    {
        _high = low < 0 ? static_cast<uint64_t>(-1) : 0;
        _low = static_cast<uint64_t>(low);
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (int16_t low)
    {
        _high = low < 0 ? static_cast<uint64_t>(-1) : 0;
        _low = static_cast<uint64_t>(low);
        return *this;
    }

    /// Up conversion for ordinary types.
    uint128_t &operator = (int8_t  low)
    {
        _high = low < 0 ? static_cast<uint64_t>(-1) : 0;
        _low = static_cast<uint64_t>(low);
        return *this;
    }

    /// Implement the standard math operator.
    int Compare(const uint128_t &other) const;

    /// Serialize as hex string.
    std::string ToHexString() const;

    /// Parse from hex string.
    /// Any characters beyond 32 characters are not checked.
    bool FromHexString(const char *hex);

    /// Getter.
    uint64_t high() const
    {
        return _high;
    }

    /// Getter.
    uint64_t low() const
    {
        return _low;
    }

    /// Override the standard math operator.
    bool operator == (const uint128_t &other) const
    {
        return Compare(other) == 0;
    }

    /// Override the standard math operator.
    bool operator != (const uint128_t &other) const
    {
        return Compare(other) != 0;
    }

    /// Override the standard math operator.
    bool operator < (const uint128_t &other) const
    {
        return Compare(other) < 0;
    }

    /// Override the standard math operator.
    bool operator > (const uint128_t &other) const
    {
        return Compare(other) > 0;
    }

    /// Override the standard math operator.
    bool operator <= (const uint128_t &other) const
    {
        return Compare(other) <= 0;
    }

    /// Override the standard math operator.
    bool operator >= (const uint128_t &other) const
    {
        return Compare(other) >= 0;
    }

    /// Hex string length.
    static const size_t HEX_STRING_LENGTH = 32;

private:
    uint64_t _high;     ///< High part.
    uint64_t _low;      ///< Low part.

}; // class uint128_t

} // namespace flinter

#endif // FLINTER_TYPES_UINT128_T_H
