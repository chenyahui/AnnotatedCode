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

#include "flinter/fastcgi/cgi.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <ClearSilver/ClearSilver.h>

#include "flinter/fastcgi/http_exception.h"
#include "flinter/fastcgi/http_status_codes.h"
#include "flinter/fastcgi/filter.h"
#include "flinter/utility.h"

namespace flinter {
namespace {

static void ThrowNeoError(NEOERR *err) __attribute__ ((noreturn));
static void ThrowNeoError(NEOERR *err)
{
    STRING str;
    string_init(&str);
    nerr_error_string(err, &str);
    std::string what = str.buf;
    string_clear(&str);
    nerr_ignore(&err);

    throw HttpException(400, "ClearSilver exception: %s", what.c_str());
}

static void ThrowIfNeoError(NEOERR *err)
{
    if (err == STATUS_OK) {
        return;
    }

    ThrowNeoError(err);
}

static const char *const kForbiddenHeaders[] = { // Must be low case.
    "status",
    "location",
    "set-cookie",
    "content-type",
    "content-length",
    "transfer-encoding",
};

static const char *const kForbiddenCookies[] = { // Must be low case.
    "path",
    "domain",
    "expires",
    "max-age",
};

} // anonymous namespace

class CGI::Parser {
public:
    static NEOERR *Parse(::CGI *cgi, char *method, char *ctype, void *rock);

}; // class CGI::Parser

NEOERR *CGI::Parser::Parse(::CGI *, char *, char *, void *rock)
{
    CGI *cgi = reinterpret_cast<CGI *>(rock);

    int total = atoi32(cgi->_SERVER["CONTENT_LENGTH"].c_str());
    if (total < 0) {
        return nerr_raise(NERR_PARSE, "CGI: invalid Content-Length");
    } else if (total == 0) {
        return STATUS_OK;
    }

    // ClearSilver will parse these special content type so skip them.
    const char *type = cgi->_SERVER["CONTENT_TYPE"].c_str();
    if (strcmp(type, "application/x-www-form-urlencoded") == 0 || // Exact
        strncmp(type, "multipart/form-data", 19) == 0          ){ // Prefix

        return STATUS_OK;
    }

    int len = 0;
    do {
        int ret;
        char buffer[8192];
        cgiwrap_read(buffer, sizeof(buffer), &ret);
        if (ret <= 0) {
            break;
        }

        cgi->_request_body.append(buffer, buffer + ret);
        len += ret;

    } while (len < total);

    if (len != total) {
        return nerr_raise(NERR_IO, "Short read on CGI POST input (%d < %d)", len, total);
    }

    return STATUS_OK;
}
void CGI::TranslateEnvvars(Tree *maps)
{
    char *k;
    char *v;
    int i;

    maps->Clear();
    i = 0;
    while (true) {
        NEOERR *err = cgiwrap_iterenv(i++, &k, &v);
        ThrowIfNeoError(err);
        if (!k) {
            break;
        }

        maps->Set(k, v);
        free(k);
        free(v);
    }
}

void CGI::TranslateHDF(void *cgi, const std::string &node, Tree *maps)
{
    ::CGI *c = reinterpret_cast< ::CGI *>(cgi);

    HDF *parent = hdf_get_obj(c->hdf, node.c_str());
    if (!parent) {
        return;
    }

    HDF *child = hdf_obj_child(parent);
    while (child) {
        const char *k = hdf_obj_name(child);
        const char *v = hdf_obj_value(child);
        if (k && v) {
            maps->Set(k, v);
        }

        child = hdf_obj_next(child);
    }
}

void CGI::TranslatePut(void *cgi, std::map<std::string, File> *files)
{
    TranslateFile(cgi, NULL, files);
}

void CGI::TranslatePost(void *cgi, Tree *posts,
                        std::map<std::string, File> *files)
{
    ::CGI *c = reinterpret_cast< ::CGI *>(cgi);

    HDF *parent = hdf_get_obj(c->hdf, "Query");
    if (!parent) {
        return;
    }

    HDF *next = hdf_obj_child(parent);
    while (next) {
        HDF *child = next;
        next = hdf_obj_next(next);

        const char *k = hdf_obj_name(child);
        const char *v = hdf_obj_value(child);
        if (!k || !v) {
            continue;
        }

        if (!TranslateFile(c, k, files)) {
            posts->Set(k, v);
        }
    }
}

bool CGI::TranslateFile(void *cgi, const char *key, std::map<std::string, File> *files)
{
    ::CGI *c = reinterpret_cast< ::CGI *>(cgi);

    FILE *fp = cgi_filehandle(c, key);
    if (!fp) {
        return false;
    }

    std::string path;
    if (key) {
        path = std::string("Query.") + key;
    } else {
        path = "PUT";
    }

    long size = -1;
    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
    }

    rewind(fp);
    File file;
    file._fp = fp;
    file._size = static_cast<size_t>(size);

    const char *name = hdf_get_value(c->hdf, path.c_str(), NULL);
    if (name) {
        file._name = name;
    }

    std::string stype = path + ".Type";
    const char *type = hdf_get_value(c->hdf, stype.c_str(), NULL);
    if (type) {
        file._type = type;
    }

    std::string sname = path + ".FileName";
    const char *tmp_name = hdf_get_value(c->hdf, sname.c_str(), NULL);
    if (tmp_name) {
        file._tmp_name = tmp_name;
    }

    files->insert(std::make_pair(key ? key : "", file));
    return true;
}

bool CGI::ProcessRequest()
{
    bool ready = true;

    try {
        for (std::list<Filter *>::const_iterator p = _filters.begin();
             p != _filters.end(); ++p) {

            Filter *filter = *p;
            Filter::Result ret = filter->Process(this);
            if (ret == Filter::kResultFail || ret == Filter::kResultNext) {
                ready = false;
            }

            if (ret == Filter::kResultFail || ret == Filter::kResultBreak) {
                break;
            }
        }

        if (ready) {
            Run();
        }

    } catch (const EndException &e) {
        // Just eat it, it's a normal end.
    }

    // In case that nothing has been outputted by now.
    _request_handled = true;
    FlushBody();
    return true;
}

CGI::CGI(bool process_body) : _GET(_request_queries)
                            , _POST(_request_posts)
                            , _COOKIE(_request_cookies)
                            , _SERVER(_server_variables)
                            , _REQUEST(_request_all)
                            , _FILES(_uploaded_files)
                            , BODY(this)
                            , _request_handled(false)
                            , _header_sent(false)
                            , _body_sent(false)
                            , _buffered(true)
                            , _status(200)
                            , _cgi_handle(NULL)
{
    void **cgiptr = &_cgi_handle;
    ::CGI * &cgi = *reinterpret_cast< ::CGI **>(cgiptr);
    NEOERR *err;

    TranslateEnvvars(&_server_variables);

    err = cgi_init(&cgi, NULL);
    ThrowIfNeoError(err);

    TranslateHDF(cgi, "Cookie", &_request_cookies);
    TranslateHDF(cgi, "Query",  &_request_queries);

    if (!process_body) {
        return;
    }

    err = cgi_register_parse_cb(cgi, "POST", "*", this, Parser::Parse);
    ThrowIfNeoError(err);

    // Purge Query subtree so GET values are gone.
    err = hdf_remove_tree(cgi->hdf, "Query");
    ThrowIfNeoError(err);

    err = cgi_parse(cgi);
    ThrowIfNeoError(err);

    if (_SERVER["REQUEST_METHOD"].compare("POST") == 0) {
        TranslatePost(cgi, &_request_posts, &_uploaded_files);

    } else if (_SERVER["REQUEST_METHOD"].compare("PUT") == 0) {
        TranslatePut(cgi, &_uploaded_files);
    }

    // Now we have both GET and POST data, POST overwrites GET.
    _request_all = _request_queries;
    _request_all.Merge(_request_posts, true);
}

CGI::~CGI()
{
    for (std::list<Filter *>::iterator p = _filters.begin(); p != _filters.end(); ++p) {
        delete *p;
    }

    void **cgiptr = &_cgi_handle;
    ::CGI * &cgi = *reinterpret_cast< ::CGI **>(cgiptr);
    cgi_destroy(&cgi);
}

void CGI::Redirect(const std::string &url, bool moved_permanently)
{
    SetHeaderInternal("Location", url);
    _status = moved_permanently ? 301 : 302;
}

void CGI::SetContentType(const std::string &type, const std::string &charset)
{
    if (charset.empty()) {
        SetHeaderInternal("Content-Type", type);
        return;
    }

    std::ostringstream s;
    s << type << "; charset=" << charset;
    SetHeaderInternal("Content-Type", s.str());
}

void CGI::SetHeader(const std::string &key, const std::string &value)
{
    if (key.empty() || IsForbiddenHeader(key)) {
        return;
    }

    SetHeaderInternal(key, value);
}

void CGI::SetHeaderInternal(const std::string &key,
                            const std::string &value,
                            bool allow_duplicate)
{
    if (_header_sent) { // Oops, programming error.
        throw std::logic_error("trying to set header after body");
    }

    if (value.empty()) {
        _headers.erase(key);
        return;
    }

    std::map<std::string, std::string>::iterator p = _headers.find(key);
    if (p == _headers.end() || allow_duplicate) {
        _headers.insert(std::make_pair(key, value));
    } else {
        p->second = value;
    }
}

void CGI::SetStatusCode(int status)
{
    if (kHttpStatusCodes.find(status) == kHttpStatusCodes.end()) {
        throw std::out_of_range("HTTP status code out of range");
    }

    _status = status;
}

bool CGI::IsForbiddenToken(const std::string &key, const char *const array[], size_t size)
{
    std::string c(key);
    std::string k = neos_strip(&c[0]);
    std::transform(k.begin(), k.end(), k.begin(), tolower);

    for (size_t i = 0; i < size; ++i) {
        const char *hay = array[i];
        if (k.compare(hay) == 0) {
            return true;
        }
    }

    return false;
}

bool CGI::IsForbiddenHeader(const std::string &key)
{
    return IsForbiddenToken(key, kForbiddenHeaders,
                            sizeof(kForbiddenHeaders) / sizeof(*kForbiddenHeaders));
}

bool CGI::IsForbiddenCookie(const std::string &key)
{
    return IsForbiddenToken(key, kForbiddenCookies,
                            sizeof(kForbiddenCookies) / sizeof(*kForbiddenCookies));
}

void CGI::RemoveCookie(const std::string &key,
                       const std::string &path,
                       const std::string &domain)
{
    // From PHP: 1 year and 1 second ago.
    SetCookie(key, "deleted", -31536001, path, domain);
}

void CGI::SetCookie(const std::string &key,
                    const std::string &value,
                    time_t max_age,
                    const std::string &path,
                    const std::string &domain,
                    const std::string &flags)
{
    if (key.empty() || IsForbiddenCookie(key)) {
        return;
    }

    std::ostringstream s;
    s << key << "=" << value;

    if (max_age) {
        s << "; max-age=" << max_age;

        time_t when = time(NULL) + max_age;
        struct tm tm;
        gmtime_r(&when, &tm);

        char buffer[128];
        strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        s << "; expires=" << buffer;
    }

    s << "; path=";
    if (!path.empty()) {
        s << path;
    } else {
        s << "/";
    }

    if (!domain.empty()) {
        s << "; domain=" << domain;
    }

    if (!flags.empty()) {
        s << "; " << flags;
    }

    SetHeaderInternal("Set-Cookie", s.str(), true);
}

void CGI::OutputStatusHeader()
{
    const char *str = kHttpStatusCodes.find(_status)->second;
    std::ostringstream s;
    s << "Status: " << _status << " " << str << "\n";
    RealOutput(s.str());
}

void CGI::OutputHeaders()
{
    if (_header_sent) {
        return;
    }

    _header_sent = true;
    OutputStatusHeader();
    for (std::multimap<std::string, std::string>::const_iterator
         p = _headers.begin(); p != _headers.end(); ++p) {

        std::ostringstream s;
        s << p->first.c_str() << ": " << p->second << "\n";
        RealOutput(s.str());
    }

    RealOutput("\n");
}

void CGI::OutputBodyF(const char *fmt, ...)
{
    if (!fmt || !*fmt) {
        return;
    }

    va_list va;
    va_start(va, fmt);
    OutputBodyV(fmt, va);
    va_end(va);
}

void CGI::OutputBodyV(const char *fmt, va_list va)
{
    if (!fmt || !*fmt) {
        return;
    }

    if (!_buffered) {
        OutputHeaders();
        _body_sent = true;
        RealOutputV(fmt, va);
        return;
    }

    char buffer[8192];
    size_t size;
    char *buf;
    int ret;

    ret = vsnprintf(buffer, sizeof(buffer), fmt, va);
    if (ret < 0) {
        throw std::runtime_error("vsnprintf(3)");
    } else if (ret == 0) {
        return;
    }

    size = static_cast<size_t>(ret);
    if (size < sizeof(buffer)) {
        OutputBody(buffer, size);
        return;
    }

    // Super large buffer?
    buf = new char[size + 1];
    ret = vsnprintf(buf, size + 1, fmt, va);
    if (static_cast<size_t>(ret) != size) {
        delete [] buf;
        throw std::runtime_error("vsnprintf(3)");
    }

    OutputBody(buf, size);
    delete [] buf;
}

void CGI::OutputBody(const void *body, size_t length)
{
    if (!body || !length) {
        return;
    }

    if (_buffered) {
        _buffered_body.insert(_buffered_body.end(),
                              reinterpret_cast<const char *>(body),
                              reinterpret_cast<const char *>(body) + length);

        return;
    }

    OutputHeaders();
    _body_sent = true;
    RealOutput(body, length);
}

void CGI::OutputBody(const std::string &body)
{
    OutputBody(body.data(), body.length());
}

void CGI::FlushBody()
{
    BODY.flush();
    if (_buffered_body.empty()) {
        OutputHeaders();
        return;
    }

    if (_request_handled) { // Known length.
        if (!_header_sent) {
            std::ostringstream s;
            s << _buffered_body.size();
            SetHeaderInternal("Content-Length", s.str());
        }

    } else { // Unknown length, chunked.
        if (!_header_sent) {
            SetHeaderInternal("Transfer-Encoding", "chunked");
        }
    }

    std::string body(_buffered_body.begin(), _buffered_body.end());
    _buffered_body.clear();

    OutputHeaders();
    _body_sent = true;
    RealOutput(body);
}

void CGI::SetBodyBuffered(bool buffered)
{
    _buffered = buffered;
    if (!buffered) {
        FlushBody();
    }
}

void CGI::RealOutput(const std::string &what)
{
    RealOutput(what.data(), what.length());
}

void CGI::RealOutputV(const char *fmt, va_list va)
{
    NEOERR *err = cgiwrap_writevf(fmt, va);
    ThrowIfNeoError(err);
}

void CGI::RealOutput(const void *what, size_t len)
{
    static const size_t kMaximum = INT_MAX / 2;

    const char *ptr = reinterpret_cast<const char *>(what);
    size_t now = len;

    NEOERR *err;
    while (now > kMaximum) {
        err = cgiwrap_write(ptr, kMaximum);
        ThrowIfNeoError(err);

        ptr += kMaximum;
        now -= kMaximum;
    }

    err = cgiwrap_write(ptr, (int)now);
    ThrowIfNeoError(err);
}

void CGI::AppendFilter(Filter *filter)
{
    if (filter) {
        _filters.push_back(filter);
    }
}

void CGI::End()
{
    throw EndException();
}

} // namespace flinter
