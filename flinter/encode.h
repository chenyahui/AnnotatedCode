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

#ifndef FLINTER_ENCODE_H
#define FLINTER_ENCODE_H

#include <string>

namespace flinter {

extern std::string EncodeUrl(const std::string &url);
extern std::string DecodeUrl(const std::string &url);

extern std::string EncodeHex(const std::string &what);
extern std::string DecodeHex(const std::string &hex);

extern int EncodeHex(const std::string &what, std::string *hex);
extern int DecodeHex(const std::string &hex, std::string *what);

extern int EncodeBase64(const std::string &input, std::string *output);
extern int DecodeBase64(const std::string &input, std::string *output);

extern std::string EscapeHtml(const std::string &html, bool ie_compatible = false);

} // namespace flinter

#endif // FLINTER_ENCODE_H
