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

#ifndef FLINTER_CRAWLER_H
#define FLINTER_CRAWLER_H

#include <deque>
#include <map>
#include <sstream>
#include <string>

namespace flinter {

class Crawler {
public:
    /// @param url actual url Crawler going to crawl.
    /// @param hostname DNS is not working or you're doing something tricky.
    explicit Crawler(const std::string &url,
                     const std::string &hostname = std::string());

    ~Crawler();
    Crawler();

    /*** Can be invoked before or after the first request ***/

    bool SetUrl(const std::string &url);

    /*** Can be invoked only before the first request ***/

    void SetTimeout(long timeout);
    void SetConnectTimeout(long timeout);
    void SetHostname(const std::string &hostname);

    // Only verify peer if set.
    void SetCertificateAuthorityFile(const std::string &filename);

    bool Get(std::string *result);
    bool Post(std::string *result);

    // Nothing in Set() will be used, however SetHeader() still works.
    bool PostRaw(const std::string &content_type,
                 const std::string &raw,
                 std::string *result);

    // Remove all POST fields and custom headers.
    void Clear();

    template <class T>
    void Set(const std::string &key, const T &value);
    void Set(const std::string &key, const char *value);
    void Set(const std::string &key, const std::string &value);

    template <class T>
    void SetHeader(const std::string &key, const T &value);
    void SetHeader(const std::string &key, const char *value);
    void SetHeader(const std::string &key, const std::string &value);

    /*** Can be invoked only after the first request ***/

    const std::string &effective_url() const
    {
        return _effective_url;
    }

    const std::string &content_type() const
    {
        return _content_type;
    }

    long status() const
    {
        return _status;
    }

private:
    class Context;
    explicit Crawler(const Crawler &other);
    Crawler &operator = (const Crawler &other);

    static size_t WriteFunction(char *ptr, size_t size, size_t nmemb, void *userdata);
    static bool IsBlacklisted(const std::string &header);

    bool Initialize(const std::string &content_type = std::string());
    bool Request(std::string *result);
    bool SetMethod(bool get_or_post);

    std::map<std::string, std::string> _headers;
    std::map<std::string, std::string> _posts;
    std::deque<char> _result;
    std::string _hostname;
    long _connect_timeout;
    std::string _cainfo;
    std::string _url;
    long _timeout;

    std::string _effective_url;
    std::string _content_type;
    long _status;

    Context *_context;

}; // class Crawler

template <class T>
void Crawler::Set(const std::string &key, const T &value)
{
    std::ostringstream s;
    s << value;
    Set(key, s.str());
}

template <class T>
void Crawler::SetHeader(const std::string &key, const T &value)
{
    std::ostringstream s;
    s << value;
    SetHeader(key, s.str());
}

} // namespace flinter

#endif // FLINTER_CRAWLER_H
