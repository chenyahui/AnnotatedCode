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

#ifndef FLINTER_FASTCGI_COMMON_FILTER_H
#define FLINTER_FASTCGI_COMMON_FILTER_H

#include <stdint.h>

#include <set>
#include <string>

#include <flinter/fastcgi/filter.h>

namespace flinter {

class Ajax;
class Passport;

class HttpsFilter : public Filter {
public:
    explicit HttpsFilter(uint16_t port = 0): _port(port) {}
    virtual ~HttpsFilter() {}
    virtual Result Process(CGI *cgi);
private:
    uint16_t _port;
}; // class HttpsFilter

class NoCacheFilter : public Filter {
public:
    virtual ~NoCacheFilter() {}
    virtual Result Process(CGI *cgi);
}; // class NoCacheFilter

// Only allow given methods, like "GET,POST".
class AllowedMethodsFilter : public Filter {
public:
    explicit AllowedMethodsFilter(const std::string &methods);
    virtual ~AllowedMethodsFilter() {}
    virtual Result Process(CGI *cgi);
private:
    std::set<std::string> _methods;
}; // class AllowedMethodsFilter

// Only allow given content type, like "text/plain", regardless of charset.
class AllowedContentTypeFilter : public Filter {
public:
    explicit AllowedContentTypeFilter(const std::string &allowed)
            : _allowed(allowed) {}
    virtual ~AllowedContentTypeFilter() {}
    virtual Result Process(CGI *cgi);
private:
    std::string _allowed;
}; // class AllowedContentTypeFilter

// Deny given methods and allow all others.
class DeniedMethodsFilter : public Filter {
public:
    explicit DeniedMethodsFilter(const std::string &methods);
    virtual ~DeniedMethodsFilter() {}
    virtual Result Process(CGI *cgi);
private:
    std::set<std::string> _methods;
}; // class DeniedMethodsFilter

} // namespace flinter

#endif // FLINTER_FASTCGI_COMMON_FILTER_H
