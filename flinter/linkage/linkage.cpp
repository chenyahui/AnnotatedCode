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

#include "flinter/linkage/linkage.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include "flinter/linkage/abstract_io.h"
#include "flinter/linkage/interface.h"
#include "flinter/linkage/linkage_handler.h"
#include "flinter/linkage/linkage_peer.h"
#include "flinter/linkage/linkage_worker.h"

#include "flinter/logger.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

#define INCOMPLETE(x) ((x) == AbstractIo::kStatusWannaRead || (x) == AbstractIo::kStatusWannaWrite)

namespace flinter {

const int64_t Linkage::kDefaultReceiveTimeout = 15000000000LL;  // 15s
const int64_t Linkage::kDefaultConnectTimeout = 15000000000LL;  // 15s
const int64_t Linkage::kDefaultSendTimeout    = 15000000000LL;  // 15s
const int64_t Linkage::kDefaultIdleTimeout    = 300000000000LL; // 5min

Linkage::Linkage(AbstractIo *io,
                 LinkageHandler *handler,
                 const LinkagePeer &peer,
                 const LinkagePeer &me)
        : _last_writing(0)
        , _handler(handler)
        , _peer(new LinkagePeer(peer))
        , _me(new LinkagePeer(me))
        , _io(io)
        , _receive_timeout(kDefaultReceiveTimeout)
        , _connect_timeout(kDefaultConnectTimeout)
        , _last_received(0)
        , _idle_timeout(kDefaultIdleTimeout)
        , _send_timeout(kDefaultSendTimeout)
        , _receive_jam(0)
        , _connect_jam(0)
        , _last_sent(0)
        , _send_jam(0)
        , _action(AbstractIo::kActionNone)
        , _worker(NULL)
        , _rlength(0)
        , _graceful(false)
        , _closed(false)
{
    assert(io);
    assert(handler);

    int64_t now = get_monotonic_timestamp();
    _last_received = now;
    _last_sent = now;
}

Linkage::~Linkage()
{
    delete _io;
    delete _me;
    delete _peer;
}

ssize_t Linkage::GetMessageLength(const void *buffer, size_t length)
{
    return _handler->GetMessageLength(this, buffer, length);
}

int Linkage::OnMessage(const void *buffer, size_t length)
{
    return _handler->OnMessage(this, buffer, length);
}

void Linkage::OnError(bool reading_or_writing, int errnum)
{
    _handler->OnError(this, reading_or_writing, errnum);
}

void Linkage::OnDisconnected()
{
    _handler->OnDisconnected(this);
}

bool Linkage::OnConnected()
{
    return _handler->OnConnected(this);
}

bool Linkage::AppendSendingBuffer(const void *buffer, size_t length)
{
    static const size_t kMaximumBytes = 64 * 1024 * 1024;

    assert(buffer);
    assert(length);

    const unsigned char *p = reinterpret_cast<const unsigned char *>(buffer);
    size_t left = kMaximumBytes - _wbuffer.size();
    if (left < length) {
        CLOG.Verbose("Linkage: only [%lu] bytes left for fd = %d",
                     left, _peer->fd());

        return false;
    }

    _wbuffer.insert(_wbuffer.end(), p, p + length);
    return true;
}

void Linkage::PickSendingBuffer(const void **buffer, size_t *length)
{
    assert(buffer);
    assert(length);

    if (_wbuffer.empty()) {
        *buffer = NULL;
        *length = 0;
        return;
    }

    if (_last_writing) {
        *length = _last_writing;
        _last_writing = 0;

    } else {
        *length = _wbuffer.size();
    }

    *buffer = &_wbuffer[0];
}

void Linkage::ConsumeSendingBuffer(size_t length)
{
    assert(length <= _wbuffer.size());
    std::vector<unsigned char>::iterator p = _wbuffer.begin();
    std::advance(p, static_cast<ssize_t>(length));
    _wbuffer.erase(_wbuffer.begin(), p);
}

size_t Linkage::GetSendingBufferSize() const
{
    return _wbuffer.size();
}

void Linkage::DumpSendingBuffer()
{
    _wbuffer.clear();
}

int Linkage::OnReadable(LinkageWorker *worker)
{
    return OnEvent(worker, AbstractIo::kActionRead);
}

int Linkage::OnWritable(LinkageWorker *worker)
{
    return OnEvent(worker, AbstractIo::kActionWrite);
}

bool Linkage::DoConnected()
{
    bool wanna_write = !!GetSendingBufferSize();
    _worker->SetWanna(this, true, wanna_write);
    _action = AbstractIo::kActionNone;
    UpdateConnectJam(false);
    UpdateLastReceived();
    return OnConnected();
}

int Linkage::OnEvent(LinkageWorker *worker,
                     const AbstractIo::Action &idle_action)
{
    assert(worker);
    assert(idle_action != AbstractIo::kActionNone);

    AbstractIo::Action action = _action;
    if (action == AbstractIo::kActionNone) {
        action = idle_action;
    }

    int ret;
    while (true) {
        AbstractIo::Action next_action = AbstractIo::kActionNone;
        ret = OnEventOnce(worker, action, &next_action);
        if (next_action == AbstractIo::kActionNone) {
            break;
        }

        CLOG.Verbose("Linkage: OnEventOnce() gives action [%d].", next_action);
        action = next_action;
    }

    if (_action == AbstractIo::kActionNone) {
        worker->SetWanna(this, true, !!GetSendingBufferSize());
    }

    return ret;
}

int Linkage::OnEventOnce(LinkageWorker *worker,
                         const AbstractIo::Action &action,
                         AbstractIo::Action *next_action)
{
    errno = 0;
    size_t retlen = 0;
    AbstractIo::Status status = AbstractIo::kStatusBug;
    if (action == AbstractIo::kActionRead) {
        bool more = false;
        unsigned char buffer[65536];
        status = _io->Read(buffer, sizeof(buffer), &retlen, &more);
        if (status == AbstractIo::kStatusOk) {
            _action = AbstractIo::kActionNone;
            int ret = OnReceived(buffer, retlen);
            if (ret == 0) {
                *next_action = AbstractIo::kActionShutdown;
                return 1;

            } else if (ret > 0) {
                if (more) {
                    *next_action = AbstractIo::kActionRead;
                }

                return 1;

            } else { // This is not a I/O error, don't fall into AfterEvent().
                return -1;
            }
        }

    } else if (action == AbstractIo::kActionWrite) {
        size_t length;
        const void *buffer;
        PickSendingBuffer(&buffer, &length);

        if (length) { // Something to write.
            status = _io->Write(buffer, length, &retlen);
            if (status == AbstractIo::kStatusOk) {
                _action = AbstractIo::kActionNone;
                if (retlen < length) {
                    UpdateLastSent(true, true);
                    ConsumeSendingBuffer(retlen);
                    CLOG.Verbose("Linkage: dequeued [%lu] bytes and sent [%lu] "
                                 "bytes for fd = %d", length, retlen, peer()->fd());

                    assert(GetSendingBufferSize());
                    return 1;

                } else {
                    UpdateLastSent(true, false);
                    ConsumeSendingBuffer(length);
                    CLOG.Verbose("Linkage: dequeued [%lu] bytes and sent [%lu] "
                                 "bytes for fd = %d", length, retlen, peer()->fd());

                    return 1;
                }

            } else {
                UpdateLastSent(false, true);
                if (INCOMPLETE(status)) {
                    _last_writing = length;
                }
            }

        } else if (_graceful) { // Finished writing.
            *next_action = AbstractIo::kActionShutdown;
            _action = AbstractIo::kActionNone;
            return 1;

        } else { // Nothing to write for now.
            _action = AbstractIo::kActionNone;
            return 1;
        }

    } else if (action == AbstractIo::kActionAccept) {
        status = _io->Accept();
        if (status == AbstractIo::kStatusOk) {
            _action = AbstractIo::kActionNone;
            return DoConnected() ? 1 : -1;
        }

    } else if (action == AbstractIo::kActionConnect) {
        status = _io->Connect();
        if (status == AbstractIo::kStatusOk) {
            _action = AbstractIo::kActionNone;
            return DoConnected() ? 1 : -1;
        }

    } else if (action == AbstractIo::kActionShutdown) {
        int ret = Shutdown(&status);
        if (ret <= 0) {
            // In case that I'm not called by I/O engine, enable reading.
            _action = AbstractIo::kActionNone;
            worker->SetWannaRead(this, true);
            return ret;
        }
    }

    // Action incomplete?
    _action = INCOMPLETE(status) ? action : AbstractIo::kActionNone;
    return AfterEvent(worker, status);
}

int Linkage::AfterEvent(LinkageWorker *worker,
                        const AbstractIo::Status &status)
{
    switch (status) {
    case AbstractIo::kStatusWannaRead:
        worker->SetWanna(this, true, false);
        return 1;

    case AbstractIo::kStatusWannaWrite:
        worker->SetWanna(this, false, true);
        return 1;

    case AbstractIo::kStatusClosed:
        return 0;

    case AbstractIo::kStatusOk:
        return 1;

    case AbstractIo::kStatusError:
    case AbstractIo::kStatusBug:
    default:
        return -1;
    };
}

const char *Linkage::GetActionString(const AbstractIo::Action &action)
{
    switch (action) {
    case AbstractIo::kActionRead:
        return "reading";

    case AbstractIo::kActionWrite:
        return "writing";

    case AbstractIo::kActionAccept:
        return "accepting";

    case AbstractIo::kActionConnect:
        return "connecting";

    case AbstractIo::kActionShutdown:
        return "closing";

    default:
        assert(false);
        return NULL;
    };
}

int Linkage::OnReceived(const void *buffer, size_t length)
{
    const unsigned char *ptr = reinterpret_cast<const unsigned char *>(buffer);
    if (!buffer) {
        return -1;
    } else if (!length) {
        return 1;
    }

    UpdateLastReceived();
    if (_graceful) {
        return 1;
    }

    CLOG.Verbose("Linkage: received [%lu] bytes for fd = %d", length, _peer->fd());

    // No need to move buffer.
    if (_rbuffer.empty()) {
        size_t consumed;
        int ret = DoReceived(ptr, length, &consumed);
        if (ret <= 0) {
            return ret;
        }

        if (consumed != length) {
            _rbuffer.insert(_rbuffer.end(), ptr + consumed, ptr + length);
        }

    // Buffer moving unavoidable, but I already know the minimum bytes to move.
    } else if (_rlength && _rlength <= _rbuffer.size() + length) {
        size_t last = _rbuffer.size();
        assert(_rlength > last);

        _rbuffer.insert(_rbuffer.end(), ptr, ptr + (_rlength - last));
        assert(_rbuffer.size() == _rlength);

        size_t consumed;
        int ret = DoReceived(&_rbuffer[0], _rlength, &consumed);
        if (ret <= 0) {
            return ret;
        }

        // Everything must have been extracted.
        _rbuffer.clear();

        // Now the remaining buffer.
        ptr += consumed - last;
        length -= consumed - last;
        if (length) {
            ret = DoReceived(ptr, length, &consumed);
            if (ret <= 0) {
                return ret;
            }

            if (consumed != length) {
                _rbuffer.insert(_rbuffer.end(), ptr + consumed, ptr + length);
            }
        }

    // The worst case, all the buffer must be moved.
    } else {
        _rbuffer.insert(_rbuffer.end(), ptr, ptr + length);

        size_t consumed;
        int ret = DoReceived(&_rbuffer[0], _rbuffer.size(), &consumed);
        if (ret <= 0) {
            return ret;
        }

        if (consumed) {
            std::vector<unsigned char>::iterator p = _rbuffer.begin();
            std::advance(p, static_cast<ssize_t>(consumed));
            _rbuffer.erase(_rbuffer.begin(), p);
        }
    }

    return 1;
}

int Linkage::DoReceived(const void *buffer, size_t length, size_t *consumed)
{
    const unsigned char *ptr = reinterpret_cast<const unsigned char *>(buffer);
    size_t remaining = length;

    *consumed = 0;
    while (remaining) {
        if (!_rlength) {
            ssize_t ret = GetMessageLength(ptr, remaining);
            if (ret < 0) {
                CLOG.Verbose("Linkage: failed to get message length for "
                             "fd = %d", _peer->fd());

                return -1;

            } else if (ret == 0) {
                CLOG.Verbose("Linkage: message incomplete, keep receiving "
                             "for fd = %d", _peer->fd());

                UpdateReceiveJam(true);
                break;
            }

            _rlength = static_cast<size_t>(ret);
            CLOG.Verbose("Linkage: got message length [%lu] for fd = %d",
                         _rlength, _peer->fd());
        }

        if (remaining < _rlength) {
            UpdateReceiveJam(true);
            break;
        }

        UpdateReceiveJam(false);
        CLOG.Verbose("Linkage: processing message of length [%lu] for fd = %d",
                     _rlength, _peer->fd());

        int next = OnMessage(ptr, _rlength);
        if (next < 0) {
            CLOG.Verbose("Linkage: failed to process message of length [%lu] "
                         "for fd = %d", _rlength, _peer->fd());

            return -1;

        } else if (next == 0) {
            CLOG.Verbose("Linkage: processing message of length [%lu] but "
                         "instructed to shutdown for fd = %d", _rlength, _peer->fd());

            return 0;
        }

        remaining -= _rlength;
        *consumed += _rlength;
        ptr += _rlength;
        _rlength = 0;
    }

    return 1;
}

void Linkage::UpdateLastReceived()
{
    _last_received = get_monotonic_timestamp();
}

void Linkage::UpdateReceiveJam(bool jammed)
{
    if (jammed) {
        if (!_receive_jam) {
            _receive_jam = get_monotonic_timestamp();
        }
    } else {
        _receive_jam = 0;
    }
}

void Linkage::UpdateConnectJam(bool jammed)
{
    if (jammed) {
        if (!_connect_jam) {
            _connect_jam = get_monotonic_timestamp();
        }
    } else {
        _connect_jam = 0;
    }
}

void Linkage::UpdateLastSent(bool sent, bool jammed)
{
    int64_t now = get_monotonic_timestamp();
    if (sent) {
        _last_sent = now;
    }

    if (jammed) {
        if (!_send_jam) {
            _send_jam = now;
        }
    } else {
        _send_jam = 0;
    }
}

bool Linkage::Cleanup(int64_t now)
{
    // 整个清理的模式都是如下：
    // 
    // jammed_c表示被卡住的时间
    // _connect_timeout表示配置的连接超时时间
    // _connect_jam表示开始连接的时间，连接成功后，会置为0
    int64_t jammed_c = _connect_jam ? now - _connect_jam : 0;
    if (_connect_timeout && jammed_c && jammed_c >= _connect_timeout) {
        CLOG.Verbose("Linkage: connecting but timed out for fd = %d", _peer->fd());
        return false;
    }

    int64_t jammed_w = _send_jam ? now - _send_jam : 0;
    if (_send_timeout && jammed_w && jammed_w >= _send_timeout) {
        CLOG.Verbose("Linkage: writing but timed out for fd = %d", _peer->fd());
        return false;
    }

    int64_t jammed_r = _receive_jam ? now - _receive_jam : 0;
    if (_receive_timeout && jammed_r && jammed_r >= _receive_timeout) {
        CLOG.Verbose("Linkage: reading but timed out for fd = %d", _peer->fd());
        return false;
    }

    int64_t passed_w = now - _last_sent;
    int64_t passed_r = now - _last_received;
    int64_t idle = passed_r < passed_w ? passed_r : passed_w;

    // 连接空闲的时长
    if (_idle_timeout && idle >= _idle_timeout) {
        CLOG.Verbose("Linkage: connection idle out for fd = %d", _peer->fd());
        return false;
    }

    return _handler->Cleanup(this, now);
}

bool Linkage::Attach(LinkageWorker *worker)
{
    if (!worker) {
        return false;
    } else if (_worker) {
        return _worker == worker;
    }

    bool wanna_read = false;
    bool wanna_write = false;
    AbstractIo::Action action;
    AbstractIo::Action next_action;
    if (!_io->Initialize(&action, &next_action, &wanna_read, &wanna_write)) {
        return false;
    }

    assert(AbstractIo::kActionNone == action     ||
           AbstractIo::kActionNone == next_action);

    bool connecting = (AbstractIo::kActionNone != action)    ||
                      (AbstractIo::kActionNone != next_action);

    // Enable reading if connection is established.
    if (!connecting) {
        wanna_read = true;
    }

    if (!DoAttach(worker, _peer->fd(), wanna_read, wanna_write, true)) {
        return false;
    }

    _worker = worker;
    if (!connecting) {
        if (!DoConnected()) {
            DoDetach(worker);
            _worker = NULL;
            return false;
        }

        return true;
    }

    UpdateConnectJam(true);
    if (action == AbstractIo::kActionNone) {
        // Wait for events and invoke next_action.
        _action = next_action;
        return true;
    }

    // Invoke action immediately.
    _action = action;
    int ret = OnEvent(worker, action);
    if (ret <= 0) {
        DoDetach(worker);
        _worker = NULL;
        return false;
    }

    return true;
}

bool Linkage::Detach(LinkageWorker *worker)
{
    if (!worker) {
        return false;
    } else if (!_worker || _worker != worker) {
        return true;
    }

    if (!DoDetach(worker)) {
        return false;
    }

    _worker = NULL;
    return true;
}

int Linkage::Disconnect(bool finish_write)
{
    _graceful = true;
    if (!finish_write) {
        DumpSendingBuffer();
    }

    if (_action != AbstractIo::kActionNone) {
        return 1;
    }

    return OnEvent(_worker, AbstractIo::kActionShutdown);
}

int Linkage::Shutdown(AbstractIo::Status *status)
{
    _graceful = true;
    if (_closed) {
        return 0;
    }

    UpdateConnectJam(true);
    *status = _io->Shutdown();
    switch (*status) {
    case AbstractIo::kStatusOk:
    case AbstractIo::kStatusClosed:
        _closed = true;
        return 0;

    case AbstractIo::kStatusWannaRead:
    case AbstractIo::kStatusWannaWrite:
        return 1;

    default:
        return -1;
    };
}

bool Linkage::Send(const void *buffer, size_t length)
{
    if (!buffer || length > INT_MAX || !_worker || _graceful) {
        return false;
    } else if (length == 0) {
        return true;
    }

    CLOG.Verbose("Linkage: sending [%lu] bytes for fd = %d", length, _peer->fd());
    if (!_worker || _action != AbstractIo::kActionNone || GetSendingBufferSize()) {
        if (!AppendSendingBuffer(buffer, length)) {
            return false;
        }

        CLOG.Verbose("Linkage: queued [%lu] bytes for fd = %d", length, _peer->fd());
        return true;
    }

    assert(_peer);
    size_t retlen;
    AbstractIo::Status status = _io->Write(buffer, length, &retlen);
    switch (status) {
    case AbstractIo::kStatusJammed:
        if (!AppendSendingBuffer(buffer, length)) {
            return false;
        }

        CLOG.Verbose("Linkage: queued [%lu] bytes for fd = %d", length, _peer->fd());
        _worker->SetWanna(this, false, true);
        UpdateLastSent(false, true);
        break;

    case AbstractIo::kStatusWannaRead:
        if (!AppendSendingBuffer(buffer, length)) {
            return false;
        }

        CLOG.Verbose("Linkage: queued [%lu] bytes for fd = %d", length, _peer->fd());
        _action = AbstractIo::kActionWrite;
        _worker->SetWanna(this, true, false);
        UpdateLastSent(false, true);
        _last_writing = length;
        break;

    case AbstractIo::kStatusWannaWrite:
        if (!AppendSendingBuffer(buffer, length)) {
            return false;
        }

        CLOG.Verbose("Linkage: queued [%lu] bytes for fd = %d", length, _peer->fd());
        _action = AbstractIo::kActionWrite;
        _worker->SetWanna(this, false, true);
        UpdateLastSent(false, true);
        _last_writing = length;
        break;

    case AbstractIo::kStatusOk:
        if (retlen < length) {
            const unsigned char *buf = reinterpret_cast<const unsigned char *>(buffer);
            if (!AppendSendingBuffer(buf + retlen, length - retlen)) {
                return false;
            }

            CLOG.Verbose("Linkage: sent [%lu] bytes, queued [%lu] bytes for fd = %d",
                         retlen, length - retlen, _peer->fd());

            _worker->SetWannaWrite(this, true);
            UpdateLastSent(true, true);

        } else {
            CLOG.Verbose("Linkage: sent [%lu] bytes for fd = %d", length, _peer->fd());
            UpdateLastSent(true, false);
        }
        break;

    default:
        return false;
    };

    return true;
}

} // namespace flinter
