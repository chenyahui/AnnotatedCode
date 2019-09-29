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

#include "flinter/linkage/listener.h"

#include <errno.h>
#include <string.h>

#include <stdexcept>

#include "flinter/linkage/interface.h"
#include "flinter/linkage/linkage_peer.h"
#include "flinter/linkage/linkage_worker.h"
#include "flinter/logger.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

namespace flinter {

Listener::Listener()
{
    _option.socket_close_on_exec = true;
    _option.socket_non_blocking = true;
}

Listener::Listener(const Interface::Option &option)
{
    _option = option;
}

Listener::~Listener()
{
    // Intended left blank.
}

bool Listener::Cleanup(int64_t /*now*/)
{
    return true;
}

bool Listener::Attach(LinkageWorker *worker)
{
    if (!worker) {
        return false;
    } else if (_workers.find(worker) != _workers.end()) {
        return true;
    }

    if (!DoAttach(worker, _listener.fd(), true, false, false)) {
        return false;
    }

    _workers.insert(worker);
    return true;
}

bool Listener::Detach(LinkageWorker *worker)
{
    if (!worker) {
        return false;
    } else if (_workers.find(worker) == _workers.end()) {
        return true;
    }

    if (!DoDetach(worker)) {
        return false;
    }

    _workers.erase(worker);
    return true;
}

int Listener::OnReadable(LinkageWorker *worker)
{
    LinkagePeer me;
    LinkagePeer peer;
    Interface::Option o;
    // accept这个连接
    int ret = _listener.Accept(o, &peer, &me);

    // 错误处理
    if (ret < 0) {
        if (errno == EINTR          ||
            errno == EAGAIN         ||
            errno == EWOULDBLOCK    ||
            errno == ECONNABORTED   ){

            return 1;

        } else if (errno == ENFILE || errno == EMFILE) {
            // It's not designed to deal with extreme conditions, you should
            // always set ulimit:nofile to a reasonable large value, or limit
            // maximum fd opened in the program logically.
            //
            // Once ENFILE/EMFILE is triggered, the program can either spin or
            // crash, since it's now impossible to accept() new fd and close it
            // afterwards. Here I choose to crash.
            CLOG.Fatal("Listener: failed to accept: %d: %s", errno, strerror(errno));
            throw std::runtime_error("Open file limit exceeded");

        } else if (errno == ENOMEM) {
            // Spin or crash? It's simple, if I don't crash here, you'll crash
            // somewhere else.
            throw std::bad_alloc();
        }

        CLOG.Warn("Listener: failed to accept: %d: %s", errno, strerror(errno));
        throw std::runtime_error("Failed to accept listening fd");

    } else if (ret > 0) {
        CLOG.Warn("Listener: failed to setup new fd: %d: %s", errno, strerror(errno));
        return 1;
    }

    // 创建连接
    LinkageBase *linkage = CreateLinkage(worker, peer, me);

    // 如果创建连接失败了，则直接close这个socket
    if (!linkage) {
        CLOG.Warn("Listener: failed to create client from fd = %d", peer.fd());
        safe_close(peer.fd());
        return 1;
    }

    // 1. 将这个tcp连接和具体worker绑定，这样的话才能加入事件循环
    // 2. 所以可以看出来，在哪个线程listen的，新的连接也放在哪个io线程
    if (!linkage->Attach(worker)) {
        CLOG.Verbose("Listener: failed to attach client for fd = %d", peer.fd());
        delete linkage;
    }

    return 1;
}

bool Listener::Listen(const Interface::Socket &socket,
                      const Interface::Option &option)
{
    bool ret;
    if (socket.domain == AF_UNSPEC) {
        Interface::Socket s(socket);
        s.domain = AF_INET6;
        ret = _listener.Listen(s, option);
        if (!ret) {
            s.domain = AF_INET;
            ret = _listener.Listen(s, option);
        }
    } else {
        ret = _listener.Listen(socket, option);
    }

    if (!ret) {
        std::string s;
        std::string o;
        socket.ToString(&s);
        option.ToString(&o);
        CLOG.Warn("Listener: failed to listen on %s {%s}: %d: %s",
                  s.c_str(), o.c_str(), errno, strerror(errno));

        return false;
    }

    return true;
}

bool Listener::ListenTcp6(uint16_t port, bool loopback)
{
    if (!_listener.ListenTcp6(port, loopback)) {
        CLOG.Warn("Listener: failed to listen on TCPv6 %s:%u",
                  loopback ? "loopback" : "any", port);

        return false;
    }

    return true;
}

bool Listener::ListenTcp4(uint16_t port, bool loopback)
{
    if (!_listener.ListenTcp4(port, loopback)) {
        CLOG.Warn("Listener: failed to listen on TCPv4 %s:%u",
                  loopback ? "loopback" : "any", port);

        return false;
    }

    return true;
}

bool Listener::ListenTcp(uint16_t port, bool loopback)
{
    if (!_listener.ListenTcp(port, loopback)) {
        CLOG.Warn("Listener: failed to listen on TCP %s:%u",
                  loopback ? "loopback" : "any", port);

        return false;
    }

    return true;
}

bool Listener::ListenUnix(const std::string &sockname,
                          bool file_based, bool privileged)
{
    if (!_listener.ListenUnix(sockname, file_based, privileged)) {
        CLOG.Warn("Listener: failed to listen on %s%s [%s]",
                  privileged ? "privileged " : "",
                  file_based ? "file" : "namespace",
                  sockname.c_str());

        return false;
    }

    return true;
}

int Listener::Shutdown()
{
    if (!_listener.Shutdown()) {
        return -1;
    }

    return 0;
}

ssize_t Listener::GetMessageLength(const void * /*buffer*/, size_t /*length*/)
{
    return -1; // The hell?
}

int Listener::OnMessage(const void * /*buffer*/, size_t /*length*/)
{
    return -1; // The hell?
}

int Listener::OnReceived(const void * /*buffer*/, size_t /*length*/)
{
    return -1; // The hell?
}

void Listener::OnError(bool /*reading_or_writing*/, int /*errnum*/)
{
    // Intended left blank.
}

void Listener::OnDisconnected()
{
    // Intended left blank.
}

bool Listener::OnConnected()
{
    return true;
}

int Listener::OnWritable(LinkageWorker * /*worker*/)
{
    return -1; // The hell?
}

int Listener::Disconnect(bool /*finish_write*/)
{
    for (std::set<LinkageWorker *>::const_iterator p = _workers.begin();
         p != _workers.end(); ++p) {

        LinkageWorker *worker = *p;
        worker->SetWannaRead(this, false);
    }

    if (!_listener.Shutdown()) {
        return -1;
    }

    return 0;
}

} // namespace flinter
