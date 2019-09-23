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

#ifndef  FLINTER_LINKAGE_LINKAGE_PEER_H
#define  FLINTER_LINKAGE_LINKAGE_PEER_H

#include <sys/types.h>
#include <stdint.h>

#include <string>
#include <vector>

#if defined(__unix__) || defined(__MACH__)
struct sockaddr_in6;
struct sockaddr_un;
typedef int HANDLE;
#endif

struct sockaddr_in;
struct sockaddr;

namespace flinter {

// 一个ip地址类，相当于muduo中的InetAddress
// 但是这个不同的是，这个还包含fd，表示已连接的地址
class LinkagePeer {
public:
    LinkagePeer();

#if defined(__unix__) || defined(__MACH__)
    bool Set(const struct sockaddr_in6 *ip, HANDLE fd);
    bool Set(const struct sockaddr_un  *ip, HANDLE fd);
#endif
    bool Set(const struct sockaddr_in  *ip, HANDLE fd);
    bool Set(const struct sockaddr     *ip, HANDLE fd);

    const std::vector<int> &ip() const
    {
        return _ip;
    }

    /// Print IP as string.
    const std::string &ip_str() const
    {
        return _ip_str;
    }

    uint16_t port() const
    {
        return _port;
    }

    const HANDLE &fd() const
    {
        return _fd;
    }

    const uid_t &uid() const
    {
        return _uid;
    }

    const gid_t &gid() const
    {
        return _gid;
    }

    const pid_t &pid() const
    {
        return _pid;
    }

private:
    std::vector<int> _ip;
    std::string _ip_str;
    uint16_t _port;
    HANDLE _fd;

    uid_t   _uid;
    gid_t   _gid;
    pid_t   _pid;

}; // class LinkagePeer

} // namespace flinter

#endif //FLINTER_LINKAGE_LINKAGE_PEER_H
