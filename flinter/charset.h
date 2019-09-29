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

#ifndef FLINTER_CHARSET_H
#define FLINTER_CHARSET_H

#include <stdint.h>

#include <string>
#include <vector>

namespace flinter {

/*
 * UTF-8 is recommended for all on wire transmissions.
 *
 * If data type is of std::string, it can go on wire, but not suitable for
 * internal processing (except UTF-8).
 *
 * Note that UTF-32 is the only encoding that is of fixed width.
 *
 * Avoid GB18030.
 * Never GBK.
 */

// On wire encoding conversions.
extern int charset_utf8_to_gb18030(const std::string &u8, std::string *gb);
extern int charset_gb18030_to_utf8(const std::string &gb, std::string *u8);

extern int charset_utf8_to_gbk(const std::string &u8, std::string *gb);
extern int charset_gbk_to_utf8(const std::string &gb, std::string *u8);

extern int charset_utf8_to_json(const std::string &u8, std::string *json);

// Internal conversions.
extern int charset_utf8_to_utf16(const std::string &u8,
                                 std::basic_string<uint16_t> *u16);

extern int charset_utf8_to_utf32(const std::string &u8,
                                 std::basic_string<uint32_t> *u32);

extern int charset_utf16_to_utf8(const std::basic_string<uint16_t> &u16,
                                 std::string *u8);

extern int charset_utf16_to_utf32(const std::basic_string<uint16_t> &u16,
                                  std::basic_string<uint32_t> *u32);

extern int charset_utf32_to_utf8(const std::basic_string<uint32_t> &u32,
                                 std::string *u8);

extern int charset_utf32_to_utf16(const std::basic_string<uint32_t> &u32,
                                  std::basic_string<uint16_t> *u16);

} // namespace flinter

#endif /* FLINTER_CHARSET_H */
