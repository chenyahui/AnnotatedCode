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

#include "flinter/fastcgi/common_filters.h"

#include <string.h>

#include <set>
#include <sstream>
#include <stdexcept>

#include <json/json.h>

#include "flinter/fastcgi/cgi.h"
#include "flinter/fastcgi/default_handlers.h"
#include "flinter/fastcgi/http_exception.h"
#include "flinter/explode.h"

namespace flinter {

Filter::Result HttpsFilter::Process(CGI *cgi)
{
    if (cgi->_SERVER["REQUEST_SCHEME"].compare("https") == 0) {
        return kResultPass;
    }

    uint16_t port = _port;
    if (port == 0) {
        port = 443;
    }

    std::string hostname = cgi->_SERVER["HTTP_HOST"];
    size_t pos = hostname.find(':');
    if (pos != std::string::npos) {
        hostname = hostname.substr(0, pos);
    }

    std::ostringstream s;
    s << "https://" << hostname;
    if (port != 443) {
        s << ":" << port;
    }

    s << cgi->_SERVER["REQUEST_URI"];
    std::string url = s.str();

    cgi->Redirect(url);
    return kResultFail;
}

Filter::Result NoCacheFilter::Process(CGI *cgi)
{
    cgi->SetHeader("Cache-Control", "private, no-cache, no-store, must-revalidate, max-age=0");
    cgi->SetHeader("Pragma", "no-cache");
    return kResultPass;
}

AllowedMethodsFilter::AllowedMethodsFilter(const std::string &methods)
{
    explode(methods, ",", &_methods);
}

DeniedMethodsFilter::DeniedMethodsFilter(const std::string &methods)
{
    explode(methods, ",", &_methods);
}

Filter::Result AllowedMethodsFilter::Process(CGI *cgi)
{
    if (_methods.find(cgi->_SERVER["REQUEST_METHOD"]) == _methods.end()) {
        throw HttpException(405);
    }
    return kResultPass;
}

Filter::Result DeniedMethodsFilter::Process(CGI *cgi)
{
    if (_methods.find(cgi->_SERVER["REQUEST_METHOD"]) != _methods.end()) {
        throw HttpException(405);
    }
    return kResultPass;
}

Filter::Result AllowedContentTypeFilter::Process(CGI *cgi)
{
    const size_t alen = _allowed.length();
    const std::string &t = cgi->_SERVER["CONTENT_TYPE"];
    if (t.length() < alen                                       ||
        (t.length() > alen && t[alen] != ';')                   ||
        t.substr(0, _allowed.length()).compare(_allowed)        ){

        throw HttpException(400);
    }

    return kResultPass;
}

} // namespace flinter
