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

#ifndef FLINTER_FASTCGI_DEFAULT_HANDLERS_H
#define FLINTER_FASTCGI_DEFAULT_HANDLERS_H

#include <string>

#include <flinter/fastcgi/cgi.h>

namespace flinter {

class DefaultHandler : public CGI {
public:
    DefaultHandler() : CGI(false), _status_code(500) {}
    virtual ~DefaultHandler() {}

    void set_status_code(int status_code)
    {
        _status_code = status_code;
    }

protected:
    int status_code() const
    {
        return _status_code;
    }

private:
    int _status_code;

}; // class DefaultHandler

class DefaultApacheHandler : public DefaultHandler {
public:
    virtual ~DefaultApacheHandler() {}

protected:
    virtual void Run();

}; // class DefaultErrorHandler

class NotFoundHandler : public DefaultApacheHandler {
public:
    NotFoundHandler() { set_status_code(404); }
    virtual ~NotFoundHandler() {}

}; // class NotFoundHandler

} // namespace flinter

#endif // FLINTER_FASTCGI_DEFAULT_HANDLERS_H
