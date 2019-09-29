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

#ifndef FLINTER_FASTCGI_DISPATCHER_H
#define FLINTER_FASTCGI_DISPATCHER_H

#include <stdarg.h>

#include <map>
#include <string>

#include <flinter/factory.h>
#include <flinter/singleton.h>

namespace flinter {

class CGI;
class CustomDispatcher;
class DefaultHandler;
class FixedThreadPool;
class HttpException;
class TLS;

class Dispatcher : public Singleton<Dispatcher> {
    DECLARE_SINGLETON(Dispatcher);

public:
    enum Mode {
        kModeAutomatic,
        kModeExecutable,
        kModeFastCGI,
        kModeCGI,
    }; // enum Mode

    const std::string &site_root() { return _site_root; }
    const Mode &mode() { return _mode; }

    // Must be in format of "/" or "/some/path/"
    static void set_site_root(const std::string &site_root);

    // Life span taken
    static void set_custom_dispatcher(CustomDispatcher *custom_dispatcher);

    // Should Dispatcher capture signals internally?
    // If not, call Dispatcher::on_sigterm() when you receive terminating signals, and try
    // to ignore SIGPIPE for your own sake.
    //
    // Only for FastCGI.
    // Only effective before you call main() below.
    // Default yes.
    static void set_handle_signals_internally(bool handle_internally);

    // If you're running a standalone FastCGI.
    // Default stdin.
    static void set_listen_fd(int fd);

    // Don't guess, do what I say.
    static void set_forced_mode(const Mode &mode);

    // If true, all requests are sent to the only handler even if URI mismatches.
    // Ignored if there's more than one handlers are registered.
    // Default yes.
    static void set_single_handler_mode(bool single_handler_mode);

    // Helper method to try to determine mode and call methods above.
    static int main(int argc, char *argv[]);

    // Unlike main() above, this method doesn't block the calling thread.
    static bool SpawnFastCGI(size_t threads);

    // SIGUSR1 will be sent to all threads within the pool, please make sure that you've
    // install the signal handler properly. Don't ignore it or we can't interrupt the
    // blocking accept(2) call. A empty (dummy) handler is good.
    static void Shutdown();

    // Don't call these register methods directly.
    // Use CGI_DISPATCH(class, path) or CGI_DISPATCH_DEFAULT(class) in your cpp file.
    void RegisterDefaultFactory(const Factory<DefaultHandler> *factory);
    void RegisterFactory(const Factory<CGI> &factory,
                         const std::string &path);

    static void on_sigterm(int signum);

    ~Dispatcher();

private:
    class Worker;
    class PassthroughDispatcher;

    static int   EmulatedRead   (void *context, char *buf, int buf_len);
    static int   EmulatedWrite  (void *context, const char *buf, int buf_len);
    static int   EmulatedWriteF (void *context, const char *fmt, va_list va);
    static char *EmulatedGetenv (void *context, const char *key);
    static int   EmulatedPutenv (void *context, const char *key, const char *value);
    static int   EmulatedIterenv(void *context, int i, char **key, char **value);

    static bool _handle_signals_internally;
    static bool _single_handler_mode;
    static bool _run;

    void ProcessException(const CGI *cgi, const HttpException &e);
    void InitializeClearSilverForFastCGI();
    void InitializeFastCGI();
    CGI *GetDefaultHandler();
    bool DispatchRequest();
    CGI *GetHandler();

    int RunAsExecutable(int argc, char *argv[]);
    int RunAsFastCGI(int argc, char *argv[]);
    int RunAsCGI(int argc, char *argv[]);

    int Run(int argc, char *argv[]);
    Dispatcher();

    static const std::map<std::string, const Factory<CGI> *> kSystemFactories;
    std::map<std::string, const Factory<CGI> *> _factories;
    const Factory<DefaultHandler> *_default_factory;
    CustomDispatcher *_custom_dispatcher;
    std::string _site_root;
    int _listen_fd;
    Mode _mode;

    // For SpawnFastCGI()
    FixedThreadPool *_sfc_pool;
    TLS *_sfc_tls;

}; // class Dispatcher

} // namespace flinter

#define __CGI_DISPATCH_LINE_NUMBER(x) x
#define _CGI_DISPATCH_LINE_NUMBER __CGI_DISPATCH_LINE_NUMBER(__LINE__)

#define _CGI_DISPATCH_CLASS_NAME(klass,suffix) \
klass##_cgi_dispatch_ClAsS_##suffix

#define _CGI_DISPATCH_OBJECT_NAME(klass,suffix) \
klass##_cgi_dispatch_ObJeCt_##suffix

#define _CGI_DISPATCH(klass,path,suffix) \
static class _CGI_DISPATCH_CLASS_NAME(klass,suffix) { \
public: \
    _CGI_DISPATCH_CLASS_NAME(klass,suffix)() { \
        ::flinter::Dispatcher::GetInstance()->RegisterFactory(_factory, (path)); \
    } \
private: \
    ::flinter::FactoryDirect< ::flinter::CGI, klass > _factory; \
} _CGI_DISPATCH_OBJECT_NAME(klass,suffix);

#define CGI_DISPATCH(klass,path) \
_CGI_DISPATCH(klass,path,_CGI_DISPATCH_LINE_NUMBER)

#define CGI_DISPATCH_DEFAULT(klass) \
static class _CGI_DISPATCH_CLASS_NAME(klass,DeFaUlT) { \
public: \
    _CGI_DISPATCH_CLASS_NAME(klass,DeFaUlT)() { \
        ::flinter::Dispatcher::GetInstance()->RegisterDefaultFactory(&_factory); \
    } \
private: \
    ::flinter::FactoryDirect< ::flinter::DefaultHandler, klass > _factory; \
} _CGI_DISPATCH_OBJECT_NAME(klass,DeFaUlT);

#endif // FLINTER_FASTCGI_DISPATCHER_H
