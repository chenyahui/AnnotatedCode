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

#include "flinter/fastcgi/dispatcher.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include <ClearSilver/ClearSilver.h>
#include <fcgiapp.h>

#include "flinter/fastcgi/cgi.h"
#include "flinter/fastcgi/custom_dispatcher.h"
#include "flinter/fastcgi/default_handlers.h"
#include "flinter/fastcgi/http_exception.h"
#include "flinter/thread/fixed_thread_pool.h"
#include "flinter/thread/tls.h"
#include "flinter/logger.h"
#include "flinter/msleep.h"
#include "flinter/runnable.h"

extern char **environ;

namespace flinter {

static const FactoryDirect<DefaultHandler, DefaultApacheHandler> g_default_factory;

static const std::pair<std::string, const Factory<CGI> *> kSystemFactoriesRaw[] = {
    // Maybe add some useful handlers here, like status page.
};

const std::map<std::string, const Factory<CGI> *> Dispatcher::kSystemFactories(
        kSystemFactoriesRaw, kSystemFactoriesRaw +
                sizeof (kSystemFactoriesRaw) / sizeof(*kSystemFactoriesRaw)
);

bool Dispatcher::_handle_signals_internally = true;
bool Dispatcher::_single_handler_mode = true;
bool Dispatcher::_run = true;

class Dispatcher::PassthroughDispatcher : public CustomDispatcher {
public:
    virtual ~PassthroughDispatcher() {}

    virtual std::string RequestUriToHandlerPath(const std::string &request_uri)
    {
        return request_uri.substr(0, request_uri.find('?'));
    }

}; // class Dispatcher::PassThroughDispatcher

class Dispatcher::Worker : public Runnable {
public:
    explicit Worker(Dispatcher *dispatcher) : _dispatcher(dispatcher) {}
    virtual ~Worker() {}

    virtual bool Run()
    {
        FCGX_Request request;
        if (FCGX_InitRequest(&request,
                             _dispatcher->_listen_fd,
                             FCGI_FAIL_ACCEPT_ON_INTR)) {

            LOG(ERROR) << "Dispatcher: failed to initialize FastCGI request.";
            return false;
        }

        int ret = 0;
        while (_run && (ret = FCGX_Accept_r(&request)) == 0) {
            _dispatcher->_sfc_tls->Set(&request);

            if (!_dispatcher->DispatchRequest()) {
                LOG(ERROR) << "Dispatcher: failed to dispatch FastCGI request.";
                return false;
            }

            FCGX_Finish_r(&request);
        }

        if (ret && ret != -EINTR) {
            LOG(ERROR) << "Dispatcher: failed to accept FastCGI request: " << ret;
        }

        return true;
    }

private:
    Dispatcher *_dispatcher;

}; // class Dispatcher::Worker

void Dispatcher::set_custom_dispatcher(CustomDispatcher *custom_dispatcher)
{
    if (!custom_dispatcher) {
        throw std::invalid_argument("invalid custom dispatcher specified");
    }

    Dispatcher *const dispatcher = GetInstance();
    delete dispatcher->_custom_dispatcher;
    dispatcher->_custom_dispatcher = custom_dispatcher;
}

void Dispatcher::set_site_root(const std::string &site_root)
{
    if (site_root.empty()                        ||
        site_root[0] != '/'                      ||
        site_root[site_root.length() - 1] != '/' ){

        throw std::invalid_argument("invalid site root specified");
    }

    GetInstance()->_site_root = site_root;
}

void Dispatcher::set_listen_fd(int fd)
{
    if (fd < 0) {
        fd = STDIN_FILENO;
    }

    GetInstance()->_listen_fd = fd;
}

void Dispatcher::set_handle_signals_internally(bool handle_internally)
{
    _handle_signals_internally = handle_internally;
}

void Dispatcher::set_single_handler_mode(bool single_handler_mode)
{
    _single_handler_mode = single_handler_mode;
}

void Dispatcher::set_forced_mode(const Mode &mode)
{
    GetInstance()->_mode = mode;
}

int Dispatcher::main(int argc, char *argv[])
{
    return GetInstance()->Run(argc, argv);
}

Dispatcher::Dispatcher() : _default_factory(&g_default_factory)
                         , _custom_dispatcher(new PassthroughDispatcher)
                         , _site_root("/")
                         , _listen_fd(STDIN_FILENO)
                         , _mode(kModeAutomatic)
                         , _sfc_pool(new FixedThreadPool)
                         , _sfc_tls(new TLS)
{
    // Intended left blank.
}

Dispatcher::~Dispatcher()
{
    delete _custom_dispatcher;
    delete _sfc_pool;
    delete _sfc_tls;
}

void Dispatcher::RegisterDefaultFactory(const Factory<DefaultHandler> *factory)
{
    if (factory) {
        _default_factory = factory;
    } else {
        _default_factory = &g_default_factory;
    }
}

void Dispatcher::RegisterFactory(const Factory<CGI> &factory, const std::string &path)
{
    _factories[path] = &factory;
}

int Dispatcher::RunAsExecutable(int argc, char *argv[])
{
    _mode = kModeExecutable;

    char *path = NULL;
    if (optind < argc) {
        path = argv[optind];
        if (*path != '/') {
            std::cerr << "Invalid REQUEST_URI." << std::endl;
            return EXIT_FAILURE;
        }
        setenv("REQUEST_URI", path, 1);

    } else {
        path = getenv("REQUEST_URI");
        if (!path) {
            setenv("REQUEST_URI", "/", 1);
        }
        path = getenv("REQUEST_URI");
    }

    char *qmark = strchr(path, '?');
    if (qmark) {
        *qmark = '\0';
        setenv("SCRIPT_NAME", path, 1);
        setenv("QUERY_STRING", qmark + 1, 1);
        *qmark = '?';
    }

    setenv("HTTP_HOST",         "localhost",    0);
    setenv("REMOTE_ADDR",       "127.0.0.1",    0);
    setenv("REMOTE_PORT",       "49152",        0);
    setenv("REQUEST_METHOD",    "GET",          0);
    setenv("REQUEST_SCHEME",    "http",         0);
    setenv("SERVER_ADDR",       "127.0.0.1",    0);
    setenv("SERVER_PORT",       "80",           0);

    setenv("GATEWAY_INTERFACE", "CGI/1.1",      1);
    setenv("SERVER_PROTOCOL",   "HTTP/1.1",     1);
    setenv("SERVER_SOFTWARE",   "Apache",       1);

    // Initialize ClearSilver with standard mode.
    cgiwrap_init_std(argc, argv, environ);

    if (!DispatchRequest()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int Dispatcher::RunAsCGI(int argc, char *argv[])
{
    _mode = kModeCGI;

    // Initialize ClearSilver with standard mode.
    cgiwrap_init_std(argc, argv, environ);

    if (!DispatchRequest()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void Dispatcher::InitializeClearSilverForFastCGI()
{
    // ClearSilver has thread model issue so initialize it before going multi-threaded.
    ::CGI *cgi;
    NEOERR *err = cgi_init(&cgi, NULL);
    if (err == STATUS_OK) {
        cgi_destroy(&cgi);
    } else {
        nerr_ignore(&err);
    }
}

void Dispatcher::on_sigterm(int /*signum*/)
{
    _run = false;

    // It's signal handler safe.
    FCGX_ShutdownPending();

    if (!_handle_signals_internally) {
        return;
    }

    // Reset signal handlers so another signal will kill for sure.
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;

    sigaction(SIGHUP , &act, NULL);
    sigaction(SIGINT , &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
}

void Dispatcher::InitializeFastCGI()
{
    _mode = kModeFastCGI;

    /*
     * Note for signal handling in FastCGI mode.
     *
     * There is no way for us to use atomic signal handling technique since
     * FastCGI wraps the accepting routine without atomic capabilities, so
     * that if a signal sneaks in between the _run check and the accept(2),
     * you know what I mean.
     *
     * It's not so reliable so kill the process again if it's stale.
     */

    // Initialize signal handlers.
    if (_handle_signals_internally) {
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = on_sigterm;
        sigfillset(&act.sa_mask);

        sigaction(SIGHUP , &act, NULL);
        sigaction(SIGINT , &act, NULL);
        sigaction(SIGQUIT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
        sigaction(SIGUSR1, &act, NULL);

        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, NULL);
    }

    InitializeClearSilverForFastCGI();

    // Initialize ClearSilver with emulated mode.
    cgiwrap_init_emu(_sfc_tls,
                     EmulatedRead,
                     EmulatedWriteF,
                     EmulatedWrite,
                     EmulatedGetenv,
                     EmulatedPutenv,
                     EmulatedIterenv);
}

int Dispatcher::RunAsFastCGI(int /*argc*/, char * /*argv*/ [])
{
    InitializeFastCGI();

    Worker worker(this);
    bool ret = worker.Run();
    if (!ret) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int Dispatcher::Run(int argc, char *argv[])
{
    int ret;
    if (_mode == kModeAutomatic) {
        // Try to guess from all that we can get.
        if (FCGX_IsCGI()) {
            const char *gateway = getenv("GATEWAY_INTERFACE");

            if (!gateway || strcmp(gateway, "CGI/1.1")) {
                ret = RunAsExecutable(argc, argv);
            } else {
                ret = RunAsCGI(argc, argv);
            }

        } else {
            ret = RunAsFastCGI(argc, argv);
        }

    } else if (_mode == kModeFastCGI) {
        ret = RunAsFastCGI(argc, argv);
    } else if (_mode == kModeCGI) {
        ret = RunAsCGI(argc, argv);
    } else {
        ret = RunAsExecutable(argc, argv);
    }

    return ret;
}

CGI *Dispatcher::GetHandler()
{
    if (_single_handler_mode && _factories.size() == 1) {
        return _factories.begin()->second->Create();
    }

    char *path;
    if (cgiwrap_getenv("REQUEST_URI", &path) != STATUS_OK) {
        path = NULL;
    }

    if (!path) {
        std::cerr << "Failed to retrieve REQUEST_URI." << std::endl;
        return NULL;
    }

    if (*path != '/') {
        std::cerr << "Invalid REQUEST_URI: " << path << std::endl;
        free(path);
        return NULL;
    }

    char *const qmark = strchr(path, '?');
    if (qmark) {
        *qmark = '\0';
    }

    if (strncmp(path, _site_root.data(), _site_root.length())) {
        free(path);
        return GetDefaultHandler();
    }

    const char *const rpath = path + _site_root.length() - 1;
    const std::map<std::string, const Factory<CGI> *>::const_iterator
            p = kSystemFactories.find(rpath);

    if (p != kSystemFactories.end()) {
        return p->second->Create();
    }

    if (qmark) {
        *qmark = '?';
    }

    std::string uri = _custom_dispatcher->RequestUriToHandlerPath(rpath);
    if (uri.empty()) {
        return GetDefaultHandler();
    }

    const std::map<std::string, const Factory<CGI> *>::const_iterator
            q = _factories.find(uri);

    if (q != _factories.end()) {
        return q->second->Create();
    }

    return GetDefaultHandler();
}

CGI *Dispatcher::GetDefaultHandler()
{
    // It's not known to me!
    DefaultHandler *handler = _default_factory->Create();
    handler->set_status_code(404);
    return handler;
}

bool Dispatcher::DispatchRequest()
{
    CGI *handler = NULL;
    bool result = false;

    try {
        handler = GetHandler();
        if (!handler) {
            return false;
        }

        handler->ProcessRequest();
        result = true;

    } catch (const HttpException &e) {
        ProcessException(handler, e);
        result = true;

    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
    }

    delete handler;
    return result;
}

void Dispatcher::ProcessException(const CGI *cgi, const HttpException &e)
{
    // Nothing has been sent.
    if (!cgi || !cgi->_header_sent) {
        DefaultHandler *handler = _default_factory->Create();
        handler->set_status_code(e.status_code());
        handler->ProcessRequest();
        delete handler;
    }

    // It's too late, forget about it.
}

int Dispatcher::EmulatedRead(void *context, char *buf, int buf_len)
{
    TLS *tls = reinterpret_cast<TLS *>(context);
    FCGX_Request *request = reinterpret_cast<FCGX_Request *>(tls->Get());
    return FCGX_GetStr(buf, buf_len, request->in);
}

int Dispatcher::EmulatedWriteF(void *context, const char *fmt, va_list va)
{
    TLS *tls = reinterpret_cast<TLS *>(context);
    FCGX_Request *request = reinterpret_cast<FCGX_Request *>(tls->Get());
    return FCGX_VFPrintF(request->out, fmt, va);
}

int Dispatcher::EmulatedWrite(void *context, const char *buf, int buf_len)
{
    TLS *tls = reinterpret_cast<TLS *>(context);
    FCGX_Request *request = reinterpret_cast<FCGX_Request *>(tls->Get());
    return FCGX_PutStr(buf, buf_len, request->out);
}

char *Dispatcher::EmulatedGetenv(void *context, const char *key)
{
    TLS *tls = reinterpret_cast<TLS *>(context);
    FCGX_Request *request = reinterpret_cast<FCGX_Request *>(tls->Get());

    char *value = FCGX_GetParam(key, request->envp);
    if (!value) {
        return NULL;
    }

    return strdup(value);
}

int Dispatcher::EmulatedPutenv(void *context, const char *key, const char *value)
{
    TLS *tls = reinterpret_cast<TLS *>(context);
    FCGX_Request *request = reinterpret_cast<FCGX_Request *>(tls->Get());

    size_t kl = strlen(key);
    size_t vl = strlen(value);
    char *n = reinterpret_cast<char *>(malloc(kl + vl + 2));
    if (!n) {
        return -1;
    }

    memcpy(n, key, kl);
    n[kl] = '=';
    memcpy(n + kl + 1, value, vl);
    n[kl + 1 + vl] = '\0';

    size_t c = 0;
    for (char **p = request->envp; *p; ++p) {
        if (strncmp(*p, n, kl + 1) == 0) {
            free(*p);
            *p = n;
            return 0;
        }

        ++c;
    }

    // New one, need to relocate envp.
    char **envp = reinterpret_cast<char **>(malloc(sizeof(char *) * (c + 2)));
    if (!envp) {
        free(n);
        return -1;
    }

    char **p = request->envp;
    char **q = envp;
    while (*p) {
        *q++ = *p++;
    }

    *q = n;
    free(request->envp);
    request->envp = envp;
    return 0;
}

int Dispatcher::EmulatedIterenv(void *context, int i, char **key, char **value)
{
    TLS *tls = reinterpret_cast<TLS *>(context);
    FCGX_Request *request = reinterpret_cast<FCGX_Request *>(tls->Get());

    *key = NULL;
    *value = NULL;
    char *s = request->envp[i];
    if (!s) {
        return 0;
    }

    char *k = strchr(s, '=');
    if (!k) {
        return 0;
    }

    *k = '\0';
    *key = strdup(s);
    *k = '=';
    if (!*key) {
        return -1;
    }

    *value = strdup(k + 1);
    if (!*value) {
        free(*key);
        *key = NULL;
        return -1;
    }

    return 0;
}

bool Dispatcher::SpawnFastCGI(size_t threads)
{
    assert(threads);

    // Initialize FastCGI.
    if (FCGX_Init()) {
        std::cerr << "Failed to initialize FastCGI." << std::endl;
        return false;
    }

    Dispatcher *instance = Dispatcher::GetInstance();
    instance->InitializeFastCGI();
    FixedThreadPool *pool = instance->_sfc_pool;

    if (!pool->Initialize(threads)) {
        pool->Shutdown();
        return false;
    }

    for (size_t i = 0; i < threads; ++i) {
        Worker *worker = new Worker(instance);
        if (!pool->AppendJob(worker, true)) {
            pool->Shutdown();
            return false;
        }
    }

    return true;
}

void Dispatcher::Shutdown()
{
    Dispatcher *instance = Dispatcher::GetInstance();
    instance->_sfc_pool->KillAll(SIGUSR1);

    // In case there's a race, kill twice.
    msleep(100);
    instance->_sfc_pool->KillAll(SIGUSR1);

    instance->_sfc_pool->Shutdown();
}

} // namespace flinter
