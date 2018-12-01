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

#include "flinter/xml.h"

#include <assert.h>

#include "config.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>

namespace flinter {

class Xml::Validator {
public:
    explicit Validator(const std::string &xml);
    ~Validator();

    operator bool() const { return !!_valid; }
    bool Validate(xmlDoc *doc) const;

private:
    xmlSchemaParserCtxt *_parser;
    xmlSchemaValidCtxt *_valid;
    xmlSchema *_schema;

}; // class Xml::Validator

Xml::Xml() : _doc(NULL), _root(NULL)
{
    // Intended left blank.
}

bool Xml::DoParse(const std::string &xml,
                  bool memory_or_file,
                  const std::string *schema,
                  const char *encoding)
{
    Reset();

    const char *enc = NULL;
    if (encoding && *encoding) {
        enc = encoding;
    }

    xmlDoc *doc;
    if (memory_or_file) {
        doc = xmlReadMemory(xml.data(), (int)xml.length(), NULL, enc, 0);
    } else {
        doc = xmlReadFile(xml.c_str(), enc, 0);
    }

    if (!doc) {
        return false;
    }

    if (schema) {
        Validator validator(*schema);
        if (!validator) {
            xmlFreeDoc(doc);
            return false;
        }

        if (!validator.Validate(doc)) {
            xmlFreeDoc(doc);
            return false;
        }
    }

    _doc = doc;
    _root = xmlDocGetRootElement(doc);
    return true;
}

bool Xml::ParseFromFile(const std::string &xml_file,
                        const std::string &schema,
                        const char *encoding)
{
    return DoParse(xml_file, false, &schema, encoding);
}

bool Xml::Parse(const std::string &xml,
                const std::string &schema,
                const char *encoding)
{
    return DoParse(xml, true, &schema, encoding);
}

bool Xml::ParseFromFile(const std::string &xml_file, const char *encoding)
{
    return DoParse(xml_file, false, NULL, encoding);
}

bool Xml::Parse(const std::string &xml, const char *encoding)
{
    return DoParse(xml, true, NULL, encoding);
}

void Xml::Reset()
{
    if (_doc) {
        xmlFreeDoc(_doc);
    }

    _root = NULL;
    _doc = NULL;
}

Xml::~Xml()
{
    Reset();
}

Xml::Validator::Validator(const std::string &xml) : _parser(NULL)
                                                  , _valid(NULL)
                                                  , _schema(NULL)
{
    xmlSchemaParserCtxt *parser =
            xmlSchemaNewMemParserCtxt(xml.data(), (int)xml.length());

    if (!parser) {
        return;
    }

    xmlSchema *schema = xmlSchemaParse(parser);
    if (!schema) {
        xmlSchemaFreeParserCtxt(parser);
        return;
    }

    xmlSchemaValidCtxt *valid = xmlSchemaNewValidCtxt(schema);
    if (!valid) {
        xmlSchemaFree(schema);
        xmlSchemaFreeParserCtxt(parser);
        return;
    }

    _parser = parser;
    _schema = schema;
    _valid  = valid;
}

Xml::Validator::~Validator()
{
    if (_valid) {
        xmlSchemaFreeValidCtxt(_valid);
    }

    if (_schema) {
        xmlSchemaFree(_schema);
    }

    if (_parser) {
        xmlSchemaFreeParserCtxt(_parser);
    }
}

bool Xml::Validator::Validate(xmlDoc *doc) const
{
    return xmlSchemaValidateDoc(_valid, doc) == 0;
}

bool Xml::Initialize()
{
    LIBXML_TEST_VERSION;
    xmlKeepBlanksDefault(0);
    return true;
}

void Xml::Shutdown()
{
    xmlCleanupParser();
}

} // namespace flinter
