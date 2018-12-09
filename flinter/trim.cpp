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

#include "flinter/trim.h"

namespace flinter {

namespace {
const char *DELIMITER = " \t\r\n\f\v\b\a";
} // anonymous namespace

std::string trim(const std::string &string) {
    // This way removes trailing zero from string.
    std::string str = string.c_str();

    size_t strhead = str.find_first_not_of(DELIMITER);
    if (strhead == std::string::npos) {
        // Pure space string...
        return std::string();
    }

    size_t strtail = str.find_last_not_of(DELIMITER);
    return str.substr(strhead, strtail - strhead + 1);
}

std::string ltrim(const std::string &string) {
    // This way removes trailing zero from string.
    std::string str = string.c_str();

    size_t strhead = str.find_first_not_of(DELIMITER);
    if (strhead == std::string::npos) {
        // Pure space string...
        return std::string();
    } else {
        return str.substr(strhead);
    }
}

std::string rtrim(const std::string &string) {
    // This way removes trailing zero from string.
    std::string str = string.c_str();

    size_t strtail = str.find_last_not_of(DELIMITER);
    if (strtail == std::string::npos) {
        // Pure space string...
        return std::string();
    } else {
        return str.substr(0, strtail + 1);
    }
}

} // namespace flinter
