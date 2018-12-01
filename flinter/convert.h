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

#ifndef FLINTER_CONVERT_H
#define FLINTER_CONVERT_H

#include <string>

namespace flinter {

template <class T>
T convert(const std::string &from, const T &defval = T(), bool *valid = NULL);

const char *convert(const std::string &from,
                    const char *defval = NULL,
                    bool *valid = NULL);

} // namespace flinter

#endif // FLINTER_CONVERT_H
