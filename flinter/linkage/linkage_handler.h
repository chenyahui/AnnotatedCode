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

#ifndef FLINTER_LINKAGE_LINKAGE_HANDLER_H
#define FLINTER_LINKAGE_LINKAGE_HANDLER_H

#include <sys/types.h>
#include <stdint.h>

namespace flinter {

class Linkage;

class LinkageHandler {
public:
    virtual ~LinkageHandler() {}

    /// Return the packet size even if it's incomplete as long as you can tell.
    /// Very handy when you write the message length in the header.
    ///
    /// @return >0 message length.
    /// @return  0 message length is yet determined, keep receiving.
    /// @return <0 message is invalid.
    virtual ssize_t GetMessageLength(Linkage *linkage,
                                     const void *buffer,
                                     size_t length) = 0;

    /// @return >0 keep coming.
    /// @return  0 hang up gracefully.
    /// @return <0 error occurred, hang up immediately.
    virtual int OnMessage(Linkage *linkage,
                          const void *buffer,
                          size_t length) = 0;

    virtual void OnDisconnected(Linkage *linkage);
    virtual bool OnConnected(Linkage *linkage);
    virtual void OnError(Linkage *linkage,
                         bool reading_or_writing,
                         int errnum);

    virtual bool Cleanup(Linkage *linkage, int64_t now);

}; // class LinkageHandler

} // namespace flinter

#endif // FLINTER_LINKAGE_LINKAGE_HANDLER_H
