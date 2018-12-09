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

#include "flinter/linkage/linkage_peer.h"

#if defined(__unix__)
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <sys/un.h>
#else
# include <WinSock2.h>
typedef uint32_t in_addr_t;
#endif

#include <assert.h>

#if defined(LOCAL_PEERCRED)
# include <sys/ucred.h>
#endif

namespace flinter {

LinkagePeer::LinkagePeer() : _port(0)
#if defined(__unix__)
                           , _fd(-1)
#else
                           , _fd(NULL)
#endif
                           , _uid(0)
                           , _gid(0)
                           , _pid(0)
{
    // Intended left blank.
}

#if defined(__unix__)
bool LinkagePeer::Set(const struct sockaddr_un *ip, HANDLE fd)
{
    assert(ip);
    assert(ip->sun_family == AF_UNIX);
    if (!ip || ip->sun_family != AF_UNIX) {
        return false;
    }

#undef CREDENTIALS

    do {
#if defined(SO_PEERCRED)
# define CREDENTIALS
        {
        struct ucred u;
        socklen_t ul = sizeof(u);
        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &u, &ul) == 0) {
            _uid = u.uid;
            _gid = u.gid;
            _pid = u.pid;
            break;
        }
        }
#endif

#if HAVE_GETPEEREID
# define CREDENTIALS
        {
        uid_t uid;
        gid_t gid;
        if (getpeereid(fd, &uid, &gid) == 0) {
            _uid = uid;
            _gid = gid;
            _pid = 0;
            break;
        }
        }
#endif

#if defined(LOCAL_PEERCRED) && defined(XUCRED_VERSION)
# define CREDENTIALS
        {
        struct xucred u;
        socklen_t ul = sizeof(u);
        if (getsockopt(fd, 0, LOCAL_PEERCRED, &u, &ul) == 0) {
            if (u.cr_version == XUCRED_VERSION) {
                _uid = u.cr_uid;
                _gid = u.cr_gid;
                _pid = 0;
                break;
            }
        }
        }
#endif

#ifndef CREDENTIALS
# error Getting peer credentials is not supported.
#endif

        return false;

    } while (false);

    if (ip->sun_path[0]) {
        _ip_str.assign("file://");
        _ip_str.append(ip->sun_path,
                       strnlen(ip->sun_path, sizeof(ip->sun_path)));

    } else {
        _ip_str.assign("unix://");
        _ip_str.append(ip->sun_path + 1,
                       strnlen(ip->sun_path + 1, sizeof(ip->sun_path) - 1));
    }

    _ip.clear();
    _port = 0;
    _fd = fd;

    return true;
}

bool LinkagePeer::Set(const struct sockaddr_in6 *ip, HANDLE fd)
{
    assert(ip);
    assert(ip->sin6_family == AF_INET6);
    if (!ip || ip->sin6_family != AF_INET6) {
        return false;
    }

    _ip.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        _ip[i] = ip->sin6_addr.s6_addr[i];
    }

    char buffer[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ip->sin6_addr, buffer, sizeof(buffer));
    _ip_str = buffer;

    _port = ntohs(ip->sin6_port);
    _fd = fd;
    _uid = 0;
    _gid = 0;
    _pid = 0;
    return true;
}
#endif // defined(__unix__)

bool LinkagePeer::Set(const struct sockaddr_in *ip, HANDLE fd)
{
    assert(ip);
    assert(ip->sin_family == AF_INET);
    if (!ip || ip->sin_family != AF_INET) {
        return false;
    }

    _ip.resize(4);
    in_addr_t p = ip->sin_addr.s_addr;
    for (size_t i = 0; i < 4; ++i) {
        _ip[i] = static_cast<unsigned char>(p);
        p >>= 8;
    }

#if defined(__unix__)
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip->sin_addr, buffer, sizeof(buffer));
    _ip_str = buffer;
#else
    _ip_str = inet_ntoa(ip->sin_addr); // Thread-safe on Windows.
#endif

    _port = ntohs(ip->sin_port);
    _fd = fd;
    _uid = 0;
    _gid = 0;
    _pid = 0;
    return true;
}

bool LinkagePeer::Set(const struct sockaddr *ip, HANDLE fd)
{
    assert(ip);
    if (!ip) {
        return false;
    }

    switch (ip->sa_family) {
#if defined(__unix__)
    case AF_INET6:
        return Set(reinterpret_cast<const struct sockaddr_in6 *>(ip), fd);

    case AF_UNIX:
        return Set(reinterpret_cast<const struct sockaddr_un *>(ip), fd);
#endif

    case AF_INET:
        return Set(reinterpret_cast<const struct sockaddr_in *>(ip), fd);

    default:
        return false;

    };
}

} // namespace flinter
