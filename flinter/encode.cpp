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

#include "flinter/encode.h"

#include <stdint.h>

#include <sstream>

namespace flinter {
namespace {

static int HexChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

} // anonymous namespace

int EncodeHex(const std::string &what, std::string *hex)
{
    static const char HEX[] = "0123456789ABCDEF";

    if (what.empty()) {
        hex->clear();
        return 0;
    }

    hex->resize(what.length() * 2);
    std::string::iterator p = hex->begin();
    for (std::string::const_iterator q = what.begin(); q != what.end(); ++q) {
        *p++ = HEX[static_cast<uint8_t>(*q) / 16];
        *p++ = HEX[static_cast<uint8_t>(*q) % 16];
    }

    return 0;
}

std::string EncodeHex(const std::string &what)
{
    std::string hex;
    if (EncodeHex(what, &hex)) {
        return std::string();
    }

    return hex;
}

int DecodeHex(const std::string &hex, std::string *what)
{
    size_t length = hex.length();
    if (!length || (length % 2)) {
        return -1;
    }

    what->resize(length / 2);
    std::string::iterator p = what->begin();
    for (size_t i = 0; i < hex.length(); i += 2) {
        int h = HexChar(hex[i]);
        int l = HexChar(hex[i + 1]);
        if (h < 0 || l < 0) {
            return -1;
        }

        *p++ = static_cast<char>(h * 16 + l);
    }

    return 0;
}

std::string DecodeHex(const std::string &hex)
{
    std::string what;
    if (DecodeHex(hex, &what)) {
        return std::string();
    }

    return what;
}

std::string EscapeHtml(const std::string &html, bool ie_compatible)
{
    if (html.empty()) {
        return html;
    }

    size_t extra = 0;
    for (std::string::const_iterator p = html.begin(); p != html.end(); ++p) {
        switch (*p) {
        case '<' :
        case '>' : extra += 3; break;
        case '&' : extra += 4; break;
        case '\'':
        case '"' : extra += 5; break;
        default  :             break;
        };
    }

    if (!extra) {
        return html;
    }

    std::string result;
    result.reserve(html.length() + extra);
    const char *apos = ie_compatible ? "&#039;" : "&apos;";
    for (std::string::const_iterator p = html.begin(); p != html.end(); ++p) {
        switch (*p) {
        case '<' : result += "&lt;"  ; break;
        case '>' : result += "&gt;"  ; break;
        case '&' : result += "&amp;" ; break;
        case '"' : result += "&quot;"; break;
        case '\'': result += apos    ; break;
        default  : result += *p      ; break;
        };
    }

    return result;
}

} // namespace flinter
