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

#ifndef FLINTER_FASTCGI_FILTER_H
#define FLINTER_FASTCGI_FILTER_H

namespace flinter {

class CGI;

class Filter {
public:
    enum Result {
        kResultPass,        // I'm good, keep going.
        kResultFail,        // Abort and treat as fail.
        kResultBreak,       // Abort and treat as success.
        kResultNext,        // Keep going but ultimately fail.
    };

    virtual ~Filter() {}

    // @return false to stop processor chain.
    virtual Result Process(CGI *cgi) = 0;

}; // class Filter

} // namespace flinter

#endif // FLINTER_FASTCGI_FILTER_H
