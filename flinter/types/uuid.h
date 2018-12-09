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

#ifndef FLINTER_TYPES_UUID_H
#define FLINTER_TYPES_UUID_H

#include <string>

namespace flinter {

/**
 * Universally Unique Identifier (UUID).
 */
class Uuid {
public:
    /// Helper method returning a random UUID object.
    static Uuid CreateRandom();
    static bool IsValidString(const char *str);
    static bool IsValidString(const std::string &str);

    Uuid();                                     ///< Construct a null UUID.
    explicit Uuid(const char *value);           ///< Will abort() if invalid.
    explicit Uuid(const std::string &value);    ///< Will abort() if invalid.
    Uuid(const Uuid &other);                    ///< Copy constructor.
    ~Uuid();                                    ///< Destructor.

    /// Generate a random UUID.
    void Generate();

    /// Make a UUID all zeros.
    void Clear();

    /// Parse a UUID in string format, 8-4-4-4-12 without braces.
    bool Parse(const char *value);
    bool Parse(const std::string &value);
    Uuid &operator = (const Uuid &other);

    /// Convert a UUID to its string format, 8-4-4-4-12 without braces.
    std::string str() const;

    /// Convert a UUID to its binary form, 16 bytes array, same order as human reading.
    void Save(void *buffer) const;

    /// Convert a UUID from its binary form, 16 bytes array, same order as human reading.
    void Load(const void *buffer);

    /// @retval false when the UUID is all zeros.
    operator bool() const;

    /// @retval true when the UUID is all zeros.
    bool IsNull() const;

    /// @retval <0 if this is lesser than the other.
    /// @retval  0 if this is equal to the other.
    /// @retval >0 if this is greater than the other.
    int Compare(const Uuid &other) const;

    /// Helper method.
    bool operator < (const Uuid &other) const
    {
        return Compare(other) < 0;
    }

    /// Helper method.
    bool operator > (const Uuid &other) const
    {
        return Compare(other) > 0;
    }

    /// Helper method.
    bool operator <= (const Uuid &other) const
    {
        return Compare(other) <= 0;
    }

    /// Helper method.
    bool operator >= (const Uuid &other) const
    {
        return Compare(other) >= 0;
    }

    /// Helper method.
    bool operator == (const Uuid &other) const
    {
        return Compare(other) == 0;
    }

    /// Helper method.
    bool operator != (const Uuid &other) const
    {
        return Compare(other) != 0;
    }

    static const size_t kBinaryLength = 16; ///< Length of binary form, used in Save().
    static const size_t kStringLength = 36; ///< Length of string form, used in str().

private:
    class Context;
    Context *_context;

}; // class Uuid

/// Helper method returning a random UUID object.
inline Uuid Uuid::CreateRandom()
{
    Uuid u;
    u.Generate();
    return u;
}

} // namespace flinter

#endif // FLINTER_TYPES_UUID_H
