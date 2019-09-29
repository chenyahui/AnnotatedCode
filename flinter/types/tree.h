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

#ifndef FLINTER_TYPES_TREE_H
#define FLINTER_TYPES_TREE_H

#include <stdint.h>

#include <map>
#include <ostream>
#include <sstream>
#include <string>

#include <flinter/convert.h>

namespace Json {
class Value;
} // namespace Json

struct _hdf;
struct _xmlDoc;
struct _xmlNode;

namespace flinter {

class Tree {
public:
    Tree();
    ~Tree();

    void Merge(const Tree &other, bool overwrite_existing_nodes);
    Tree &operator = (const Tree &other);
    explicit Tree(const Tree &other);

    template <class K, class V, class C, class A>
    explicit Tree(const std::map<K, V, C, A> &m);

    const Tree &Get(const std::string &path) const;
    Tree &Get(const std::string &path);

    bool Has(const std::string &path) const;
    void Clear();

    const std::string &key()       const { return _key;            }
    const std::string &value()     const { return _value;          }
    const std::string &full_path() const { return _full_path;      }

    // Make this thing look like a pair.
    const std::string &first;
    std::string &second;

    // Make this thing look like a string.
    operator std::string()         const { return _value;          }
    const char *c_str()            const { return _value.c_str();  }
    const char *data()             const { return _value.data();   }
    size_t length()                const { return _value.length(); }
    size_t size()                  const { return _value.size();   }
    bool empty()                   const { return _value.empty();  }

    int compare(const std::string &str) const { return _value.compare(str); }
    int compare(size_t pos, size_t len, const std::string &str) const
    {
        return _value.compare(pos, len, str);
    }

    template <class T>
    T key_as(const T &defval = T(), bool *valid = NULL) const
    {
        return convert<T>(_key, defval, valid);
    }

    const char *key_as(const char *defval = NULL, bool *valid = NULL) const
    {
        return convert(_key, defval, valid);
    }

    template <class T>
    T as(const T &defval = T(), bool *valid = NULL) const
    {
        return convert<T>(_value, defval, valid);
    }

    const char *as(const char *defval = NULL, bool *valid = NULL) const
    {
        return convert(_value, defval, valid);
    }

    const Tree &operator [] (const std::string &path) const
    {
        return Get(path);
    }

    Tree &operator [] (const std::string &path)
    {
        return Get(path);
    }

    template <class T>
    Tree &operator = (const T &value);

    template <class T>
    Tree &Set(const std::string &path, const T &value);

    template <class T>
    Tree &Set(const T &value);

    bool ParseFromHdfFile(const std::string &filename);
    bool ParseFromHdfString(const std::string &serialized);
    bool SerializeToHdfString(std::string *serialized,
                              bool only_nodes_with_value = true) const;

    bool SerializeToHdfFile(const std::string &filename,
                            bool only_nodes_with_value = true) const;

    bool ParseFromJson(const Json::Value &json);
    bool ParseFromJsonString(const std::string &json);

    bool ParseFromXml(struct _xmlDoc *xml);
    bool ParseFromXmlFile(const std::string &filename,
                          const char *encoding = NULL);

    bool ParseFromXmlString(const std::string &xml,
                            const char *encoding = NULL);

    bool SerializeToXmlString(std::string *serialized,
            bool only_nodes_with_value = true,
            const char *root_name = "root",
            const char *xmlns = "http://www.w3.org/XML/XMLSchema/v1.0") const;

    bool SerializeToXmlFile(const std::string &filename,
            bool only_nodes_with_value = true,
            const char *root_name = "root",
            const char *xmlns = "http://www.w3.org/XML/XMLSchema/v1.0") const;

    bool RenderTemplateFile(const std::string &filename,
                            std::ostream *out,
                            std::string *error = NULL) const;

    bool RenderTemplateString(const std::string &tmpl,
                              std::ostream *out,
                              std::string *error = NULL) const;

    template <class T, class P>
    class IteratorBase : public std::iterator<std::forward_iterator_tag, T> {
    public:
        friend class Tree;

        IteratorBase() {}
        template <class Q>
        IteratorBase(const Q &other): _p(other._p) {}

        const std::string &key() const { return _p->first; }
        T *operator -> () { return _p->second; }
        T &operator * () { return *_p->second; }

        IteratorBase operator ++ (int) { IteratorBase i; i._p = _p++; return i; }
        IteratorBase &operator ++ () { ++_p; return *this; }

        template <class Q>
        bool operator == (const Q &other) const { return _p == other._p; }

        template <class Q>
        bool operator != (const Q &other) const { return _p != other._p; }

        template <class Q>
        IteratorBase &operator = (const Q &other) { _p = other._p; return *this; }

    private:
        P _p;

    }; // class IteratorBase

    typedef IteratorBase<Tree, std::map<std::string, Tree *>::iterator> iterator;
    typedef IteratorBase<const Tree,
            std::map<std::string, Tree *>::const_iterator> const_iterator;

    size_t children_size() const { return _children.size(); }
    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();

protected:
    Tree *CreateOrSet(const std::string &path, const std::string &value);
    Tree(const std::string &full_path,
         const std::string &key,
         const std::string &value);

    const Tree *Find(const std::string &path) const;

    bool ParseFromXmlInternal(const struct _xmlNode *root);
    bool ParseFromJsonInternal(const Json::Value &json);
    bool ParseFromHdfInternal(struct _hdf *hdf);

    void SetValue(const std::string &value)
    {
        _value = value;
    }

private:
    std::string GetFullPath(const std::string &child) const;
    bool RenderTemplateInternal(const std::string &tmpl,
                                bool file_or_string,
                                std::ostream *out,
                                std::string *error) const;

    bool SerializeToXmlInternal(struct _xmlNode *root,
                                bool only_nodes_with_value,
                                bool *empty) const;

    bool SerializeToHdfInternal(struct _hdf *hdf, bool only_nodes_with_value) const;

    std::string _key;
    std::string _value;
    std::string _full_path;
    std::map<std::string, Tree *> _children;

}; // class Tree

template <class K, class V, class C, class A>
Tree::Tree(const std::map<K, V, C, A> &m)
{
    for (typename std::map<K, V, C, A>::const_iterator p = m.begin();
         p != m.end(); ++p) {

        Set(p->first, p->second);
    }
}

std::ostream &operator << (std::ostream &out, const Tree &tree);

template <class T>
Tree &Tree::operator = (const T &value)
{
    return Set(value);
}

template <class T>
Tree &Tree::Set(const std::string &path, const T &value)
{
    std::ostringstream s;
    s << value;
    CreateOrSet(path, s.str());
    return *this;
}

template <class T>
Tree &Tree::Set(const T &value)
{
    std::ostringstream s;
    s << value;
    SetValue(s.str());
    return *this;
}

} // namespace flinter

#endif // FLINTER_TYPES_TREE_H
