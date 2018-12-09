/* Copyright 2017 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_FASTCGI_CUSTOM_DISPATCHER_H
#define FLINTER_FASTCGI_CUSTOM_DISPATCHER_H

#include <string>

namespace flinter {

class CustomDispatcher {
public:
    virtual ~CustomDispatcher() {}

    // @return empty string if no mapping can be made.
    virtual std::string RequestUriToHandlerPath(const std::string &request_uri) = 0;

}; // class CustomDispatcher

} // namespace flinter

#endif // FLINTER_FASTCGI_CUSTOM_DISPATCHER_H
