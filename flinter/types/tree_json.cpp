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

#include "flinter/types/tree.h"

#include "config.h"
#if HAVE_JSON_VALUE_H
#include <json/json.h>
#endif

namespace flinter {

#if HAVE_JSON_VALUE_H
bool Tree::ParseFromJsonString(const std::string &json)
{
    Json::Value j;
    Json::Reader reader;
    if (!reader.parse(json, j)) {
        return false;
    }

    return ParseFromJson(j);
}

bool Tree::ParseFromJson(const Json::Value &json)
{
    Clear();
    return ParseFromJsonInternal(json);
}

bool Tree::ParseFromJsonInternal(const Json::Value &json)
{
    size_t index = 0;
    for (Json::Value::const_iterator p = json.begin(); p != json.end(); ++p) {
        const Json::Value &j = *p;
        std::string key = p.name();
        if (key.empty()) {
            std::ostringstream s;
            s << '[' << index++ << ']';
            key = s.str();
        }

        std::string full_path = GetFullPath(key);
        if (j.isArray() || j.isObject()) { // Go recursively.
            Tree *child = new Tree(full_path, key, std::string());
            _children.insert(std::make_pair(key, child));

            if (!child->ParseFromJsonInternal(j)) {
                return false;
            }

            continue;
        }

        std::ostringstream s;
#ifdef JSON_HAS_INT64
        if (j.isInt64() ) { s << j.asInt64();   } else
        if (j.isUInt64()) { s << j.asUInt64();  } else
#endif
        if (j.isInt()   ) { s << j.asInt();     } else
        if (j.isUInt()  ) { s << j.asUInt();    } else
        if (j.isDouble()) { s << j.asDouble();  } else
                          { s << j.asString();  }

        const std::string &value = s.str();
        Tree *child = new Tree(full_path, key, value);
        _children.insert(std::make_pair(key, child));
    }

    return true;
}
#else
bool Tree::ParseFromJsonInternal(const Json::Value &)
{
    return false;
}
#endif // HAVE_JSON_VALUE_H

} // namespace flinter
