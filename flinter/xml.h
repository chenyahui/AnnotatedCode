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

#ifndef FLINTER_XML_H
#define FLINTER_XML_H

#include <string>

struct _xmlDoc;
struct _xmlNode;

namespace flinter {

/* Parse (and optionally validate against a XML schema) a document that's not empty. */
class Xml {
public:
    static bool Initialize();
    static void Shutdown();

    Xml();
    ~Xml();
    void Reset();

    bool Parse(const std::string &xml,
               const char *encoding = NULL);

    bool ParseFromFile(const std::string &xml_file,
                       const char *encoding = NULL);

    bool Parse(const std::string &xml,
               const std::string &schema,
               const char *encoding = NULL);

    bool ParseFromFile(const std::string &xml_file,
                       const std::string &schema,
                       const char *encoding = NULL);

    operator bool() const { return !!_root; }
    struct _xmlDoc *doc() const { return _doc; }
    struct _xmlNode *root() const { return _root; }

private:
    bool DoParse(const std::string &xml,
                 bool memory_or_file,
                 const std::string *schema,
                 const char *encoding);

    class Validator;
    struct _xmlDoc *_doc;
    struct _xmlNode *_root;

}; // class XMLDocument

} // namespace flinter

#endif // FLINTER_XML_H
