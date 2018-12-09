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

#include "flinter/linkage/easy_context.h"

#include "flinter/linkage/easy_handler.h"
#include "flinter/linkage/linkage.h"
#include "flinter/linkage/ssl_peer.h"

namespace flinter {

EasyContext::EasyContext(EasyServer *easy_server,
                         EasyHandler *easy_handler,
                         bool auto_release_handler,
                         uint64_t channel,
                         const LinkagePeer &peer,
                         const LinkagePeer &me,
                         int io_thread_id)
        : _easy_server(easy_server)
        , _easy_handler(easy_handler)
        , _auto_release_handler(auto_release_handler)
        , _channel(channel)
        , _peer(peer)
        , _me(me)
        , _ssl_peer(NULL)
        , _io_thread_id(io_thread_id)
{
    // Intended left blank.
}

EasyContext::~EasyContext()
{
    if (_auto_release_handler) {
        delete _easy_handler;
    }

    delete _ssl_peer;
}

void EasyContext::set_ssl_peer(const SslPeer *ssl_peer)
{
    if (_ssl_peer) {
        delete _ssl_peer;
        _ssl_peer = NULL;
    }

    if (ssl_peer) {
        _ssl_peer = new SslPeer(*ssl_peer);
    }
}

} // namespace flinter
