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

#include "flinter/linkage/linkage_handler.h"

#include <assert.h>
#include <string.h>

#include "flinter/linkage/linkage.h"
#include "flinter/linkage/linkage_peer.h"
#include "flinter/logger.h"

namespace flinter {

void LinkageHandler::OnError(Linkage *linkage,
                             bool reading_or_writing,
                             int errnum)
{
    assert(linkage);
    CLOG.Trace("Linkage: ERROR %d when %s for fd = %d: %s",
               errnum, reading_or_writing ? "reading" : "writing",
               linkage->peer()->fd(), strerror(errnum));
}

void LinkageHandler::OnDisconnected(Linkage *linkage)
{
    assert(linkage);
    CLOG.Trace("Linkage: DISCONNECT for fd = %d", linkage->peer()->fd());
}

bool LinkageHandler::OnConnected(Linkage *linkage)
{
    assert(linkage);
    const LinkagePeer &peer = *linkage->peer();
    CLOG.Trace("Linkage: CONNECT %s:%u for fd = %d",
               peer.ip_str().c_str(), peer.port(), peer.fd());

    return true;
}

bool LinkageHandler::Cleanup(Linkage *linkage, int64_t now)
{
    (void)linkage;
    (void)now;

    return true;
}

} // namespace flinter
