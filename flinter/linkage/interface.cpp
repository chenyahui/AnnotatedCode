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

#if defined(__unix__)

#include "flinter/linkage/interface.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iomanip>
#include <sstream>

#include "flinter/linkage/linkage_peer.h"
#include "flinter/linkage/resolver.h"
#include "flinter/logger.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

namespace flinter {

template <class T>
static bool GetPeerOrClose(int s, LinkagePeer *peer, LinkagePeer *me)
{
    if (peer) {
        T sa;
        socklen_t len = sizeof(sa);
        if (getpeername(s, reinterpret_cast<struct sockaddr *>(&sa), &len) ||
            !peer->Set(&sa, s)) {

            safe_close(s);
            return false;
        }
    }

    if (me) {
        T sa;
        socklen_t len = sizeof(sa);
        if (getsockname(s, reinterpret_cast<struct sockaddr *>(&sa), &len) ||
            !me->Set(&sa, s)) {

            safe_close(s);
            return false;
        }
    }

    return true;
}

template <class T>
int DoAccept(int s, LinkagePeer *peer, LinkagePeer *me)
{
    T addr;
    struct sockaddr *sa = reinterpret_cast<struct sockaddr *>(&addr);
    socklen_t len = sizeof(addr);
    int fd = safe_accept(s, sa, &len);
    if (fd < 0) {
        return -1;
    }

    if (!GetPeerOrClose<T>(fd, peer, me)) {
        return 1;
    }

    return 0;
}

Interface::Socket::Socket()
        : domain(AF_UNSPEC)
        , type(SOCK_STREAM)
        , protocol(0)
        , socket_interface(NULL)
        , socket_hostname(NULL)
        , socket_bind_port(0)
        , socket_port(0)
        , unix_abstract(NULL)
        , unix_pathname(NULL)
        , unix_mode(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
{
    // Intended left blank.
}

void Interface::Socket::ToString(std::string *str) const
{
    assert(str);
    std::ostringstream s;
    s << *this;
    str->assign(s.str());
}

Interface::Option::Option()
        : tcp_defer_accept(false)
        , tcp_nodelay(false)
        , udp_broadcast(false)
        , udp_multicast(false)
        , socket_close_on_exec(false)
        , socket_reuse_address(false)
        , socket_non_blocking(false)
        , socket_keepalive(false)
{
    // Intended left blank.
}

void Interface::Option::ToString(std::string *str) const
{
    assert(str);
    std::ostringstream s;
    s << *this;
    str->assign(s.str());
}

Interface::Interface() : _socket(-1), _domain(AF_UNSPEC)
{
    // Intended left blank.
}

Interface::~Interface()
{
    Close();
}

int Interface::Bind(int s, void *sockaddr, size_t addrlen)
{
    return bind(s, reinterpret_cast<struct sockaddr *>(sockaddr),
                   static_cast<socklen_t>(addrlen));
}

int Interface::CreateListenSocket(const Socket &socket,
                                  const Option &option,
                                  void *sockaddr,
                                  size_t addrlen)
{
    int s = CreateSocket(socket, option);
    if (s < 0) {
        return -1;
    }

    if (Bind(s, sockaddr, addrlen)) {
        safe_close(s);
        return -1;
    }

    if (socket.type == SOCK_STREAM) {
        if (listen(s, kMaximumQueueLength)) {
            safe_close(s);
            return -1;
        }
    }

    return s;
}

int Interface::CreateSocket(const Socket &socket, const Option &option)
{
    int s = ::socket(socket.domain, socket.type, socket.protocol);
    if (s < 0) {
        return -1;
    }

    if (!InitializeSocket(option, s)) {
        safe_close(s);
        return -1;
    }

    return s;
}

bool Interface::InitializeSocket(const Option &option, int s)
{
    if (option.socket_reuse_address) {
        if (set_socket_reuse_address(s)) {
            return false;
        }
    }

    if (option.socket_non_blocking) {
        if (set_non_blocking_mode(s)) {
            return false;
        }
    }

    if (option.socket_close_on_exec) {
        if (set_close_on_exec(s)) {
            return false;
        }
    }

    if (option.socket_keepalive) {
        if (set_socket_keepalive(s)) {
            return false;
        }
    }

    if (option.tcp_defer_accept) {
        if (set_tcp_defer_accept(s)) {
            return false;
        }
    }

    if (option.tcp_nodelay) {
        if (set_tcp_nodelay(s)) {
            return false;
        }
    }

    if (option.udp_broadcast) {
        if (set_socket_broadcast(s)) {
            return false;
        }
    }

    return true;
}

template <class T>
int Interface::DoConnectTcp(const Socket &socket,
                            const Option &option,
                            LinkagePeer *peer,
                            LinkagePeer *me)
{
    if (!socket.socket_hostname  ||
        !*socket.socket_hostname ||
        !socket.socket_port      ){

        errno = EINVAL;
        return -1;
    }

    Resolver *const resolver = Resolver::GetInstance();
    T addr;

    int s = CreateSocket(socket, option);
    if (s < 0) {
        CLOG.Verbose("Interface: failed to initialize socket: %d: %s",
                     errno, strerror(errno));

        return -1;
    }

    if (socket.socket_interface && *socket.socket_interface) {
        if (!resolver->Resolve(socket.socket_interface,
                               socket.socket_bind_port,
                               &addr)) {

            safe_close(s);
            return -1;
        }

        if (Bind(s, &addr, sizeof(addr))) {
            CLOG.Verbose("Interface: failed to bind socket(%d): %d: %s",
                         s, errno, strerror(errno));

            safe_close(s);
            return -1;
        }
    }

    if (!resolver->Resolve(socket.socket_hostname,
                           socket.socket_port,
                           &addr)) {

        safe_close(s);
        return -1;
    }

    int ret = safe_connect(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            CLOG.Verbose("Interface: failed to connect socket(%d): %d: %s",
                         s, errno, strerror(errno));

            safe_close(s);
            return -1;
        }
    }

    if (!GetPeerOrClose<T>(s, NULL, me)) {
        return -1;
    }

    if (peer) {
        if (!peer->Set(&addr, s)) {
            safe_close(s);
            return -1;
        }
    }

    _socket = s;
    _domain = AF_INET;
    return ret == 0 ? 0 : 1;
}

int Interface::DoConnectUnix(const Socket &socket,
                             const Option &option,
                             LinkagePeer *peer,
                             LinkagePeer * /*me*/)
{
    bool file_based = true;
    const char *name = NULL;
    if (socket.unix_pathname && *socket.unix_pathname) {
        name = socket.unix_pathname;
    } else if (socket.unix_abstract && *socket.unix_abstract) {
        name = socket.unix_abstract;
        file_based = false;
    }

    struct sockaddr_un aun;
    memset(&aun, 0, sizeof(aun));
    aun.sun_family = AF_UNIX;
    size_t len = strlen(name);
    if (len >= sizeof(aun.sun_path)) {
        errno = EINVAL;
        return -1;
    }

    if (file_based) {
        memcpy(aun.sun_path, name, len);
    } else {
        memcpy(aun.sun_path + 1, name, len);
    }

    len += sizeof(aun) - sizeof(aun.sun_path) + 1;
    int s = CreateSocket(socket, option);
    if (s < 0) {
        return -1;
    }

    // UNIX socket will never be in progress.
    if (safe_connect(s, reinterpret_cast<struct sockaddr *>(&aun),
                        static_cast<socklen_t>(len))) {

        safe_close(s);
        return -1;
    }

    if (peer) {
        if (!peer->Set(&aun, s)) {
            return -1;
        }
    }

    _socket = s;
    _domain = AF_UNIX;
    return 0;
}

int Interface::Connect(const Socket &socket,
                       const Option &option,
                       LinkagePeer *peer,
                       LinkagePeer *me)
{
    // For connecting, SOCK_STREAM and SOCK_DGRAM share the same actions.
    if (socket.type != SOCK_STREAM && socket.type != SOCK_DGRAM) {
        errno = EINVAL;
        return -1;

    } else if (!Close()) {
        return -1;

    } else if (socket.domain == AF_INET6) { // UDP as well.
        return DoConnectTcp<struct sockaddr_in6>(socket, option, peer, me);

    } else if (socket.domain == AF_INET ) { // UDP as well.
        return DoConnectTcp<struct sockaddr_in>(socket, option, peer, me);

    } else if (socket.domain == AF_UNIX ) {
        return DoConnectUnix(socket, option, peer, me);
    }

    CLOG.Fatal("Interface: BUG: unsupported socket type: <%d:%d:%d>",
               socket.domain, socket.type, socket.protocol);

    return -1;
}

bool Interface::DoListenTcp4(const Socket &socket,
                             const Option &option,
                             LinkagePeer *me)
{
    struct ip_mreqn mreqn;
    struct sockaddr_in ain;
    bool multicast = false;

    Resolver *const resolver = Resolver::GetInstance();
    if (socket.type == SOCK_DGRAM && option.udp_multicast) {
        memset(&mreqn, 0, sizeof(mreqn));
        if (!socket.socket_interface                                     ||
            !*socket.socket_interface                                    ||
            inet_aton(socket.socket_interface, &mreqn.imr_address) != 1  ){

            errno = EINVAL;
            return false;
        }

        if (!resolver->Resolve(socket.socket_hostname,
                               socket.socket_bind_port,
                               &ain, Resolver::kFirst)) {

            return false;
        }

        mreqn.imr_multiaddr = ain.sin_addr;
        multicast = true;

    } else {
        const char *hostname = "0.0.0.0";
        if (socket.socket_interface && *socket.socket_interface) {
            hostname = socket.socket_interface;
        }

        if (!resolver->Resolve(hostname, socket.socket_bind_port, &ain, Resolver::kFirst)) {
            return false;
        }
    }

    int s = CreateListenSocket(socket, option, &ain, sizeof(ain));
    if (s < 0) {
        return false;
    }

    if (multicast) {
        if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn))) {
            safe_close(s);
            return false;
        }
    }

    if (!GetPeerOrClose<struct sockaddr_in>(s, NULL, me)) {
        return false;
    }

    _socket = s;
    _domain = AF_INET;

    char buffer[128];
    CLOG.Verbose("Interface: LISTEN<%d> %s4 <%s:%u>", s,
                 socket.type == SOCK_STREAM ? "TCP" : "UDP",
                 inet_ntop(AF_INET, &ain.sin_addr, buffer, sizeof(buffer)),
                 socket.socket_bind_port);

    return true;
}

bool Interface::DoListenTcp6(const Socket &socket,
                             const Option &option,
                             LinkagePeer *me)
{
    struct sockaddr_in6 ai6;
    const char *hostname = "::";
    if (socket.socket_interface && *socket.socket_interface) {
        hostname = socket.socket_interface;
    }

    Resolver *const resolver = Resolver::GetInstance();
    if (!resolver->Resolve(hostname,
                           socket.socket_bind_port,
                           &ai6, Resolver::kFirst)) {

        return false;
    }

    int s = CreateListenSocket(socket, option, &ai6, sizeof(ai6));
    if (s < 0) {
        return false;
    }

    if (!GetPeerOrClose<struct sockaddr_in6>(s, NULL, me)) {
        return false;
    }

    _socket = s;
    _domain = AF_INET6;

    char buffer[128];
    CLOG.Verbose("Interface: LISTEN<%d> %s6 <[%s]:%u>", s,
                 socket.type == SOCK_STREAM ? "TCP" : "UDP",
                 inet_ntop(AF_INET6, &ai6.sin6_addr, buffer, sizeof(buffer)),
                 socket.socket_bind_port);

    return true;
}

bool Interface::DoListenUnixSocket(const Socket &socket,
                                   const Option &option,
                                   LinkagePeer * /*me*/)
{
    struct sockaddr_un aun;
    memset(&aun, 0, sizeof(aun));
    aun.sun_family = AF_UNIX;

    size_t len = strlen(socket.unix_pathname);
    if (len >= sizeof(aun.sun_path)) {
        errno = EINVAL;
        return false;
    }

    memcpy(aun.sun_path, socket.unix_pathname, len);
    len += sizeof(aun) - sizeof(aun.sun_path) + 1;

    int s = CreateSocket(socket, option);
    if (s < 0) {
        return false;
    }

    if (Bind(s, &aun, len)) {
        safe_close(s);
        return false;
    }

    if (chmod(socket.unix_pathname, socket.unix_mode)) {
        safe_close(s);
        unlink(socket.unix_pathname);
        return false;
    }

    if (listen(s, kMaximumQueueLength)) {
        safe_close(s);
        unlink(socket.unix_pathname);
        return false;
    }

    _socket = s;
    _domain = AF_UNIX;
    _sockname = socket.unix_pathname;
    CLOG.Verbose("Interface: LISTEN<%d> PATHNAME%c [%s]", s,
                 socket.type == SOCK_STREAM ? 's' : 'd',
                 socket.unix_pathname);

    return true;
}

bool Interface::DoListenUnixNamespace(const Socket &socket,
                                      const Option &option,
                                      LinkagePeer * /*me*/)
{
    struct sockaddr_un aun;
    memset(&aun, 0, sizeof(aun));
    aun.sun_family = AF_UNIX;

    size_t len = strlen(socket.unix_abstract);
    if (len >= sizeof(aun.sun_path)) {
        errno = EINVAL;
        return false;
    }

    memcpy(aun.sun_path + 1, socket.unix_pathname, len);
    len += sizeof(aun) - sizeof(aun.sun_path) + 1;

    int s = CreateListenSocket(socket, option, &aun, len);
    if (s < 0) {
        return false;
    }

    _socket = s;
    _domain = AF_UNIX;
    CLOG.Verbose("Interface: LISTEN<%d> ABSTRACT%c [%s]", s,
                 socket.type == SOCK_STREAM ? 's' : 'd',
                 socket.unix_abstract);

    return true;
}

bool Interface::DoListenUnix(const Socket &socket,
                             const Option &option,
                             LinkagePeer *me)
{
    if (socket.unix_pathname && *socket.unix_pathname) {
        return DoListenUnixSocket(socket, option, me);

    } else if (socket.unix_abstract && *socket.unix_abstract) {
        return DoListenUnixNamespace(socket, option, me);
    }

    CLOG.Fatal("Interface: BUG: neither pathname nor abstract namespace "
                               "specified for UNIX sockets.");

    errno = EINVAL;
    return false;
}

bool Interface::Listen(const Socket &socket,
                       const Option &option,
                       LinkagePeer *me)
{
    if (socket.type != SOCK_STREAM && socket.type != SOCK_DGRAM) {
        errno = EINVAL;
        return false;

    } else if (!Close()) {
        return false;

    } else if (socket.domain == AF_INET6) {
        return DoListenTcp6(socket, option, me); // UDP as well.

    } else if (socket.domain == AF_INET ) {
        return DoListenTcp4(socket, option, me); // UDP as well.

    } else if (socket.domain == AF_UNIX ) {
        return DoListenUnix(socket, option, me);
    }

    CLOG.Fatal("Interface: BUG: unsupported socket type: <%d:%d:%d>",
               socket.domain, socket.type, socket.protocol);

    errno = EINVAL;
    return false;
}

bool Interface::ListenTcp6(uint16_t port, bool loopback)
{
    Socket s;
    s.domain = AF_INET6;
    s.type = SOCK_STREAM;
    s.socket_bind_port = port;
    s.socket_interface = loopback ? "::1" : "::";

    Option o;
    o.socket_non_blocking = true;
    o.socket_reuse_address = true;
    o.socket_close_on_exec = true;
    return Listen(s, o);
}

bool Interface::ListenTcp4(uint16_t port, bool loopback)
{
    Socket s;
    s.domain = AF_INET;
    s.type = SOCK_STREAM;
    s.socket_bind_port = port;
    s.socket_interface = loopback ? "127.0.0.1" : "0.0.0.0";

    Option o;
    o.socket_non_blocking = true;
    o.socket_reuse_address = true;
    o.socket_close_on_exec = true;
    return Listen(s, o);
}

bool Interface::ListenTcp(uint16_t port, bool loopback)
{
    return ListenTcp6(port, loopback) || ListenTcp4(port, loopback);
}

bool Interface::ListenUnix(const std::string &sockname, bool file_based, bool privileged)
{
    static const mode_t kPriviledged = S_IRUSR | S_IWUSR;
    static const mode_t kNormal = S_IRUSR | S_IWUSR
                                | S_IRGRP | S_IWGRP
                                | S_IROTH | S_IWOTH;

    Socket s;
    s.domain = AF_UNIX;
    s.type = SOCK_STREAM;

    Option o;
    o.socket_non_blocking = true;
    o.socket_close_on_exec = true;

    if (file_based) {
        s.unix_pathname = sockname.c_str();
        s.unix_mode = privileged ? kPriviledged : kNormal;

    } else {
        s.unix_abstract = sockname.c_str();
    }

    return Listen(s, o);
}

bool Interface::Shutdown()
{
    if (_socket < 0) {
        return true;
    }

    // There're race conditions since socket handling is in kernel space,
    // if peer hungup is done by kernel, even if user space haven't call
    // shutdown() before, it will fail with ENOTCONN, so it's normal.
    if (shutdown(_socket, SHUT_RDWR)) {
        if (errno != ENOTCONN) {
            CLOG.Warn("Interface: failed to shutdown(%d): %d: %s",
                      _socket, errno, strerror(errno));

            return false;
        }
    }

    return true;
}

bool Interface::Close()
{
    if (_socket < 0) {
        return true;
    }

    if (!_sockname.empty()) {
        // Try but not necessarily succeed.
        unlink(_sockname.c_str());
        _sockname.clear();
    }

    int socket = _socket;
    _socket = -1;

    if (safe_close(socket)) {
        return false;
    }

    _domain = AF_UNSPEC;
    return true;
}

int Interface::Accept(LinkagePeer *peer, LinkagePeer *me)
{
    Option o;
    o.socket_close_on_exec = true;
    o.socket_non_blocking = true;
    return Accept(o, peer, me);
}

int Interface::Accept(const Option &option,
                      LinkagePeer *peer,
                      LinkagePeer *me)
{
    assert(_socket >= 0);
    assert(peer);

    int ret = -1;
    if (_socket < 0) {
        return -1;

    } else if (_domain == AF_INET6) {
        ret = DoAccept<struct sockaddr_in6>(_socket, peer, me);

    } else if (_domain == AF_INET) {
        ret = DoAccept<struct sockaddr_in >(_socket, peer, me);

    } else if (_domain == AF_UNIX) {
        ret = DoAccept<struct sockaddr_un >(_socket, peer, me);

    } else {
        errno = ESOCKTNOSUPPORT;
    }

    if (ret) {
        return ret;
    }

    if (!InitializeSocket(option, peer->fd())) {
        safe_close(peer->fd());
        return 1;
    }

    return 0;
}

bool Interface::Accepted(int fd)
{
    Option o;
    o.socket_non_blocking = true;
    o.socket_close_on_exec = true;
    return Accepted(o, fd);
}

bool Interface::Accepted(const Option &option, int fd)
{
    if (!Close() || !InitializeSocket(option, fd)) {
        return false;
    }

    _domain = AF_UNSPEC;
    _socket = fd;
    return true;
}

int Interface::ConnectUnix(const std::string &sockname,
                           bool file_based,
                           LinkagePeer *peer,
                           LinkagePeer *me)
{
    Socket s;
    s.domain = AF_UNIX;
    s.type = SOCK_STREAM;

    Option o;
    o.socket_non_blocking = true;
    o.socket_close_on_exec = true;

    if (file_based) {
        s.unix_pathname = sockname.c_str();
    } else {
        s.unix_abstract = sockname.c_str();
    }

    return Connect(s, o, peer, me);
}

int Interface::ConnectTcp4(const std::string &hostname,
                           uint16_t port,
                           LinkagePeer *peer,
                           LinkagePeer *me)
{
    Socket s;
    s.domain = AF_INET;
    s.type = SOCK_STREAM;
    s.socket_port = port;
    s.socket_hostname = hostname.c_str();

    Option o;
    o.socket_non_blocking = true;
    o.socket_close_on_exec = true;

    return Connect(s, o, peer, me);
}

// 一直等到连接成功
bool Interface::WaitUntilConnected(int64_t timeout)
{
    if (_socket < 0) {
        return false;
    }

    int milliseconds = -1;
    if (timeout >= 0) {
        milliseconds = static_cast<int>(timeout / 1000000LL);
    }

    int ret = safe_wait_until_connected(_socket, milliseconds);
    if (ret) {
        CLOG.Verbose("Interface: failed to wait until connected(%d): %d: %s",
                     _socket, errno, strerror(errno));

        return false;
    }

    return true;
}

bool Interface::TestIfConnected()
{
    if (_socket < 0) {
        errno = EBADF;
        return false;
    }

    int ret = safe_test_if_connected(_socket);
    if (ret) {
        CLOG.Verbose("Interface: failed to test if connected(%d): %d: %s",
                     _socket, errno, strerror(errno));

        return false;
    }

    return true;
}

} // namespace flinter

std::ostream &operator << (std::ostream &s, const flinter::Interface::Socket &d)
{
    if (d.domain == AF_UNIX) {
        if (d.unix_pathname && *d.unix_pathname) {
            s << "unix://" << d.unix_pathname << ","
              << std::oct << std::setfill('0') << std::setw(4) << d.unix_mode;
        } else if (d.unix_abstract && *d.unix_abstract) {
            s << "unix://[" << d.unix_abstract << "]";
        } else {
            s << "unix://<invalid>";
        }
    } else if (d.domain == AF_INET6 && d.type == SOCK_STREAM) {
        s << "tcp6://["
          << (d.socket_hostname && *d.socket_hostname ? d.socket_hostname : "::")
          << "]:" << (d.socket_bind_port ? d.socket_bind_port : d.socket_port);
    } else if (d.domain == AF_INET6 && d.type == SOCK_DGRAM) {
        s << "udp6://["
          << (d.socket_hostname && *d.socket_hostname ? d.socket_hostname : "::")
          << "]:" << (d.socket_bind_port ? d.socket_bind_port : d.socket_port);
    } else if (d.domain == AF_INET && d.type == SOCK_STREAM) {
        s << "tcp4://"
          << (d.socket_hostname && *d.socket_hostname ? d.socket_hostname : "0.0.0.0")
          << ":" << (d.socket_bind_port ? d.socket_bind_port : d.socket_port);
    } else if (d.domain == AF_INET && d.type == SOCK_DGRAM) {
        s << "udp4://"
          << (d.socket_hostname && *d.socket_hostname ? d.socket_hostname : "0.0.0.0")
          << ":" << (d.socket_bind_port ? d.socket_bind_port : d.socket_port);
    } else {
        s << "unknown://[" << d.domain << "," << d.type << "," << d.protocol << "]";
    }

    return s;
}

std::ostream &operator << (std::ostream &s, const flinter::Interface::Option &d)
{
    const char *delim = "";

    if (d.tcp_defer_accept    ) { s << delim << "Ta"; delim = ","; }
    if (d.tcp_nodelay         ) { s << delim << "Td"; delim = ","; }
    if (d.udp_broadcast       ) { s << delim << "Ub"; delim = ","; }
    if (d.udp_multicast       ) { s << delim << "Um"; delim = ","; }
    if (d.socket_close_on_exec) { s << delim << "Se"; delim = ","; }
    if (d.socket_reuse_address) { s << delim << "Sr"; delim = ","; }
    if (d.socket_non_blocking ) { s << delim << "Sn"; delim = ","; }
    if (d.socket_keepalive    ) { s << delim << "Sk"; delim = ","; }

    return s;
}

#endif // defined(__unix__)
