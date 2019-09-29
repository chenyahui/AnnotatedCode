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

#ifndef FLINTER_FASTCGI_CGI_H
#define FLINTER_FASTCGI_CGI_H

#include <stdarg.h>
#include <time.h>

#include <deque>
#include <list>
#include <map>
#include <ostream>
#include <streambuf>
#include <string>

#include <flinter/types/read_only_map.h>
#include <flinter/types/tree.h>

namespace flinter {

class Dispatcher;
class Filter;
class Tree;

class CGI {
public:
    friend class Dispatcher;

    class File {
    public:
        File() : _size(0), _fp(NULL) {}

        const std::string &tmp_name() const { return _tmp_name; }
        const std::string &name() const     { return _name;     }
        const std::string &type() const     { return _type;     }
        size_t size() const                 { return _size;     }
        FILE *fp() const                    { return _fp;       }

    private:
        friend class CGI;

        std::string _tmp_name;
        std::string _name;
        std::string _type;
        size_t _size;
        FILE *_fp;

    }; // class File;

    virtual ~CGI();

    /*
     * Different behaviors compared to PHP.
     *
     * _REQUEST contains GET and POST data, POST value overwrites GET value if key is the
     * same, this is the same effect as GP in php.ini:request_order (default value). This
     * behavior is not tunable.
     *
     * $_ENV is not populated.
     *
     * Due to different implementation, $_FILES lacks: error. Instead, it has an additional
     * field: FILE *fp. tmp_name might be missing if ClearSilver is configured to unlink
     * file automatically.
     */
    const Tree &_GET;
    const Tree &_POST;
    const Tree &_COOKIE;
    const Tree &_SERVER;
    const Tree &_REQUEST;
    ReadOnlyMap<std::string, File> _FILES;

    /*
     * Only filled in if it's a POST request but the content type is NOT of
     *   application/x-www-form-urlencoded
     *   multipart/form-data
     */
    const std::string &request_body() { return _request_body; }

    class BodyStreamBuf : public std::streambuf {
    public:
        BodyStreamBuf(CGI *cgi) : _cgi(cgi) {}
        virtual ~BodyStreamBuf() {}

    protected:
        virtual int overflow(int c)
        {
            char n = static_cast<char>(c);
            _cgi->OutputBody(&n, 1);
            return traits_type::not_eof(c);
        }

        virtual std::streamsize xsputn(const char *s, std::streamsize n)
        {
            _cgi->OutputBody(s, static_cast<size_t>(n));
            return n;
        }

    private:
        CGI *_cgi;

    }; // class BodyStreamBuf

    class BodyStream : public std::ostream {
    public:
        friend class CGI;
        virtual ~BodyStream() {}
    private:
        BodyStream(CGI *cgi) : _buf(cgi) { init(&_buf); }
        BodyStreamBuf _buf;
    }; // class BodyStream

    // No escaping is done for all header operations.
    void SetContentType(const std::string &type,
                        const std::string &charset = std::string());

    void SetStatusCode(int status);
    void SetHeader(const std::string &key, const std::string &value);
    void Redirect(const std::string &url, bool moved_permanently = false);

    // Won't de-duplicate
    // If you set same `key` multiple times, it's up to browser what to do.
    void SetCookie(const std::string &key,
                   const std::string &value,
                   time_t max_age = 0, // 0 for session
                   const std::string &path = std::string(),
                   const std::string &domain = std::string(),
                   const std::string &flags = std::string());

    void RemoveCookie(const std::string &key,
                      const std::string &path = std::string(),
                      const std::string &domain = std::string());

    // If buffered, outputted body will hold in memory, and you can still set headers even
    // after body is buffered but not yet outputted.
    void SetBodyBuffered(bool buffered);

    // Output body immediately unless buffered, with format.
    void OutputBodyF(const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
    void OutputBodyV(const char *fmt, va_list va);

    // Output body immediately unless buffered.
    void OutputBody(const void *body, size_t length);
    void OutputBody(const std::string &body);

    // End processing normally.
    void End();

    // Output body immediately unless buffered, std::ostream.
    BodyStream BODY;

protected:
    virtual void Run() = 0;

    // @param filter life span taken, just new.
    void AppendFilter(Filter *filter);

    explicit CGI(bool process_body = true);

private:
    class Parser;

    class EndException : std::exception {
        // Nothing required.
    }; // class EndException

    static bool IsForbiddenHeader(const std::string &key);
    static bool IsForbiddenCookie(const std::string &key);

    static bool IsForbiddenToken(const std::string &key,
                                 const char *const array[],
                                 size_t size);

    static void TranslatePost(void *cgi, Tree *posts,
                              std::map<std::string, File> *files);

    // @return true if it's indeed a file, not a normal value.
    static bool TranslateFile(void *cgi, const char *key,
                              std::map<std::string, File> *files);

    static void TranslateHDF(void *cgi, const std::string &node, Tree *maps);
    static void TranslatePut(void *cgi, std::map<std::string, File> *files);
    static void TranslateEnvvars(Tree *maps);

    // Really send content on the wire.
    static void RealOutputV(const char *fmt, va_list va);
    static void RealOutput(const void *what, size_t len);
    static void RealOutput(const std::string &what);

    void SetHeaderInternal(const std::string &key,
                           const std::string &value,
                           bool allow_duplicate = false);

    void OutputStatusHeader();
    bool ProcessRequest();
    void OutputHeaders();
    void FlushBody();

    std::multimap<std::string, std::string> _headers;
    std::deque<char> _buffered_body;
    std::string _request_body;
    bool _request_handled;
    bool _header_sent;
    bool _body_sent;
    bool _buffered;
    int _status;

    Tree _request_queries;
    Tree _request_cookies;
    Tree _request_posts;
    Tree _request_all;
    Tree _server_variables;
    std::map<std::string, File> _uploaded_files;

    std::list<Filter *> _filters;

    // ClearSilver handle, weak type.
    void *_cgi_handle;

}; // class CGI

} // namespace flinter

#endif // FLINTER_FASTCGI_CGI_H
