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

// If libxml2 is detected, flinter/xml is built.
#if HAVE_LIBXML_XMLVERSION_H
#include <libxml/tree.h>
#include "flinter/xml.h"
#endif

namespace flinter {

#if HAVE_LIBXML_XMLVERSION_H
bool Tree::ParseFromXmlString(const std::string &xml, const char *encoding)
{
    Xml doc;
    if (!doc.Parse(xml, encoding)) {
        return false;
    }

    Clear();
    if (!doc.root()) {
        return true;
    }

    return ParseFromXmlInternal(doc.root());
}

bool Tree::ParseFromXml(struct _xmlDoc *xml)
{
    if (!xml) {
        return false;
    }

    Clear();
    xmlNode *root = xmlDocGetRootElement(xml);
    if (!root) {
        return true;
    }

    return ParseFromXmlInternal(root);
}

bool Tree::ParseFromXmlFile(const std::string &filename, const char *encoding)
{
    Xml doc;
    if (!doc.ParseFromFile(filename, encoding)) {
        return false;
    }

    Clear();
    if (!doc.root()) {
        return true;
    }

    return ParseFromXmlInternal(doc.root());
}

bool Tree::ParseFromXmlInternal(const struct _xmlNode *root)
{
    // XML allows nodes having the same name under the same parent.
    std::map<std::string, size_t> counts;
    for (xmlNode *p = root->children; p; p = p->next) {
        const char *key = reinterpret_cast<const char *>(p->name);
        if (!key) {
            continue;
        }

        ++counts[key];
    }

    std::map<std::string, size_t> indexes;
    for (xmlNode *p = root->children; p; p = p->next) {
        const char *k = reinterpret_cast<const char *>(p->name);
        std::string full_path;
        std::string key;

        if (k) {
            key = k;
            if (counts[k] > 1) {
                std::ostringstream s;
                s << '[' << indexes[k]++ << ']';
                key.append(s.str());
            }

            full_path = GetFullPath(key);
        }

        if (p->type == XML_ELEMENT_NODE) {
            if (key.empty()) {
                return false;
            }

            Tree *child = new Tree(full_path, key, std::string());
            _children.insert(std::make_pair(key, child));

            if (!child->ParseFromXmlInternal(p)) {
                return false;
            }

        } else if (p->type == XML_TEXT_NODE || p->type == XML_CDATA_SECTION_NODE) {
            char *value = reinterpret_cast<char *>(xmlNodeGetContent(p));
            if (value && *value) {
                SetValue(value);
            }
            xmlFree(value);
        }

        // Ignore all other element types.
    }

    return true;
}

bool Tree::SerializeToXmlString(std::string *serialized,
                                bool only_nodes_with_value,
                                const char *root_name,
                                const char *xmlns) const
{
    if (!serialized || !root_name || !*root_name) {
        return false;
    }

    xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
    if (!doc) {
        return false;
    }

    xmlNode *root = xmlNewNode(NULL, BAD_CAST root_name);
    if (!root) {
        xmlFreeDoc(doc);
        return false;
    }

    xmlDocSetRootElement(doc, root);

    if (xmlns && *xmlns) {
        xmlNs *ns = xmlNewNs(root, BAD_CAST xmlns, NULL);
        if (!ns) {
            xmlFreeDoc(doc);
            return false;
        }
    }

    bool empty;
    if (!SerializeToXmlInternal(root, only_nodes_with_value, &empty)) {
        xmlFreeDoc(doc);
        return false;
    }

    int size;
    xmlChar *buffer;
    xmlDocDumpFormatMemoryEnc(doc, &buffer, &size, "UTF-8", 1);
    if (!buffer) {
        xmlFreeDoc(doc);
        return false;
    }

    serialized->assign(buffer, buffer + size);
    xmlFree(buffer);
    xmlFreeDoc(doc);
    return true;
}

bool Tree::SerializeToXmlFile(const std::string &filename,
                              bool only_nodes_with_value,
                              const char *root_name,
                              const char *xmlns) const
{
    if (!root_name || !*root_name) {
        return false;
    }

    xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
    if (!doc) {
        return false;
    }

    xmlNode *root = xmlNewNode(NULL, BAD_CAST root_name);
    if (!root) {
        xmlFreeDoc(doc);
        return false;
    }

    xmlDocSetRootElement(doc, root);

    if (xmlns && *xmlns) {
        xmlNs *ns = xmlNewNs(root, BAD_CAST xmlns, NULL);
        if (!ns) {
            xmlFreeDoc(doc);
            return false;
        }
    }

    bool empty;
    if (!SerializeToXmlInternal(root, only_nodes_with_value, &empty)) {
        xmlFreeDoc(doc);
        return false;
    }

    if (xmlSaveFormatFileEnc(filename.c_str(), doc, "UTF-8", 1) < 0) {
        xmlFreeDoc(doc);
        return false;
    }

    xmlFreeDoc(doc);
    return true;
}

bool Tree::SerializeToXmlInternal(struct _xmlNode *root,
                                  bool only_nodes_with_value,
                                  bool *empty) const
{
    *empty = true;
    for (std::map<std::string, Tree *>::const_iterator p = _children.begin();
         p != _children.end(); ++p) {

        Tree *child = p->second;
        std::string key = child->_key;
        size_t pos = key.find('['); // Was a node with duplicated name.
        if (pos != std::string::npos) {
            key.resize(pos);
        }

        if (child->_children.empty()) { // Text node.
            if (child->_value.empty() && only_nodes_with_value) {
                continue;
            }

            if (!xmlNewTextChild(root, NULL,
                                 BAD_CAST key.c_str(),
                                 BAD_CAST child->_value.c_str())) {

                return false;
            }

        } else { // Element node.
            xmlNode *c = xmlNewNode(NULL, BAD_CAST key.c_str());
            if (!c) {
                return false;
            }

            bool e;
            if (!child->SerializeToXmlInternal(c, only_nodes_with_value, &e)) {
                xmlFreeNode(c);
                return false;
            }

            if (e && only_nodes_with_value) {
                xmlFreeNode(c);
                return true;
            }

            if (!xmlAddChild(root, c)) {
                xmlFreeNode(c);
                return false;
            }
        }

        *empty = false;
    }

    return true;
}
#else
bool Tree::ParseFromXmlInternal(const struct _xmlNode *)
{
    return false;
}
#endif // HAVE_LIBXML_XMLVERSION_H

} // namespace flinter
