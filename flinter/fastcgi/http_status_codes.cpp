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

#include "flinter/fastcgi/http_status_codes.h"

namespace flinter {

static const std::pair<int, const char *> kStatusCodes[] = {
    std::make_pair(100, "Continue"),
    std::make_pair(101, "Switching Protocols"),
    std::make_pair(102, "Processing"),
    std::make_pair(200, "OK"),
    std::make_pair(201, "Created"),
    std::make_pair(202, "Accepted"),
    std::make_pair(203, "Non-Authoritative Information"),
    std::make_pair(204, "No Content"),
    std::make_pair(205, "Reset Content"),
    std::make_pair(206, "Partial Content"),
    std::make_pair(207, "Multi-Status"),
    std::make_pair(208, "Already Reported"),
    std::make_pair(226, "IM Used"),
    std::make_pair(300, "Multiple Choises"),
    std::make_pair(301, "Moved Permanently"),
    std::make_pair(302, "Found"),
    std::make_pair(303, "See Other"),
    std::make_pair(304, "Not Modified"),
    std::make_pair(305, "Use Proxy"),
    std::make_pair(306, "Switch Proxy"),
    std::make_pair(307, "Temporary Redirect"),
    std::make_pair(308, "Permanent Redirect"),
    std::make_pair(400, "Bad Request"),
    std::make_pair(401, "Unauthorized"),
    std::make_pair(402, "Payment Required"),
    std::make_pair(403, "Forbidden"),
    std::make_pair(404, "Not Found"),
    std::make_pair(405, "Method Not Allowed"),
    std::make_pair(406, "Not Acceptable"),
    std::make_pair(407, "Proxy Authentication Required"),
    std::make_pair(408, "Request Timeout"),
    std::make_pair(409, "Conflict"),
    std::make_pair(410, "Gone"),
    std::make_pair(411, "Length Required"),
    std::make_pair(412, "Precondition Failed"),
    std::make_pair(413, "Request Entity Too Large"),
    std::make_pair(414, "Request-URI Too Long"),
    std::make_pair(415, "Unsupported Media Type"),
    std::make_pair(416, "Requested Range Not Satisfiable"),
    std::make_pair(417, "Expectation Failed"),
    std::make_pair(419, "Authentication Timeout"),
    std::make_pair(422, "Unprocessable Entity"),
    std::make_pair(423, "Locked"),
    std::make_pair(424, "Failed Dependency"),
    std::make_pair(424, "Method Failure"),
    std::make_pair(425, "Unordered Collection"),
    std::make_pair(426, "Upgrade Required"),
    std::make_pair(428, "Precondition Required"),
    std::make_pair(429, "Too Many Requests"),
    std::make_pair(431, "Request Header Fields Too Large"),
    std::make_pair(451, "Unavailable For Legal Reasons"),
    std::make_pair(500, "Internal Server Error"),
    std::make_pair(501, "Not Implemented"),
    std::make_pair(502, "Bad Gateway"),
    std::make_pair(503, "Service Unavailable"),
    std::make_pair(504, "Gateway Timeout"),
    std::make_pair(505, "HTTP Version Not Supported"),
    std::make_pair(506, "Variant Also Negotiates"),
    std::make_pair(507, "Insufficient Storage"),
    std::make_pair(508, "Loop Detected"),
    std::make_pair(510, "Not Extended"),
    std::make_pair(511, "Network Authentication Required"),
};

const std::map<int, const char *> kHttpStatusCodes(kStatusCodes,
        kStatusCodes + sizeof(kStatusCodes) / sizeof(*kStatusCodes));

} // namespace flinter
