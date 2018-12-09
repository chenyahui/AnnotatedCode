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

#ifndef FLINTER_LINKAGE_EASY_SERVER_H
#define FLINTER_LINKAGE_EASY_SERVER_H

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <ostream>
#include <queue>
#include <string>
#include <vector>

#include <flinter/linkage/interface.h>
#include <flinter/types/atomic.h>
#include <flinter/types/unordered_map.h>
#include <flinter/factory.h>
#include <flinter/runnable.h>

namespace flinter {

class AbstractIo;
class Condition;
class EasyContext;
class EasyHandler;
class EasyTuner;
class FixedThreadPool;
class Interface;
class Linkage;
class LinkagePeer;
class LinkageWorker;
class Listener;
class Mutex;
class MutexLocker;
class SslContext;

class EasyServer {
public:
    typedef uint64_t channel_t;

    struct Configure {
        int64_t incoming_receive_timeout;
        int64_t incoming_connect_timeout;
        int64_t incoming_send_timeout;
        int64_t incoming_idle_timeout;

        int64_t outgoing_receive_timeout;
        int64_t outgoing_connect_timeout;
        int64_t outgoing_send_timeout;
        int64_t outgoing_idle_timeout;

         size_t maximum_incoming_connections;

    }; // struct Configure

    struct ListenOption {
        Interface::Socket listen_socket;
        Interface::Option listen_option;
        Interface::Option accepted_option;
        Factory<EasyHandler> *easy_factory;
        EasyHandler *easy_handler;
        SslContext *ssl;

        ListenOption();
        void ToString(std::string *str) const;
    }; // struct ListenOption

    struct ConnectOption {
        Interface::Socket connect_socket;
        Interface::Option connect_option;
        Factory<EasyHandler> *easy_factory;
        EasyHandler *easy_handler;
        SslContext *ssl;
        int thread_id;

        ConnectOption();
        void ToString(std::string *str) const;
    }; // struct ConnectOption

    EasyServer();
    virtual ~EasyServer();

    /// @param interval nanoseconds.
    /// @param timer will be released after executed.
    bool RegisterTimer(int64_t interval, Runnable *timer);

    /// @param after nanoseconds.
    /// @param repeat nanoseconds.
    /// @param timer will be released after executed.
    bool RegisterTimer(int64_t after, int64_t repeat, Runnable *timer);

    /// Call before Initialize().
    /// Either handler or factory must be set.
    /// handler/factory life span NOT taken, keep it valid.
    bool Listen(const ListenOption &o);

    /// Allocate channel for outgoing connection.
    /// Call after Initialize().
    /// @sa Forget()
    /// Either handler or factory must be set.
    /// handler/factory life span NOT taken, keep it valid.
    channel_t Connect(const ConnectOption &o);

    /// Call before Initialize().
    /// @param easy_handler life span NOT taken, keep it valid.
    bool Listen(uint16_t port, EasyHandler *easy_handler);

    /// Call before Initialize().
    /// @param easy_factory life span NOT taken, keep it valid.
    bool Listen(uint16_t port, Factory<EasyHandler> *easy_factory);

    /// Call before Initialize().
    /// @param ssl life span NOT taken, keep it valid.
    /// @param easy_handler life span NOT taken, keep it valid.
    bool SslListen(uint16_t port, SslContext *ssl, EasyHandler *easy_handler);

    /// Call before Initialize().
    /// @param ssl life span NOT taken, keep it valid.
    /// @param easy_factory life span NOT taken, keep it valid.
    bool SslListen(uint16_t port,
                   SslContext *ssl,
                   Factory<EasyHandler> *easy_factory);

    /// Call before Initialize(), change content directly.
    Configure *configure()
    {
        return &_configure;
    }

    /// @param slots I/O thread affinities, empty list means no affinity.
    /// @param workers job threads affinities, NULL to share I/O threads.
    /// @param easy_tuner life span NOT taken, keep it valid.
    /// @sa explode_lists()
    bool Initialize(const std::string &slots,
                    const std::string *workers,
                    EasyTuner *easy_tuner = NULL);

    /// @param slots how many I/O threads, typically 1~4.
    /// @param workers how many job threads, 0 to share I/O threads.
    /// @param easy_tuner life span NOT taken, keep it valid.
    bool Initialize(size_t slots,
                    size_t workers,
                    EasyTuner *easy_tuner = NULL);

    bool Disconnect(channel_t channel, bool finish_write = true);
    bool Disconnect(const EasyContext &context, bool finish_write = true);

    /// For disconnected incoming connections, message is silently dropped.
    /// For disconnected outgoing connections, new connection is made and sent.
    bool Send(channel_t channel, const void *buffer, size_t length);
    bool Send(const EasyContext &context, const void *buffer, size_t length);

    /// Allocate channel for outgoing connection.
    /// Call after Initialize().
    /// @sa Forget()
    channel_t ConnectTcp4(const std::string &host,
                          uint16_t port,
                          EasyHandler *easy_handler,
                          int thread_id = -1);

    /// Allocate channel for outgoing connection.
    /// Call after Initialize().
    /// @sa Forget()
    channel_t ConnectTcp4(const std::string &host,
                          uint16_t port,
                          Factory<EasyHandler> *easy_factory,
                          int thread_id = -1);

    /// Allocate channel for outgoing connection.
    /// Call after Initialize().
    /// @sa Forget()
    channel_t SslConnectTcp4(const std::string &host,
                             uint16_t port,
                             SslContext *ssl,
                             EasyHandler *easy_handler,
                             int thread_id = -1);

    /// Allocate channel for outgoing connection.
    /// Call after Initialize().
    /// @sa Forget()
    channel_t SslConnectTcp4(const std::string &host,
                             uint16_t port,
                             SslContext *ssl,
                             Factory<EasyHandler> *easy_factory,
                             int thread_id = -1);

    /// Remove outgoing connection information after disconnected.
    void Forget(channel_t channel);
    void Forget(const EasyContext &context);

    /// Run job in any job threads, or current thread if there's none.
    /// @param job will be released after executed.
    /// @param hash to map to worker thread if appliable, -1 for any worker.
    void QueueOrExecuteJob(Runnable *job, int hash = -1);

    /// Queue job in specified I/O thread, assertion if thread_id is invalid.
    bool QueueIo(Runnable *job, int thread_id);

    /// Thread safe.
    bool Shutdown();

    static bool IsOutgoingChannel(channel_t channel)
    {
        return !!(channel & 0x8000000000000000ull);
    }

    static bool IsValidChannel(channel_t channel)
    {
        return channel != kInvalidChannel;
    }

    static const channel_t kInvalidChannel;
    static const size_t kMaximumWorkers;
    static const size_t kMaximumSlots;

private:
    class OutgoingInformation;
    class ProxyLinkageWorker;
    class RegisterTimerJob;
    class ProxyListener;
    class DisconnectJob;
    class ProxyLinkage;
    class ProxyHandler;
    class JobWorker;
    class IoContext;
    class SendJob;

    // Locked.
    void ReleaseChannel(channel_t channel);

    // Called by JobWorker thread.
    Runnable *GetJob(size_t id);

    // Called by ProxyListener.
    Linkage *AllocateChannel(LinkageWorker *worker,
                             const LinkagePeer &peer,
                             const LinkagePeer &me,
                             ProxyHandler *proxy_handler);

    // Called by EasyServer itself.
    static std::pair<EasyHandler *, bool> GetEasyHandler(ProxyHandler *proxy_handler);
    static AbstractIo *GetAbstractIo(ProxyHandler *proxy_handler,
                                     Interface *interface,
                                     bool client_or_server,
                                     bool socket_connecting);

    channel_t AllocateChannel(IoContext *ioc, bool incoming_or_outgoing);
    bool AttachListeners(LinkageWorker *worker);
    void DoAppendJob(Runnable *job, int hash); // No lock.
    bool DoShutdown(MutexLocker *locker);
    void DoDumpJobs();

    // Locked.
    int GetThreadId(channel_t channel) const
    {
        return static_cast<int>((channel & 0x7fffffffffffffffull) % _slots);
    }

    // Locked.
    IoContext *GetIoContext(channel_t channel) const
    {
        if (!IsValidChannel(channel)) {
            return NULL;
        }

        return _io_context[static_cast<size_t>(GetThreadId(channel))];
    }

    // thread_id can be out of range so a random one is picked.
    ProxyLinkageWorker *GetIoWorker(int thread_id) const;

    ProxyLinkage *DoReconnect(ProxyLinkageWorker *worker,
                              channel_t channel,
                              const OutgoingInformation *info);

    // Called by jobs from I/O workers.
    void DoRealDisconnect(channel_t channel, bool finish_write);
    void DoRealSend(ProxyLinkageWorker *worker,
                    channel_t channel,
                    const void *buffer,
                    size_t length);

    static const Configure kDefaultConfigure;

    typedef std::unordered_map<channel_t, OutgoingInformation *> outgoing_map_t;
    typedef std::unordered_map<channel_t, ProxyLinkage *> channel_map_t;
    typedef std::unordered_map<channel_t, ProxyHandler *> connect_map_t;

    std::list<std::pair<Runnable *, std::pair<int64_t, int64_t> > > _timers;
    std::list<ProxyHandler *> _listen_proxy_handlers;
    std::vector<std::queue<Runnable *> > _hashjobs;
    std::vector<ProxyLinkageWorker *> _io_workers;
    std::vector<JobWorker *> _job_workers;
    std::list<Listener *> _listeners;
    std::queue<Runnable *> _jobs;
    FixedThreadPool *const _pool;
    Condition *const _incoming;
    Configure _configure;
    Mutex *const _gmutex;

    // For efficiency, these variables are not lock protected.
    size_t _workers;
    size_t _slots;

    // Locked per I/O thread.
    std::vector<IoContext *> _io_context;

    // Global connection management.
    uatomic64_t _incoming_connections;
    uatomic64_t _outgoing_connections;

}; // class EasyServer

} // namespace flinter

std::ostream &operator << (std::ostream &s, const flinter::EasyServer::ListenOption &d);
std::ostream &operator << (std::ostream &s, const flinter::EasyServer::ConnectOption &d);

#endif // FLINTER_LINKAGE_EASY_SERVER_H
