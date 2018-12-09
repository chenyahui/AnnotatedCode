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

#ifndef FLINTER_FASTCGI_HTTP_EXCEPTION_H
#define FLINTER_FASTCGI_HTTP_EXCEPTION_H

#include <string>

namespace flinter {

class HttpException : std::exception {
public:
    explicit HttpException(int status_code) throw();
    HttpException(int status_code, const char *format, ...) throw();
    virtual ~HttpException() throw() {}

    HttpException(const HttpException &other) throw();
    HttpException &operator =(const HttpException &other) throw();

    virtual const std::string &detail() const throw() { return _detail; }
    virtual const char *what() const throw() { return _what.c_str(); }
    int status_code() const throw() { return _status_code; }

private:
    void Translate() throw();

    std::string _detail;
    std::string _what;
    int _status_code;

}; // class HttpException

} // namespace flinter

#endif // FLINTER_FASTCGI_HTTP_EXCEPTION_H
