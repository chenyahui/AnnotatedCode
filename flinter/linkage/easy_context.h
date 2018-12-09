/* Copyright 2015 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_LINKAGE_EASY_CONTEXT_H
#define FLINTER_LINKAGE_EASY_CONTEXT_H

#include <stdint.h>

#include <flinter/linkage/linkage_peer.h>

namespace flinter {

class EasyHandler;
class EasyServer;
class SslPeer;

class EasyContext {
public:
    typedef uint64_t channel_t;

    EasyContext(EasyServer *easy_server,
                EasyHandler *easy_handler,
                bool auto_release_handler,
                channel_t channel,
                const LinkagePeer &peer,
                const LinkagePeer &me,
                int io_thread_id);

    ~EasyContext();

    EasyServer *easy_server() const
    {
        return _easy_server;
    }

    EasyHandler *easy_handler() const
    {
        return _easy_handler;
    }

    channel_t channel() const
    {
        return _channel;
    }

    const LinkagePeer &me() const
    {
        return _me;
    }

    const LinkagePeer &peer() const
    {
        return _peer;
    }

    const SslPeer *ssl_peer() const
    {
        return _ssl_peer;
    }

    int io_thread_id() const
    {
        return _io_thread_id;
    }

    // Don't call this.
    void set_ssl_peer(const SslPeer *ssl_peer);

private:
    EasyServer *_easy_server;
    EasyHandler *_easy_handler;
    bool _auto_release_handler;
    channel_t _channel;
    const LinkagePeer _peer;
    const LinkagePeer _me;
    const SslPeer *_ssl_peer;
    const int _io_thread_id;

}; // class EasyContext

} // namespace flinter

#endif // FLINTER_LINKAGE_EASY_CONTEXT_H
