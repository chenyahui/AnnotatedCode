/* Copyright 2016 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_TYPES_DECIMAL_H
#define FLINTER_TYPES_DECIMAL_H

#include <string>

namespace flinter {

class Decimal {
public:
    Decimal();
    ~Decimal();

    enum Rounding {
        kRoundingTiesToEven             , // Best accuracy
        kRoundingTiesAwayFromZero       , // Round up
        kRoundingTowardsZero            , // Truncate
        kRoundingTowardsPositiveInfinity, // Ceil
        kRoundingTowardsNegativeInfinity, // Floor
    };

    // Always pass in valid strings or you crash.
    /* explicit */Decimal(const std::string &s);

    // Always pass in valid strings or you crash.
    /* explicit */Decimal(const char *s);

    /* explicit */Decimal(unsigned long long s);
    /* explicit */Decimal(unsigned short s);
    /* explicit */Decimal(unsigned char s);
    /* explicit */Decimal(unsigned long s);
    /* explicit */Decimal(unsigned int s);
    /* explicit */Decimal(long long s);
    /* explicit */Decimal(short s);
    /* explicit */Decimal(char s);
    /* explicit */Decimal(long s);
    /* explicit */Decimal(int s);

    // When str() doesn't specify scale, or you're using / operator.
    void set_default_scale(int default_scale);
    int default_scale() const;

    // When str() or you're using / operator.
    void set_default_rounding(const Rounding &default_rounding);
    const Rounding &default_rounding() const;

    bool Parse(const std::string &s);
    std::string str(int scale = -1) const;

    /// @retval <0 result is lesser than actual value.
    /// @retval  0 result is exact.
    /// @retval >0 result is greater than actual value.
    int Serialize(std::string *s, int scale,
                  const Rounding &rounding = kRoundingTiesToEven) const;

    bool zero() const;
    bool positive() const;
    bool negative() const;

    void Add(const Decimal &o);
    void Sub(const Decimal &o);
    void Mul(const Decimal &o);

    /// @retval <0 result is lesser than actual value.
    /// @retval  0 result is exact.
    /// @retval >0 result is greater than actual value.
    int Div(const Decimal &o, int scale,
            const Rounding &rounding = kRoundingTiesToEven);

    Decimal &operator += (const Decimal &o);
    Decimal &operator -= (const Decimal &o);
    Decimal &operator *= (const Decimal &o);
    Decimal &operator /= (const Decimal &o);

    Decimal operator + (const Decimal &o) const;
    Decimal operator - (const Decimal &o) const;
    Decimal operator * (const Decimal &o) const;
    Decimal operator / (const Decimal &o) const;
    Decimal operator - () const;

    bool operator != (const Decimal &o) const;
    bool operator == (const Decimal &o) const;
    bool operator >= (const Decimal &o) const;
    bool operator <= (const Decimal &o) const;
    bool operator < (const Decimal &o) const;
    bool operator > (const Decimal &o) const;
    Decimal &operator = (const Decimal &o);
    int compare(const Decimal &o) const;
    Decimal(const Decimal &o);
    int scale() const;

protected:
    static void PrintOne(std::string *s, bool negative, int scale);
    static void PrintZero(std::string *s, int scale);

    int SerializeWithRounding(std::string *s, int scale,
                              const Rounding &rounding) const;

    int SerializeWithAppending(std::string *s, int scale) const;
    void Upscale(int s);
    void Cleanup();

private:
    class Context;
    Context *const _context;

}; // class Decimal

} // namespace flinter

#endif // FLINTER_TYPES_DECIMAL_H
