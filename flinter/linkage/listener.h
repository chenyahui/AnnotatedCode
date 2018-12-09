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

#ifndef FLINTER_LINKAGE_LISTENER_H
#define FLINTER_LINKAGE_LISTENER_H

#include <set>
#include <string>

#include <flinter/linkage/interface.h>
#include <flinter/linkage/linkage_base.h>

namespace flinter {

class LinkagePeer;

/// This class can be attached to multiple LinkageWorkers.
class Listener : public LinkageBase {
public:
    Listener();
    virtual ~Listener();
    explicit Listener(const Interface::Option &acceptedOption);

    bool Listen(const Interface::Socket &socket,
                const Interface::Option &option);

    bool ListenTcp6(uint16_t port, bool loopback);
    bool ListenTcp4(uint16_t port, bool loopback);
    bool ListenTcp (uint16_t port, bool loopback);
    bool ListenUnix(const std::string &sockname,
                    bool file_based = true,
                    bool privileged = false);

    virtual ssize_t GetMessageLength(const void *buffer, size_t length);
    virtual int OnReceived(const void *buffer, size_t length);
    virtual void OnError(bool reading_or_writing, int errnum);
    virtual int OnMessage(const void *buffer, size_t length);
    virtual void OnDisconnected();
    virtual bool OnConnected();

    virtual int Disconnect(bool finish_write = true);
    virtual bool Attach(LinkageWorker *worker);
    virtual bool Detach(LinkageWorker *worker);
    virtual bool Cleanup(int64_t now);

protected:
    virtual LinkageBase *CreateLinkage(LinkageWorker *worker,
                                       const LinkagePeer &peer,
                                       const LinkagePeer &me) = 0;

    virtual int OnReadable(LinkageWorker *worker);
    virtual int OnWritable(LinkageWorker *worker);
    virtual int Shutdown();

private:
    Interface _listener;
    Interface::Option _option;
    std::set<LinkageWorker *> _workers;

}; // class Listener

} // namespace flinter

#endif // FLINTER_LINKAGE_LISTENER_H
