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

#ifndef FLINTER_LINKAGE_LINKAGE_H
#define FLINTER_LINKAGE_LINKAGE_H

#include <sys/types.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <flinter/linkage/abstract_io.h>
#include <flinter/linkage/linkage_base.h>

namespace flinter {

class LinkageHandler;
class LinkagePeer;

class Linkage : public LinkageBase {
public:
    /// Won't be initialized until Attach() is called.
    /// @param io life span TAKEN.
    /// @param handler life span NOT taken.
    /// @param peer copied.
    /// @param me copied.
    Linkage(AbstractIo *io,
            LinkageHandler *handler,
            const LinkagePeer &peer,
            const LinkagePeer &me);

    virtual ~Linkage();

    virtual int Disconnect(bool finish_write = true);
    virtual bool Attach(LinkageWorker *worker);
    virtual bool Detach(LinkageWorker *worker);
    virtual bool Cleanup(int64_t now);

    virtual int OnReceived(const void *buffer, size_t length);
    virtual void OnError(bool reading_or_writing, int errnum);
    virtual void OnDisconnected();
    virtual bool OnConnected();

    /// @return true sent or queued.
    virtual bool Send(const void *buffer, size_t length);

    void set_receive_timeout(int64_t timeout) { _receive_timeout = timeout; }
    void set_connect_timeout(int64_t timeout) { _connect_timeout = timeout; }
    void set_send_timeout   (int64_t timeout) { _send_timeout    = timeout; }
    void set_idle_timeout   (int64_t timeout) { _idle_timeout    = timeout; }

    static const int64_t kDefaultReceiveTimeout;
    static const int64_t kDefaultConnectTimeout;
    static const int64_t kDefaultSendTimeout;
    static const int64_t kDefaultIdleTimeout;

    const LinkagePeer *peer() const
    {
        return _peer;
    }

    const LinkagePeer *me() const
    {
        return _me;
    }

    LinkageWorker *worker()
    {
        return _worker;
    }

    AbstractIo *io()
    {
        return _io;
    }

protected:
    /// Return the packet size even if it's incomplete as long as you can tell.
    /// Very handy when you write the message length in the header.
    ///
    /// @return >0 message length.
    /// @return  0 message length is yet determined, keep receiving.
    /// @return <0 message is invalid.
    virtual ssize_t GetMessageLength(const void *buffer, size_t length);

    /// @return >0 keep coming.
    /// @return  0 hang up gracefully.
    /// @return <0 error occurred, hang up immediately.
    virtual int OnMessage(const void *buffer, size_t length);

    /// @return >0 don't change event monitoring status.
    /// @return  0 no more data to read, hang up gracefully.
    /// @return <0 error occurred, drop connection immediately.
    virtual int OnReadable(LinkageWorker *worker);

    /// @return >0 don't change event monitoring status.
    /// @return  0 no more data to write, hang up gracefully.
    /// @return <0 error occurred, drop connection immediately.
    virtual int OnWritable(LinkageWorker *worker);

    /// @return >0 don't change event monitoring status.
    /// @return  0 shut down completed successfully.
    /// @return <0 error occurred, drop connection immediately.
    virtual int Shutdown(AbstractIo::Status *status);

private:
    static const char *GetActionString(const AbstractIo::Action &action);

    int DoReceived(const void *buffer, size_t length, size_t *consumed);
    bool AppendSendingBuffer(const void *buffer, size_t length);
    void PickSendingBuffer(const void **buffer, size_t *length);
    void ConsumeSendingBuffer(size_t length);
    size_t GetSendingBufferSize() const;
    void DumpSendingBuffer();

    int OnEvent(LinkageWorker *worker,
                const AbstractIo::Action &idle_action);

    /// @param next_action set to AbstractIo::kActionNone before calling.
    int OnEventOnce(LinkageWorker *worker,
                    const AbstractIo::Action &action,
                    AbstractIo::Action *next_action);

    int AfterEvent(LinkageWorker *worker,
                   const AbstractIo::Status &status);

    bool DoConnected();

    /// Connection is being established or closed.
    void UpdateConnectJam(bool jammed);

    /// Incoming message is incomplete.
    void UpdateReceiveJam(bool jammed);

    /// Some bytes are just received.
    void UpdateLastReceived();

    /// @param sent if some bytes are just sent.
    /// @param jammed if the message is incomplete.
    void UpdateLastSent(bool sent, bool jammed);

    // Not a very good data structure, use it for now.
    std::vector<unsigned char> _rbuffer;

    // Only used when kernel send buffer is full.
    std::vector<unsigned char> _wbuffer;

    // If jammed, find exactly the same length of last write.
    size_t _last_writing;

    LinkageHandler *const _handler;
    LinkagePeer *const _peer;
    LinkagePeer *const _me;
    AbstractIo *const _io;

    int64_t _receive_timeout;
    int64_t _connect_timeout;
    int64_t _last_received;
    int64_t _idle_timeout;
    int64_t _send_timeout;
    int64_t _receive_jam;
    int64_t _connect_jam;
    int64_t _last_sent;
    int64_t _send_jam;

    AbstractIo::Action _action;
    LinkageWorker *_worker;
    size_t _rlength;
    bool _graceful;
    bool _closed;

}; // class Linkage

} // namespace flinter

#endif // FLINTER_LINKAGE_LINKAGE_H
