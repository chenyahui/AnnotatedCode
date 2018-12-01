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

#include "flinter/fastcgi/default_handlers.h"

#include <algorithm>
#include <stdexcept>

#include "flinter/fastcgi/dispatcher.h"
#include "flinter/fastcgi/http_status_codes.h"
#include "flinter/encode.h"

namespace flinter {
namespace {

static void RunStandardHandler(CGI *cgi, int status_code)
{
    std::map<int, const char *>::const_iterator p = kHttpStatusCodes.find(status_code);
    if (p == kHttpStatusCodes.end()) {
        p = kHttpStatusCodes.find(500);
    }

    cgi->SetStatusCode(status_code);
    cgi->SetBodyBuffered(true);
    cgi->SetContentType("text/html", "iso-8859-1");

    std::string path = cgi->_SERVER["REQUEST_URI"];
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }

    std::string final = EscapeHtml(DecodeUrl(path));
    std::string admin = EscapeHtml(cgi->_SERVER["SERVER_ADMIN"]);
    std::string method = EscapeHtml(cgi->_SERVER["REQUEST_METHOD"]);
    std::ostream &BODY = cgi->BODY;

    BODY << "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
            "<html><head>\n"
            "<title>" << p->first << " " << p->second << "</title>\n"
            "</head><body>\n"
            "<h1>" << p->second << "</h1>\n";

    switch (p->first) {
    case 301:
    case 302:
    case 307:
    case 308:
        BODY << "<p>The document has moved <a href=\"" << final << "\">here</a>.</p>\n";
        break;

    case 303:
        BODY << "<p>The answer to your request is located <a href=\"" << final
             << "\">here</a>.</p>\n";
        break;

    case 305:
        BODY << "<p>This resource is only accessible through the proxy\n" << final
             << "<br />\nYou will need to configure your client to use that proxy.</p>\n";
        break;

    case 400:
        BODY << "<p>Your browser sent a request that this server could not understand.<br />\n";
        break;

    case 403:
    case 407:
        BODY << "<p>You don't have permission to access " << final
             << "\non this server.<br /></p>\n";
        break;

    case 404:
        BODY << "<p>The requested URL " << final << " was not found on this server.</p>\n";
        break;

    case 405:
        BODY << "<p>The requested method " << method
             << " is not allowed for the URL " << final << ".</p>\n";
        break;

    case 406:
        BODY << "<p>An appropriate representation of the requested resource "
             << final << " could not be found on this server.</p>\n";
        break;

    case 412:
        BODY << "<p>The precondition on the request for the URL " << final
             << " evaluated to false.</p>\n";
        break;

    case 415:
        BODY << "<p>The supplied request data is not in a format\n"
                 "acceptable for processing by this resource.</p>\n";
        break;

    case 416:
        BODY << "<p>None of the range-specifier values in the Range\n"
                "request-header field overlap the current extent\n"
                "of the selected resource.</p>\n";
        break;

    case 428:
        BODY << "<p>The request is required to be conditional.</p>\n";
        break;

    case 501:
        BODY << "<p>" << method << " to " << final << " not supported.<br /></p>\n";
        break;

    case 502:
        BODY << "<p>The proxy server received an invalid\n"
                "response from an upstream server.<br /></p>\n";
        break;

    case 503:
        BODY << "<p>The server is temporarily unable to service your\n"
                "request due to maintenance downtime or capacity\n"
                "problems. Please try again later.</p>\n";
        break;

    case 504:
        BODY << "<p>The gateway did not receive a timely response\n"
                "from the upstream server or application.</p>\n";
        break;

    case 500:
    default:
        BODY << "<p>The server encountered an internal error or\n"
                "misconfiguration and was unable to complete\n"
                "your request.</p>\n"
                "<p>Please contact the server administrator at \n"
                " " << admin << " to inform them of the time this error occurred,\n"
                " and the actions you performed just before this error.</p>\n"
                "<p>More information about this error may be available\n"
                "in the server error log.</p>\n";
    };

    BODY << "</body></html>\n";
}

} // anonymous namespace

void DefaultApacheHandler::Run()
{
    RunStandardHandler(this, status_code());
}

} // namespace flinter
