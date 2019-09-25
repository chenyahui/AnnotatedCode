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

#include "flinter/linkage/easy_server.h"

#include <sys/socket.h>
#include <assert.h>
#include <string.h>

#include <stdexcept>

#include "flinter/linkage/easy_context.h"
#include "flinter/linkage/easy_handler.h"
#include "flinter/linkage/easy_tuner.h"
#include "flinter/linkage/file_descriptor_io.h"
#include "flinter/linkage/interface.h"
#include "flinter/linkage/linkage.h"
#include "flinter/linkage/linkage_handler.h"
#include "flinter/linkage/linkage_peer.h"
#include "flinter/linkage/linkage_worker.h"
#include "flinter/linkage/listener.h"
#include "flinter/linkage/ssl_io.h"

#include "flinter/thread/condition.h"
#include "flinter/thread/fixed_thread_pool.h"
#include "flinter/thread/mutex.h"
#include "flinter/thread/mutex_locker.h"
#include "flinter/thread/scheduler.h"

#include "flinter/types/shared_ptr.h"

#include "flinter/explode.h"
#include "flinter/runnable.h"
#include "flinter/logger.h"
#include "flinter/utility.h"

#include "config.h"
#if HAVE_OPENSSL_OPENSSLV_H
#include <openssl/err.h>
#endif

namespace flinter {

const EasyServer::Configure EasyServer::kDefaultConfigure = {
    /* incoming_receive_timeout     = */ 5000000000LL,
    /* incoming_connect_timeout     = */ 5000000000LL,
    /* incoming_send_timeout        = */ 5000000000LL,
    /* incoming_idle_timeout        = */ 60000000000LL,
    /* outgoing_receive_timeout     = */ 5000000000LL,
    /* outgoing_connect_timeout     = */ 5000000000LL,
    /* outgoing_send_timeout        = */ 5000000000LL,
    /* outgoing_idle_timeout        = */ 60000000000LL,
    /* maximum_active_connections   = */ 50000,
};

EasyServer::ListenOption::ListenOption()
        : easy_factory(NULL)
        , easy_handler(NULL)
        , ssl(NULL)
{
    listen_socket.domain = AF_INET6;
    listen_socket.type = SOCK_STREAM;

    listen_option.socket_close_on_exec = true;
    listen_option.socket_reuse_address = true;
    listen_option.socket_non_blocking = true;

    accepted_option.socket_close_on_exec = true;
    accepted_option.socket_non_blocking = true;
}

EasyServer::ConnectOption::ConnectOption()
        : easy_factory(NULL)
        , easy_handler(NULL)
        , ssl(NULL)
        , thread_id(-1)
{
    connect_socket.domain = AF_INET6;
    connect_socket.type = SOCK_STREAM;

    connect_option.socket_close_on_exec = true;
    connect_option.socket_non_blocking = true;
}

void EasyServer::ListenOption::ToString(std::string *str) const
{
    assert(str);
    std::ostringstream s;
    s << *this;
    str->assign(s.str());
}

void EasyServer::ConnectOption::ToString(std::string *str) const
{
    assert(str);
    std::ostringstream s;
    s << *this;
    str->assign(s.str());
}

const EasyServer::channel_t EasyServer::kInvalidChannel = 0;
const size_t EasyServer::kMaximumWorkers = 16384;
const size_t EasyServer::kMaximumSlots = 128;

class EasyServer::OutgoingInformation {
public:
    OutgoingInformation(ProxyHandler *proxy_handler,
                        const Interface::Socket &socket,
                        const Interface::Option &option,
                        int thread_id) : _proxy_handler(proxy_handler)
                                       , _socket(socket)
                                       , _option(option)
                                       , _thread_id(thread_id)
    {
        if (_socket.socket_interface) {
            _socket_interface = _socket.socket_interface;
            _socket.socket_interface = _socket_interface.c_str();
        }

        if (_socket.socket_hostname) {
            _socket_hostname = _socket.socket_hostname;
            _socket.socket_hostname = _socket_hostname.c_str();
        }

        if (_socket.unix_abstract) {
            _unix_abstract = _socket.unix_abstract;
            _socket.unix_abstract = _unix_abstract.c_str();
        }

        if (_socket.unix_pathname) {
            _unix_pathname = _socket.unix_pathname;
            _socket.unix_pathname = _unix_pathname.c_str();
        }
    }

    ProxyHandler *proxy_handler()     const { return _proxy_handler; }
    const Interface::Socket &socket() const { return _socket;        }
    const Interface::Option &option() const { return _option;        }
    int thread_id()                   const { return _thread_id;     }

private:
    ProxyHandler *const _proxy_handler;
    Interface::Socket _socket;
    Interface::Option _option;
    const int _thread_id;

    // Deep copy of _socket, ugly but put it like this now
    std::string _socket_interface;
    std::string _socket_hostname;
    std::string _unix_abstract;
    std::string _unix_pathname;

}; // class OutgoingInformation

// linkageWorker的一个proxy类，用于EasyServer测封装一些行为
class EasyServer::ProxyLinkageWorker : public LinkageWorker {
public:
    virtual ~ProxyLinkageWorker() {}
    ProxyLinkageWorker(EasyTuner *tuner,
                       int thread_id,
                       const std::vector<int> &affinities)
            : _affinities(affinities), _tuner(tuner), _thread_id(thread_id) {}

    int thread_id() const
    {
        return _thread_id;
    }

protected:
    virtual bool OnInitialize();
    virtual void OnShutdown();

private:
    const std::vector<int> _affinities;
    EasyTuner *const _tuner;
    const int _thread_id;

}; // class EasyServer::ProxyLinkageWorker

class EasyServer::ProxyListener : public Listener {
public:
    explicit ProxyListener(EasyServer *easy_server, ProxyHandler *proxy_handler)
            : _easy_server(easy_server), _proxy_handler(proxy_handler) {}
    virtual ~ProxyListener() {}

    virtual LinkageBase *CreateLinkage(LinkageWorker *worker,
                                       const LinkagePeer &peer,
                                       const LinkagePeer &me);

private:
    EasyServer *const _easy_server;
    ProxyHandler *const _proxy_handler;

}; // class EasyServer::ProxyListener

// ProxyHandler是对EasyHandler的一个代理式封装
class EasyServer::ProxyHandler : public LinkageHandler {
public:
    virtual ~ProxyHandler() {}
    ProxyHandler(const Interface::Option &accepted_option,
                 EasyHandler *easy_handler,
                 Factory<EasyHandler> *easy_factory,
                 SslContext *ssl)
            : _ssl(ssl)
            , _easy_handler(easy_handler)
            , _easy_factory(easy_factory)
            , _accepted_option(accepted_option) {}

    virtual ssize_t GetMessageLength(Linkage *linkage,
                                     const void *buffer,
                                     size_t length);

    virtual int OnMessage(Linkage *linkage,
                          const void *buffer,
                          size_t length);

    virtual void OnDisconnected(Linkage *linkage);
    virtual bool OnConnected(Linkage *linkage);
    virtual void OnError(Linkage *linkage,
                         bool reading_or_writing,
                         int errnum);

    virtual bool Cleanup(Linkage *linkage, int64_t now);

    SslContext *ssl() const
    {
        return _ssl;
    }

    const Interface::Option &accepted_option() const
    {
        return _accepted_option;
    }

    EasyHandler *easy_handler() const
    {
        return _easy_handler;
    }

    Factory<EasyHandler> *easy_factory() const
    {
        return _easy_factory;
    }

private:
    SslContext *const _ssl;
    EasyHandler *const _easy_handler;
    Factory<EasyHandler> *const _easy_factory;
    Interface::Option _accepted_option;

}; // class EasyServer::ProxyHandler

class EasyServer::ProxyLinkage : public Linkage {
public:
    ProxyLinkage(EasyContext *context, AbstractIo *io, ProxyHandler *handler,
                 const LinkagePeer &peer, const LinkagePeer &me)
            : Linkage(io, handler, peer, me), _context(context)
    {
        // Intended left blank.
    }

    virtual ~ProxyLinkage() {}
    shared_ptr<EasyContext> &context()
    {
        return _context;
    }

private:
    shared_ptr<EasyContext> _context;

}; // class EasyServer::ProxyLinkage

class EasyServer::JobWorker : public Runnable {
public:
    JobWorker(EasyServer *easy_server,
              EasyTuner *easy_tuner,
              size_t id,
              const std::vector<int> &affinities)
            : _id(id)
            , _easy_tuner(easy_tuner)
            , _easy_server(easy_server)
            , _affinities(affinities) {}

    virtual ~JobWorker() {}
    virtual bool Run();
    class Job;

private:
    const size_t _id;
    EasyTuner *const _easy_tuner;
    EasyServer *const _easy_server;
    const std::vector<int> _affinities;

}; // class EasyServer::JobWorker

class EasyServer::JobWorker::Job : public Runnable {
public:
    /// @param context COWed.
    /// @param buffer copied.
    Job(const shared_ptr<EasyContext> &context,
        const void *buffer, size_t length);

    virtual ~Job() {}
    virtual bool Run();

private:
    const shared_ptr<EasyContext> _context;
    const std::string _message;

}; // class EasyServer::JobWorker::Job

class EasyServer::RegisterTimerJob : public Runnable {
public:
    RegisterTimerJob(LinkageWorker *worker,
                     int64_t after,
                     int64_t repeat,
                     Runnable *timer) : _worker(worker)
                                      , _timer(timer)
                                      , _repeat(repeat)
                                      , _after(after) {}

    virtual ~RegisterTimerJob() {}
    virtual bool Run()
    {
        // If I get called, I'm in the worker thread, so _worker must be valid.
        if (!_worker->RegisterTimer(_after, _repeat, _timer, true)) {
            LOG(ERROR) << "EasyServer: failed to register timer.";
        }

        return true;
    }

private:
    LinkageWorker *const _worker;
    Runnable *const _timer;
    const int64_t _repeat;
    const int64_t _after;

}; // class EasyServer::RegisterTimerJob

class EasyServer::DisconnectJob : public Runnable {
public:
    DisconnectJob(EasyServer *easy_server,
                  channel_t channel,
                  bool finish_write) : _easy_server(easy_server)
                                     , _channel(channel)
                                     , _finish_write(finish_write) {}

    virtual ~DisconnectJob() {}
    virtual bool Run()
    {
        _easy_server->DoRealDisconnect(_channel, _finish_write);
        return true;
    }

private:
    EasyServer *const _easy_server;
    const channel_t _channel;
    const bool _finish_write;

}; // class EasyServer::DisconnectJob

class EasyServer::SendJob : public Runnable {
public:
    SendJob(EasyServer *easy_server,
            ProxyLinkageWorker *worker,
            channel_t channel,
            const void *buffer,
            size_t length) : _worker(worker)
                           , _easy_server(easy_server)
                           , _channel(channel)
                           , _length(0)
                           , _buffer(NULL)
    {
        if (buffer && length) {
            _buffer = malloc(length);
            if (!_buffer) {
                throw std::bad_alloc();
            }

            memcpy(_buffer, buffer, length);
            _length = length;
        }
    }

    virtual ~SendJob()
    {
        free(_buffer);
    }

    virtual bool Run()
    {
        _easy_server->DoRealSend(_worker, _channel, _buffer, _length);
        return true;
    }

    operator bool() const
    {
        return !!_buffer;
    }

private:
    ProxyLinkageWorker *const _worker;
    EasyServer *const _easy_server;
    const channel_t _channel;
    size_t _length;
    void *_buffer;

}; // class EasyServer::SendJob

// 每个io线程的上的信息
class EasyServer::IoContext {
public:
    explicit IoContext(int thread_id);
    ~IoContext();

    outgoing_map_t _outgoing_informations;
    connect_map_t _connect_proxy_handlers;
    channel_map_t _channel_linkages;    // 连接信息
    const int _thread_id;   // 该io线程的thread id, 目前这个thread id是手动分配的，不是直接获取的线程id的
    Mutex *const _mutex;    // 该io线程的mutex，用来给这个线程上的信息加锁
    channel_t _channel; // 目前已分配的channel的最大值

}; // class EasyServer::IoContext

bool EasyServer::ProxyLinkageWorker::OnInitialize()
{
    if (!Scheduler::SetAffinity(_affinities)) {
        return false;
    }

    if (!_tuner) {
        return true;
    }

    return _tuner->OnIoThreadInitialize();
}

void EasyServer::ProxyLinkageWorker::OnShutdown()
{
    if (_tuner) {
        _tuner->OnIoThreadShutdown();
    }

#if HAVE_OPENSSL_OPENSSLV_H
    // Every thread should call this before terminating.
    ERR_remove_thread_state(NULL);
#endif
}

EasyServer::JobWorker::Job::Job(const shared_ptr<EasyContext> &context,
                                const void *buffer,
                                size_t length)
        : _context(context)
        , _message(reinterpret_cast<const char *>(buffer),
                   reinterpret_cast<const char *>(buffer) + length)
{
    // Intended left blank.
    // todo 很明显，这里把数据做了一层拷贝
}

ssize_t EasyServer::ProxyHandler::GetMessageLength(Linkage *linkage,
                                                   const void *buffer,
                                                   size_t length)
{
    ProxyLinkage *l = static_cast<ProxyLinkage *>(linkage);
    EasyHandler *h = l->context()->easy_handler();
    return h->GetMessageLength(*l->context(), buffer, length);
}

// 当一个连接上，有新的消息到来时，在这个函数中处理
// 该函数在io线程中执行
int EasyServer::ProxyHandler::OnMessage(Linkage *linkage,
                                        const void *buffer,
                                        size_t length)
{
    ProxyLinkage *l = static_cast<ProxyLinkage *>(linkage);
    EasyServer *s = l->context()->easy_server();

    // Don't call QueueOrExecute() since I have to copy the buffer.

    // _workers are only set in single threaded environment,
    // skip locking to get better performance.

    // 如果存在jobworkers，则直接把任务抛给jobworkers处理
    if (s->_workers) {
        JobWorker::Job *job = new JobWorker::Job(l->context(), buffer, length);

        int hash = l->context()->easy_handler()->HashMessage(
                *l->context(), buffer, length);

        MutexLocker glocker(s->_gmutex);
        s->DoAppendJob(job, hash);
        return 1;
    }

    // 如果不存在，则直接在io线程处理
    // Same thread, handle events to LinkageWorker.
    EasyContext &c = *l->context();
    return c.easy_handler()->OnMessage(c, buffer, length);
}

void EasyServer::ProxyHandler::OnDisconnected(Linkage *linkage)
{
    ProxyLinkage *l = static_cast<ProxyLinkage *>(linkage);
    EasyHandler *h = l->context()->easy_handler();
    EasyServer *s = l->context()->easy_server();

    h->OnDisconnected(*l->context());
    s->ReleaseChannel(l->context()->channel());
}

bool EasyServer::ProxyHandler::OnConnected(Linkage *linkage)
{
    ProxyLinkage *l = static_cast<ProxyLinkage *>(linkage);
    EasyHandler *h = l->context()->easy_handler();

    if (_ssl) {
        SslIo *io = static_cast<SslIo *>(l->io());
        l->context()->set_ssl_peer(io->peer());
    }

    return h->OnConnected(*l->context());
}

void EasyServer::ProxyHandler::OnError(Linkage *linkage,
                                       bool reading_or_writing,
                                       int errnum)
{
    ProxyLinkage *l = static_cast<ProxyLinkage *>(linkage);
    EasyHandler *h = l->context()->easy_handler();
    return h->OnError(*l->context(), reading_or_writing, errnum);
}

bool EasyServer::ProxyHandler::Cleanup(Linkage *linkage, int64_t now)
{
    ProxyLinkage *l = static_cast<ProxyLinkage *>(linkage);
    EasyHandler *h = l->context()->easy_handler();
    return h->Cleanup(*l->context(), now);
}

// 当有新的连接到来的时候，在这个函数中创建新连接
LinkageBase *EasyServer::ProxyListener::CreateLinkage(LinkageWorker *worker,
                                                      const LinkagePeer &peer,
                                                      const LinkagePeer &me)
{
    return _easy_server->AllocateChannel(worker, peer, me, _proxy_handler);
}

bool EasyServer::JobWorker::Run()
{
    if (!Scheduler::SetAffinity(_affinities)) {
        LOG(ERROR) << "EasyServer: failed to set thread affinities.";
        throw std::runtime_error("EasyServer: failed to set thread affinities.");
    }

    if (_easy_tuner) {
        if (!_easy_tuner->OnJobThreadInitialize()) {
            LOG(ERROR) << "EasyServer: failed to initialize job worker.";
            throw std::runtime_error("EasyServer: failed to initialize job worker.");
        }
    }

    CLOG.Verbose("EasyServer: worker %lu started.", _id);
    while (true) {
        Runnable *job = _easy_server->GetJob(_id);
        if (!job) {
            break;
        }

        job->Run();
        delete job;
    }
    CLOG.Verbose("EasyServer: worker %lu quit.", _id);

    if (_easy_tuner) {
        _easy_tuner->OnJobThreadShutdown();
    }

#if HAVE_OPENSSL_OPENSSLV_H
    // Every thread should call this before terminating.
    ERR_remove_thread_state(NULL);
#endif

    return true;
}

bool EasyServer::JobWorker::Job::Run()
{
    EasyServer *s = _context->easy_server();
    EasyHandler *h = _context->easy_handler();

    CLOG.Verbose("JobWorker: executing [%p]", this);
    int ret = h->OnMessage(*_context, _message.data(), _message.length());
    CLOG.Verbose("JobWorker: job [%p] returned %d", this, ret);

    // Simulate LinkageWorker since we're running on our own.
    if (ret < 0) {
        h->OnError(*_context, true, errno);
        s->Disconnect(_context->channel(), false);

    } else if (ret == 0) {
        s->Disconnect(_context->channel(), true);
    }

    return true;
}

EasyServer::IoContext::IoContext(int thread_id)
        : _thread_id(thread_id)
        , _mutex(new Mutex)
        , _channel(static_cast<channel_t>(thread_id))
{
    // Intended left blank.
}

EasyServer::IoContext::~IoContext()
{
    delete _mutex;

    for (connect_map_t::iterator p = _connect_proxy_handlers.begin();
         p != _connect_proxy_handlers.end(); ++p) {

        delete p->second;
    }

    for (outgoing_map_t::iterator p = _outgoing_informations.begin();
         p != _outgoing_informations.end(); ++p) {

        delete p->second;
    }
}

EasyServer::EasyServer()
        : _pool(new FixedThreadPool)
        , _incoming(new Condition)
        , _configure(kDefaultConfigure)
        , _gmutex(new Mutex)
        , _workers(0)
        , _slots(0)
        , _incoming_connections(0)
        , _outgoing_connections(0)
{
    // Intended left blank.
}

EasyServer::~EasyServer()
{
    Shutdown();

    delete _pool;
    delete _gmutex;
    delete _incoming;
}

bool EasyServer::Listen(const ListenOption &o)
{
    if (!!o.easy_handler == !!o.easy_factory) {
        LOG(FATAL) << "EasyServer: BUG: either handler or factory must be configured.";
        return false;
    }

    if (o.listen_socket.type != SOCK_STREAM) {
        LOG(FATAL) << "EasyServer: BUG: support SOCK_STREAM only.";
        return false;
    }

    ProxyHandler *proxy_handler = new ProxyHandler(
            o.accepted_option,
            o.easy_handler,
            o.easy_factory,
            o.ssl);

    Listener *listener = new ProxyListener(this, proxy_handler);

    if (!listener->Listen(o.listen_socket, o.listen_option)) {
        delete listener;
        delete proxy_handler;
        return false;
    }

    MutexLocker glocker(_gmutex);
    if (!_io_workers.empty()) {
        delete listener;
        delete proxy_handler;
        throw std::logic_error("EasyServer only listens before Initialize()");
    }

    _listen_proxy_handlers.push_back(proxy_handler);
    _listeners.push_back(listener);
    return true;
}

bool EasyServer::Listen(uint16_t port, EasyHandler *easy_handler)
{
    if (!easy_handler) {
        LOG(FATAL) << "EasyServer: BUG: no handler configured.";
        return false;
    }

    ListenOption o;
    o.listen_socket.domain = AF_INET;
    o.listen_socket.socket_bind_port = port;
    o.easy_handler = easy_handler;
    return Listen(o);
}

bool EasyServer::SslListen(uint16_t port,
                           SslContext *ssl,
                           EasyHandler *easy_handler)
{
    if (!ssl) {
        LOG(FATAL) << "EasyServer: BUG: no SSL context configured.";
        return false;
    }

    if (!easy_handler) {
        LOG(FATAL) << "EasyServer: BUG: no handler configured.";
        return false;
    }

    ListenOption o;
    o.listen_socket.domain = AF_INET;
    o.listen_socket.socket_bind_port = port;
    o.easy_handler = easy_handler;
    o.ssl = ssl;
    return Listen(o);
}

bool EasyServer::Listen(uint16_t port, Factory<EasyHandler> *easy_factory)
{
    if (!easy_factory) {
        LOG(FATAL) << "EasyServer: BUG: no handler factory configured.";
        return false;
    }

    ListenOption o;
    o.listen_socket.domain = AF_INET;
    o.listen_socket.socket_bind_port = port;
    o.easy_factory = easy_factory;
    return Listen(o);
}

bool EasyServer::SslListen(uint16_t port,
                           SslContext *ssl,
                           Factory<EasyHandler> *easy_factory)
{
    if (!ssl) {
        LOG(FATAL) << "EasyServer: BUG: no SSL context configured.";
        return false;
    }

    if (!easy_factory) {
        LOG(FATAL) << "EasyServer: BUG: no handler factory configured.";
        return false;
    }

    ListenOption o;
    o.listen_socket.domain = AF_INET;
    o.listen_socket.socket_bind_port = port;
    o.easy_factory = easy_factory;
    o.ssl = ssl;
    return Listen(o);
}

bool EasyServer::RegisterTimer(int64_t interval, Runnable *timer)
{
    return RegisterTimer(interval, interval, timer);
}

bool EasyServer::RegisterTimer(int64_t after, int64_t repeat, Runnable *timer)
{
    if (after < 0 || repeat <= 0 || !timer) {
        return false;
    }

    int64_t tid = get_current_thread_id();
    MutexLocker glocker(_gmutex);
    if (_io_workers.empty()) {
        _timers.push_back(std::make_pair(timer, std::make_pair(after, repeat)));
        return true;
    }

    // 这里可以看到 timer是建立在io线程的
    LinkageWorker *worker = GetIoWorker(-1);

    if (tid > 0 && tid == worker->running_thread_id()) {
        glocker.Unlock();
        if (!worker->RegisterTimer(after, repeat, timer, true)) {
            LOG(ERROR) << "EasyServer: failed to register timer.";
            return false;
        }

        return true;
    }

    Runnable *job = new RegisterTimerJob(worker, after, repeat, timer);
    return worker->SendCommand(job);
}

/**
 */ 
bool EasyServer::Initialize(const std::string &slots,
                            const std::string *workers,
                            EasyTuner *easy_tuner)
{
    std::vector<std::vector<int> > as;
    std::vector<std::vector<int> > aw;
    if (explode_lists(slots, &as, 256)                 ||
        (workers && explode_lists(*workers, &aw, 256)) ||
        as.empty()                                     ||
        as.size() > kMaximumSlots                      ||
        aw.size() > kMaximumWorkers                    ){

        LOG(ERROR) << "EasyServer: invalid initializing parameters.";
        return false;
    }

    // 初始化线程池, 线程池的个数等于 ioworker的数目 + jobworker的数目
    size_t total = as.size() + aw.size();
    if (!_pool->Initialize(total)) {
        LOG(ERROR) << "EasyServer: failed to initialize thread pool.";
        return false;
    }

    // Set constants before going multi-threaded.
    // DoShutdown() will reset them if anything bad happens.
    _job_workers.reserve(aw.size());
    _io_workers.reserve(as.size());
    _workers = aw.size();
    _slots = as.size();

    MutexLocker glocker(_gmutex);

    // 创建iowoker
    for (size_t i = 0; i < as.size(); ++i) {
        ProxyLinkageWorker *worker =
                new ProxyLinkageWorker(easy_tuner, static_cast<int>(i), as[i]);

        _io_workers.push_back(worker);

        // 将ioworker和listener绑定起来
        if (!AttachListeners(worker)) {
            LOG(ERROR) << "EasyServer: failed to initialize I/O workers.";
            DoShutdown(&glocker);
            return false;
        }
    }

    // 注册定时器，这样就是不是意味着运行后，没办法再注册timer了
    for (std::list<std::pair<Runnable *, std::pair<int64_t, int64_t> > >::iterator
         p = _timers.begin(); p != _timers.end();) {

        // -1代表随机取一个ioworker
        LinkageWorker *worker = GetIoWorker(-1);
        
        if (!worker->RegisterTimer(p->second.first, p->second.second, p->first, true)) {
            LOG(ERROR) << "EasyServer: failed to initialize timers.";
            DoShutdown(&glocker);
            return false;
        }

        p = _timers.erase(p);
    }

    // 初始化IoContext
    _io_context.reserve(as.size());
    for (size_t i = 0; i < as.size(); ++i) {
        _io_context.push_back(new IoContext(static_cast<int>(i)));
    }

    // 初始化jobworker
    for (size_t i = 0; i < aw.size(); ++i) {
        JobWorker *worker = new JobWorker(this, easy_tuner, i, aw[i]);
        _job_workers.push_back(worker);
    }

    _hashjobs.resize(aw.size());

    // Now multi-threading.

    for (std::vector<ProxyLinkageWorker *>::const_iterator
         p = _io_workers.begin(); p != _io_workers.end(); ++p) {

        if (!_pool->AppendJob(*p, true)) {
            LOG(ERROR) << "EasyServer: failed to append I/O workers.";
            DoShutdown(&glocker);
            return false;
        }
    }

    for (std::vector<JobWorker *>::const_iterator
         p = _job_workers.begin(); p != _job_workers.end(); ++p) {

        if (!_pool->AppendJob(*p, true)) {
            LOG(ERROR) << "EasyServer: failed to append job workers.";
            DoShutdown(&glocker);
            return false;
        }
    }

    return true;
}

bool EasyServer::Initialize(size_t slots,
                            size_t workers,
                            EasyTuner *easy_tuner)
{
    if (slots  == 0               ||
        slots   > kMaximumSlots   ||
        workers > kMaximumWorkers ){

        LOG(ERROR) << "EasyServer: invalid initializing parameters.";
        return false;
    }

    const char *delim;

    delim = "";
    std::string as;
    for (size_t i = 0; i < slots; ++i) {
        as.append(delim);
        delim = ";";
    }

    delim = "";
    std::string aw;
    for (size_t i = 0; i < workers; ++i) {
        aw.append(delim);
        delim = ";";
    }

    return Initialize(as, workers ? &aw : NULL, easy_tuner);
}

bool EasyServer::Shutdown()
{
    MutexLocker glocker(_gmutex);
    return DoShutdown(&glocker);
}

void EasyServer::DoDumpJobs()
{
    while (!_jobs.empty()) {
        delete _jobs.front();
        _jobs.pop();
    }

    for (std::vector<std::queue<Runnable *> >::iterator p = _hashjobs.begin();
         p != _hashjobs.end(); ++p) {

        while (!p->empty()) {
            delete p->front();
            p->pop();
        }
    }
}

bool EasyServer::DoShutdown(MutexLocker *locker)
{
    DoDumpJobs();
    for (size_t i = 0; i < _workers; ++i) {
        DoAppendJob(NULL, static_cast<int>(i));
    }

    for (std::vector<ProxyLinkageWorker *>::const_iterator
         p = _io_workers.begin(); p != _io_workers.end(); ++p) {

        (*p)->Shutdown();
    }

    // Job workers and I/O workers are released as well.
    locker->Unlock();
    _pool->Shutdown();

    // Now back into single threaded mode, don't lock again.

    for (std::vector<IoContext *>::iterator p = _io_context.begin();
         p != _io_context.end(); ++p) {

        delete *p;
    }

    for (std::list<ProxyHandler *>::iterator p = _listen_proxy_handlers.begin();
         p != _listen_proxy_handlers.end(); ++p) {

        delete *p;
    }

    for (std::list<Listener *>::iterator p = _listeners.begin();
         p != _listeners.end(); ++p) {

        delete *p;
    }

    for (std::list<std::pair<Runnable *, std::pair<int64_t, int64_t> > >::iterator
         p = _timers.begin(); p != _timers.end(); ++p) {

        delete p->first;
    }

    DoDumpJobs();

    _listen_proxy_handlers.clear();
    _job_workers.clear();
    _io_workers.clear();
    _io_context.clear();
    _listeners.clear();
    _hashjobs.clear();
    _timers.clear();
    _workers = 0;
    _slots = 0;

    // Preserve _channel.
    return true;
}

bool EasyServer::AttachListeners(LinkageWorker *worker)
{
    for (std::list<Listener *>::const_iterator p = _listeners.begin();
         p != _listeners.end(); ++p) {

        Listener *l = *p;
        if (!l->Attach(worker)) {
            return false;
        }
    }

    return true;
}

Runnable *EasyServer::GetJob(size_t id)
{
    MutexLocker glocker(_gmutex);
    while (_jobs.empty() && _hashjobs[id].empty()) {
        _incoming->Wait(_gmutex);
    }

    Runnable *job;
    if (!_hashjobs[id].empty()) {
        CLOG.Verbose("EasyServer: got job from my queue.");
        job = _hashjobs[id].front();
        _hashjobs[id].pop();
    } else {
        CLOG.Verbose("EasyServer: got job from global queue.");
        job = _jobs.front();
        _jobs.pop();
    }

    return job;
}

void EasyServer::DoAppendJob(Runnable *job, int hash)
{
    if (hash < 0) {
        _jobs.push(job);
        _incoming->WakeOne();
    } else {
        assert(!_hashjobs.empty());
        size_t i = static_cast<size_t>(hash) % _hashjobs.size();
        _hashjobs[i].push(job);
        _incoming->WakeAll();
    }

    CLOG.Verbose("EasyServer: appended job [%p] %d", job, hash);
}

std::pair<EasyHandler *, bool> EasyServer::GetEasyHandler(ProxyHandler *proxy_handler)
{
    assert(proxy_handler);

    Factory<EasyHandler> *const f = proxy_handler->easy_factory();
    if (f) {
        return std::make_pair(f->Create(), true);
    } else {
        return std::make_pair(proxy_handler->easy_handler(), false);
    }
}

AbstractIo *EasyServer::GetAbstractIo(ProxyHandler *proxy_handler,
                                      Interface *interface,
                                      bool client_or_server,
                                      bool socket_connecting)
{
    assert(proxy_handler);
    assert(interface);

    if (proxy_handler->ssl()) {
        return new SslIo(interface,
                         client_or_server,
                         socket_connecting,
                         proxy_handler->ssl());

    } else {
        return new FileDescriptorIo(interface,
                                    socket_connecting);
    }
}

Linkage *EasyServer::AllocateChannel(LinkageWorker *worker,
                                     const LinkagePeer &peer,
                                     const LinkagePeer &me,
                                     ProxyHandler *proxy_handler)
{
    ProxyLinkageWorker *const w = static_cast<ProxyLinkageWorker *>(worker);
    IoContext *const ioc = _io_context[static_cast<size_t>(w->thread_id())];

    uint64_t inconn = _incoming_connections.AddAndFetch(1);
    if (inconn >= _configure.maximum_incoming_connections) {
        LOG(VERBOSE) << "EasyServer: incoming connections over limit: "
                     << _configure.maximum_incoming_connections;

        _incoming_connections.SubAndFetch(1);
        return NULL;
    }

    MutexLocker locker(ioc->_mutex);
    channel_t channel = AllocateChannel(ioc, true);

    Interface *interface = new Interface;
    // 初始化连接
    if (!interface->Accepted(proxy_handler->accepted_option(), peer.fd())) {
        _incoming_connections.SubAndFetch(1);
        delete interface;
        return NULL;
    }

    std::pair<EasyHandler *, bool> h = GetEasyHandler(proxy_handler);

    EasyContext *context = new EasyContext(this, h.first, h.second,
                                           channel, peer, me,
                                           w->thread_id());

    AbstractIo *io = GetAbstractIo(proxy_handler, interface, false, false);
    ProxyLinkage *linkage = new ProxyLinkage(context, io, proxy_handler, peer, me);

    linkage->set_receive_timeout(_configure.incoming_receive_timeout);
    linkage->set_connect_timeout(_configure.incoming_connect_timeout);
    linkage->set_send_timeout(_configure.incoming_send_timeout);
    linkage->set_idle_timeout(_configure.incoming_idle_timeout);

    ioc->_channel_linkages.insert(std::make_pair(channel, linkage));
    LOG(VERBOSE) << "EasyServer: creating linkage: " << peer.ip_str();
    return linkage;
}

void EasyServer::ReleaseChannel(channel_t channel)
{
    IoContext *const ioc = GetIoContext(channel);
    if (!ioc) {
        return;
    }

    MutexLocker locker(ioc->_mutex);
    ioc->_channel_linkages.erase(channel);
    if (!IsOutgoingChannel(channel)) {
        _incoming_connections.SubAndFetch(1);
        return;
    }

    _outgoing_connections.SubAndFetch(1);
    if (ioc->_outgoing_informations.find(channel) != ioc->_outgoing_informations.end()) {
        // Still needed.
        return;
    }

    LOG(VERBOSE) << "EasyServer: outgoing ProxyHandler released: " << channel;
    connect_map_t::iterator p = ioc->_connect_proxy_handlers.find(channel);
    assert(p != ioc->_connect_proxy_handlers.end());
    delete p->second;
    ioc->_connect_proxy_handlers.erase(p);
}

void EasyServer::QueueOrExecuteJob(Runnable *job, int id)
{
    if (!job) {
        return;
    }

    // _workers are only set in single threaded environment,
    // skip locking to get better performance.
    if (_workers) {
        MutexLocker glocker(_gmutex);
        DoAppendJob(job, id);
        return;
    }

    job->Run();
    delete job;
}

bool EasyServer::QueueIo(Runnable *job, int thread_id)
{
    if (!job) {
        return true;
    }

    MutexLocker glocker(_gmutex);
    std::vector<ProxyLinkageWorker *>::iterator p = _io_workers.begin();
    if (thread_id) {
        std::advance(p, thread_id);
    }

    ProxyLinkageWorker *worker = *p;
    return worker->SendCommand(job);
}

void EasyServer::Forget(channel_t channel)
{
    IoContext *const ioc = GetIoContext(channel);
    if (!ioc) {
        return;
    }

    MutexLocker locker(ioc->_mutex);
    outgoing_map_t::iterator p = ioc->_outgoing_informations.find(channel);
    if (p != ioc->_outgoing_informations.end()) {
        delete p->second;
        ioc->_outgoing_informations.erase(p);
    }

    if (ioc->_channel_linkages.find(channel) != ioc->_channel_linkages.end()) {
        // Still needed.
        return;
    }

    connect_map_t::iterator q = ioc->_connect_proxy_handlers.find(channel);
    if (q == ioc->_connect_proxy_handlers.end()) {
        return;
    }

    delete q->second;
    ioc->_connect_proxy_handlers.erase(q);
    LOG(VERBOSE) << "EasyServer: outgoing ProxyHandler released: " << channel;
}

void EasyServer::Forget(const EasyContext &context)
{
    Forget(context.channel());
}

bool EasyServer::Disconnect(channel_t channel, bool finish_write)
{
    IoContext *const ioc = GetIoContext(channel);
    if (!ioc) {
        return true;
    }

    int64_t tid = get_current_thread_id();
    MutexLocker locker(ioc->_mutex);
    channel_map_t::iterator p = ioc->_channel_linkages.find(channel);
    if (p == ioc->_channel_linkages.end()) {
        return true;
    }

    ProxyLinkage *linkage = p->second;
    LinkageWorker *worker = linkage->worker();

    if (tid > 0 && tid == worker->running_thread_id()) {
        locker.Unlock();
        int ret = linkage->Disconnect(finish_write);
        return ret >= 0;
    }

    Runnable *job = new DisconnectJob(this, channel, finish_write);
    return worker->SendCommand(job);
}

bool EasyServer::Disconnect(const EasyContext &context, bool finish_write)
{
    return Disconnect(context.channel(), finish_write);
}

void EasyServer::DoRealDisconnect(channel_t channel, bool finish_write)
{
    IoContext *const ioc = GetIoContext(channel);
    if (!ioc) {
        return;
    }

    MutexLocker locker(ioc->_mutex);
    channel_map_t::iterator p = ioc->_channel_linkages.find(channel);
    if (p == ioc->_channel_linkages.end()) {
        return;
    }

    ProxyLinkage *linkage = p->second;

    // Same thread, no need to prevent pointer being freed.
    locker.Unlock();
    linkage->Disconnect(finish_write);
}

void EasyServer::DoRealSend(ProxyLinkageWorker *worker,
                            channel_t channel,
                            const void *buffer,
                            size_t length)
{
    IoContext *const ioc = GetIoContext(channel);
    if (!ioc) {
        return;
    }

    MutexLocker locker(ioc->_mutex);
    ProxyLinkage *linkage = NULL;
    channel_map_t::iterator p = ioc->_channel_linkages.find(channel);

    if (p != ioc->_channel_linkages.end()) {
        linkage = p->second;

    } else if (IsOutgoingChannel(channel)) {
        outgoing_map_t::const_iterator q = ioc->_outgoing_informations.find(channel);
        if (q != ioc->_outgoing_informations.end()) {
            linkage = DoReconnect(worker, channel, q->second);
        }
    }

    // Same thread, no need to prevent pointer being freed.
    locker.Unlock();

    if (!linkage || !buffer) {
        return;
    }

    if (!linkage->Send(buffer, length)) {
        const EasyContext &context = *linkage->context();
        EasyHandler *h = context.easy_handler();
        h->OnError(context, false, ENOMEM);
        linkage->Disconnect(false);
    }
}

// 发送消息
bool EasyServer::Send(channel_t channel, const void *buffer, size_t length)
{
    IoContext *const ioc = GetIoContext(channel);
    if (!ioc) {
        return false;
    }

    // 获取当前线程id
    int64_t tid = get_current_thread_id();
    MutexLocker locker(ioc->_mutex);
    ProxyLinkageWorker *worker = NULL;
    channel_map_t::iterator p = ioc->_channel_linkages.find(channel);

    // 如果channel存在
    if (p != ioc->_channel_linkages.end()) {
        if (!buffer || !length) {
            return true;
        }

        worker = static_cast<ProxyLinkageWorker *>(p->second->worker());
        if (tid > 0 && tid == worker->running_thread_id()) {
            locker.Unlock();
            if (!buffer || !length) {
                return true;
            }
            return p->second->Send(buffer, length);
        }

    } else if (IsOutgoingChannel(channel)) {
        outgoing_map_t::const_iterator q = ioc->_outgoing_informations.find(channel);
        if (q != ioc->_outgoing_informations.end()) {
            worker = GetIoWorker(q->second->thread_id());
            if (tid > 0 && tid == worker->running_thread_id()) {
                Linkage *linkage = DoReconnect(worker, channel, q->second);
                if (!linkage) {
                    return false;
                }

                locker.Unlock();
                if (!buffer || !length) {
                    return true;
                }

                return linkage->Send(buffer, length);
            }
        }
    }

    locker.Unlock();
    if (!worker) {
        return true;
    }

    Runnable *job = new SendJob(this, worker, channel, buffer, length);
    return worker->SendCommand(job);
}

bool EasyServer::Send(const EasyContext &context, const void *buffer, size_t length)
{
    return Send(context.channel(), buffer, length);
}

// 如果thread_id等于-1，则随机取一个
EasyServer::ProxyLinkageWorker *EasyServer::GetIoWorker(int thread_id) const
{
    std::vector<ProxyLinkageWorker *>::const_iterator p = _io_workers.begin();

    int r = thread_id;
    if (r < 0 || static_cast<size_t>(r) >= _io_workers.size()) {
        r = rand() % static_cast<int>(_io_workers.size());
    }

    if (r) {
        std::advance(p, r);
    }

    return *p;
}

EasyServer::ProxyLinkage *EasyServer::DoReconnect(
        ProxyLinkageWorker *worker,
        channel_t channel,
        const OutgoingInformation *info)
{
    IoContext *const ioc = GetIoContext(channel);
    assert(ioc);

    LinkagePeer me;
    LinkagePeer peer;
    Interface *interface = new Interface;
    std::pair<EasyHandler *, bool> h = GetEasyHandler(info->proxy_handler());
    
    int ret = interface->Connect(info->socket(), info->option(), &peer, &me);

    EasyContext *context = new EasyContext(this, h.first, h.second,
                                           channel, peer, me,
                                           worker->thread_id());

    if (ret < 0) {
        h.first->OnError(*context, false, errno);
        h.first->OnDisconnected(*context);
        delete interface;
        delete context;
        return NULL;
    }

    AbstractIo *io = GetAbstractIo(info->proxy_handler(),
                                   interface,
                                   true,
                                   ret > 0 ? true : false);

    ProxyLinkage *linkage = new ProxyLinkage(context, io, info->proxy_handler(),
                                             peer, me);

    if (!linkage->Attach(worker)) {
        h.first->OnError(*context, false, errno);
        delete linkage;
        return NULL;
    }

    linkage->set_receive_timeout(_configure.outgoing_receive_timeout);
    linkage->set_connect_timeout(_configure.outgoing_receive_timeout);
    linkage->set_send_timeout(_configure.outgoing_send_timeout);
    linkage->set_idle_timeout(_configure.outgoing_idle_timeout);

    ioc->_channel_linkages.insert(std::make_pair(channel, linkage));
    _outgoing_connections.AddAndFetch(1);
    return linkage;
}

EasyServer::channel_t EasyServer::Connect(const ConnectOption &o)
{
    if (!!o.easy_handler == !!o.easy_factory) {
        LOG(FATAL) << "EasyServer: BUG: either handler or factory must be configured.";
        return false;
    }

    MutexLocker glocker(_gmutex);
    if (_io_workers.empty()) {
        throw std::logic_error("EasyServer only connects after Initialize()");
    }

    int thread_id = o.thread_id;
    if (thread_id < 0 || static_cast<size_t>(thread_id) >= _io_workers.size()) {
        thread_id = rand() % static_cast<int>(_io_workers.size());
    }

    IoContext *const ioc = _io_context[static_cast<size_t>(thread_id)];

    MutexLocker locker(ioc->_mutex);
    ProxyHandler *proxy_handler = new ProxyHandler(Interface::Option(),
                                                   o.easy_handler,
                                                   o.easy_factory,
                                                   o.ssl);

    OutgoingInformation *info =
            new OutgoingInformation(proxy_handler,
                                    o.connect_socket,
                                    o.connect_option,
                                    thread_id);

    channel_t c = AllocateChannel(ioc, false);
    ioc->_connect_proxy_handlers.insert(std::make_pair(c, proxy_handler));
    ioc->_outgoing_informations.insert(std::make_pair(c, info));
    return c;
}

EasyServer::channel_t EasyServer::ConnectTcp4(
        const std::string &host,
        uint16_t port,
        EasyHandler *easy_handler,
        int thread_id)
{
    if (!easy_handler) {
        LOG(FATAL) << "EasyServer: BUG: no handler configured.";
        return kInvalidChannel;
    }

    ConnectOption o;
    o.connect_socket.domain = AF_INET;
    o.connect_socket.socket_hostname = host.c_str();
    o.connect_socket.socket_port = port;
    o.easy_handler = easy_handler;
    o.thread_id = thread_id;

    return Connect(o);
}

EasyServer::channel_t EasyServer::SslConnectTcp4(
        const std::string &host,
        uint16_t port,
        SslContext *ssl,
        EasyHandler *easy_handler,
        int thread_id)
{
    if (!ssl) {
        LOG(FATAL) << "EasyServer: BUG: no SSL context configured.";
        return false;
    }

    if (!easy_handler) {
        LOG(FATAL) << "EasyServer: BUG: no handler configured.";
        return kInvalidChannel;
    }

    ConnectOption o;
    o.connect_socket.domain = AF_INET;
    o.connect_socket.socket_hostname = host.c_str();
    o.connect_socket.socket_port = port;
    o.easy_handler = easy_handler;
    o.thread_id = thread_id;
    o.ssl = ssl;

    return Connect(o);
}

EasyServer::channel_t EasyServer::ConnectTcp4(
        const std::string &host,
        uint16_t port,
        Factory<EasyHandler> *easy_factory,
        int thread_id)
{
    if (!easy_factory) {
        LOG(FATAL) << "EasyServer: BUG: no handler factory configured.";
        return kInvalidChannel;
    }

    ConnectOption o;
    o.connect_socket.domain = AF_INET;
    o.connect_socket.socket_hostname = host.c_str();
    o.connect_socket.socket_port = port;
    o.easy_factory = easy_factory;
    o.thread_id = thread_id;

    return Connect(o);
}

EasyServer::channel_t EasyServer::SslConnectTcp4(
        const std::string &host,
        uint16_t port,
        SslContext *ssl,
        Factory<EasyHandler> *easy_factory,
        int thread_id)
{
    if (!ssl) {
        LOG(FATAL) << "EasyServer: BUG: no SSL context configured.";
        return false;
    }

    if (!easy_factory) {
        LOG(FATAL) << "EasyServer: BUG: no handler factory configured.";
        return kInvalidChannel;
    }

    ConnectOption o;
    o.connect_socket.domain = AF_INET;
    o.connect_socket.socket_hostname = host.c_str();
    o.connect_socket.socket_port = port;
    o.easy_factory = easy_factory;
    o.thread_id = thread_id;
    o.ssl = ssl;

    return Connect(o);
}

EasyServer::channel_t EasyServer::AllocateChannel(IoContext *ioc,
                                                  bool incoming_or_outgoing)
{
    // 先自加，生成新的channel
    // 从这里看，生成的channel都是等差的，差值是slots, 这样就不会和其他线程生成的冲突了
    ioc->_channel += _slots;
    channel_t channel = ioc->_channel;
    
    // 用一个掩码来代表，这个连接是incoming的还是outgoing的
    if (!incoming_or_outgoing) {
        channel |= 0x8000000000000000ull;
    }

    return channel;
}

} // namespace flinter

std::ostream &operator << (std::ostream &s, const flinter::EasyServer::ListenOption &d)
{
    s <<   "L[" << d.listen_socket
      << "] O[" << d.listen_option
      << "] A[" << d.accepted_option
      << "] H[" << d.easy_handler
      << "] F[" << d.easy_factory
      << "] S[" << d.ssl
      << "]";

    return s;
}

std::ostream &operator << (std::ostream &s, const flinter::EasyServer::ConnectOption &d)
{
    s <<   "L[" << d.connect_socket
      << "] O[" << d.connect_option
      << "] H[" << d.easy_handler
      << "] F[" << d.easy_factory
      << "] S[" << d.ssl
      << "] T[" << d.thread_id
      << "]";

    return s;
}
