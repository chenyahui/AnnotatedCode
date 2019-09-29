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

#ifndef FLINTER_LINKAGE_SSL_IO_H
#define FLINTER_LINKAGE_SSL_IO_H

#include <string>

#include <flinter/linkage/abstract_io.h>

struct ssl_st;

namespace flinter {

class Interface;
class SslContext;
class SslPeer;

class SslIo : public AbstractIo {
public:
    /// @param i life span TAKEN.
    /// @param context life span NOT taken.
    SslIo(Interface *i,
          bool client_or_server,
          bool socket_connecting,
          SslContext *context);

    virtual ~SslIo();

    virtual bool Initialize(Action *action,
                            Action *next_action,
                            bool *wanna_read,
                            bool *wanna_write);

    virtual Status Read(void *buffer, size_t length, size_t *retlen, bool *more);
    virtual Status Write(const void *buffer, size_t length, size_t *retlen);
    virtual Status Shutdown();
    virtual Status Connect();
    virtual Status Accept();

    // Might be NULL.
    const SslPeer *peer() const
    {
        return _peer;
    }

private:
    static const char *GetActionString(const Action &in_progress);
    bool OnHandshaked();

    Status HandleError(const Action &in_progress, int ret);
    Status OnEvent(bool reading_or_writing);
    Status DoShutdown();
    Status DoConnect();

    struct ssl_st *const _ssl;
    const bool _client_mode;
    Interface *const _i;
    bool _connecting;
    SslPeer *_peer;

}; // class SslIo

} // namespace flinter

#endif // FLINTER_LINKAGE_SSL_IO_H
