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

#ifndef FLINTER_LINKAGE_LINKAGE_BASE_H
#define FLINTER_LINKAGE_LINKAGE_BASE_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace flinter {

class LinkagePeer;
class LinkageWorker;

// 一个纯虚函数
class LinkageBase {
public:
    friend class LinkageWorker;

    virtual ~LinkageBase() {}

    virtual int OnReceived(const void *buffer, size_t length) = 0;
    virtual void OnError(bool reading_or_writing, int errnum) = 0;
    virtual void OnDisconnected() = 0;
    virtual bool OnConnected() = 0;

    virtual int Disconnect(bool finish_write = true) = 0;
    virtual bool Attach(LinkageWorker *worker) = 0;
    virtual bool Detach(LinkageWorker *worker) = 0;
    virtual bool Cleanup(int64_t now) = 0;

protected:
    /// Return the packet size even if it's incomplete as long as you can tell.
    /// Very handy when you write the message length in the header.
    ///
    /// @return >0 message length.
    /// @return  0 message length is yet determined, keep receiving.
    /// @return <0 message is invalid.
    virtual ssize_t GetMessageLength(const void *buffer, size_t length) = 0;

    /// @return >0 keep coming.
    /// @return  0 hang up gracefully.
    /// @return <0 error occurred, hang up immediately.
    virtual int OnMessage(const void *buffer, size_t length) = 0;

    /// @return >0 don't change event monitoring status.
    /// @return  0 no more data to read, hang up gracefully.
    /// @return <0 error occurred, drop connection immediately.
    virtual int OnReadable(LinkageWorker *worker) = 0;

    /// @return >0 don't change event monitoring status.
    /// @return  0 no more data to write, hang up gracefully.
    /// @return <0 error occurred, drop connection immediately.
    virtual int OnWritable(LinkageWorker *worker) = 0;

    bool DoDetach(LinkageWorker *worker);
    bool DoAttach(LinkageWorker *worker, int fd,
                  bool read_now, bool write_now,
                  bool auto_release);

}; // class LinkageBase

} // namespace flinter

#endif // FLINTER_LINKAGE_LINKAGE_BASE_H
