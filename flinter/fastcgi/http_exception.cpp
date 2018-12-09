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

#include "flinter/fastcgi/http_exception.h"

#include <stdarg.h>
#include <stdio.h>

#include <sstream>

#include "flinter/fastcgi/http_status_codes.h"

namespace flinter {

HttpException::HttpException(int status_code) throw()
        : _status_code(status_code)
{
    Translate();
}

HttpException::HttpException(int status_code, const char *format, ...) throw()
        : _status_code(status_code)
{
    Translate();

    char buffer[2048];
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);
    if (ret > 0) {
        _detail.assign(buffer);
    }
}

HttpException::HttpException(const HttpException &other) throw()
        : std::exception(other)
        , _detail(other._detail)
        , _what(other._what)
        , _status_code(other._status_code)
{
    // Intended left blank.
}

HttpException &HttpException::operator =(const HttpException &other) throw()
{
    _status_code = other._status_code;
    _what = other._what;
    return *this;
}

void HttpException::Translate() throw()
{
    std::ostringstream s;
    s << _status_code << " ";
    std::map<int, const char *>::const_iterator p = kHttpStatusCodes.find(_status_code);
    if (p != kHttpStatusCodes.end()) {
        s << p->second;
    } else {
        s << "Internal Error";
    }
    _what = s.str();
}

} // namespace flinter
