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

#include "flinter/charset.h"

#include <assert.h>
#include <stdio.h>

namespace flinter {

int charset_utf8_to_json(const std::string &utf, std::string *json)
{
    if (!json) {
        return -1;
    } else if (utf.empty()) {
        json->clear();
        return 0;
    }

    json->clear();
    std::basic_string<uint16_t> w;
    if (charset_utf8_to_utf16(utf, &w)) {
        return -1;
    }

    char buffer[8];
    for (std::basic_string<uint16_t>::const_iterator p = w.begin();
         p != w.end(); ++p) {

        switch (*p) {
        case 0x22: json->append("\\\""); continue;
        case 0x5C: json->append("\\\\"); continue;
        case 0x2F: json->append("\\/");  continue;
        case 0x08: json->append("\\b");  continue;
        case 0x0C: json->append("\\f");  continue;
        case 0x0A: json->append("\\n");  continue;
        case 0x0D: json->append("\\r");  continue;
        case 0x09: json->append("\\t");  continue;
        default:
            break;
        };

        if (*p >= 0x20 && *p <= 0x7E) {
            buffer[0] = static_cast<char>(*p);
            buffer[1] = '\0';
        } else {
            sprintf(buffer, "\\u%04X", *p);
        }

        json->append(buffer);
    }

    return 0;
}

} // namespace flinter
