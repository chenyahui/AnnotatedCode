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

#include "flinter/linkage/easy_handler.h"

#include "flinter/linkage/easy_context.h"
#include "flinter/linkage/linkage_peer.h"
#include "flinter/logger.h"

namespace flinter {

int EasyHandler::HashMessage(const EasyContext & /*context*/,
                             const void * /*buffer*/,
                             size_t /*length*/)
{
    return -1;
}

void EasyHandler::OnError(const EasyContext &context,
                          bool reading_or_writing,
                          int errnum)
{
    CLOG.Trace("Linkage: ERROR %d when %s for channel = %llu [%s:%u]",
               errnum, reading_or_writing ? "reading" : "writing",
               static_cast<unsigned long long>(context.channel()),
               context.peer().ip_str().c_str(),
               context.peer().port());
}

void EasyHandler::OnDisconnected(const EasyContext &context)
{
    CLOG.Trace("Linkage: DISCONNECTED channel = %llu [%s:%u]",
               static_cast<unsigned long long>(context.channel()),
               context.peer().ip_str().c_str(),
               context.peer().port());
}

bool EasyHandler::OnConnected(const EasyContext &context)
{
    CLOG.Trace("Linkage: CONNECTED channel = %llu [%s:%u]",
               static_cast<unsigned long long>(context.channel()),
               context.peer().ip_str().c_str(),
               context.peer().port());

    return true;
}

bool EasyHandler::Cleanup(const EasyContext &context, int64_t now)
{
    (void)context;
    (void)now;

    return true;
}

} // namespace flinter
