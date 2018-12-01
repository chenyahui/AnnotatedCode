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

#ifndef  FLINTER_LINKAGE_INTERFACE_H
#define  FLINTER_LINKAGE_INTERFACE_H

#include <sys/types.h>
#include <stdint.h>

#include <ostream>
#include <string>

namespace flinter {

class LinkagePeer;

class Interface {
public:
    struct Socket {
        int domain;
        int type;
        int protocol;

        // TCP/UDP
        const char *socket_interface;
        const char *socket_hostname;
        uint16_t socket_bind_port;
        uint16_t socket_port;

        // Unix socket
        const char *unix_abstract;
        const char *unix_pathname;
        mode_t unix_mode;

        Socket();
        void ToString(std::string *str) const;
    }; // struct Socket

    struct Option {
        // TCP only
        bool tcp_defer_accept;
        bool tcp_nodelay;
        bool tcp_cork;

        // UDP only
        bool udp_broadcast;
        bool udp_multicast;

        // Generic
        bool socket_close_on_exec;
        bool socket_reuse_address;
        bool socket_non_blocking;
        bool socket_keepalive;

        Option();
        void ToString(std::string *str) const;
    }; // struct Option

    Interface();
    ~Interface();

    /**
     * Create, bind and listen for a unix domain socket.
     *
     * @param sockname is the name without leading zero.
     * @param file_based sockets will have a file on disk, removed automatically if
     *        possible, unless chroot() is called, permission denied or the program
     *        crashed.
     * @param privileged umask 600 or 0666 for file based, ignored if not file based.
     *
     */
    bool ListenUnix(const std::string &sockname, bool file_based = true, bool privileged = false);

    /// IPv6 any interface can accept IPv4 connections, but loopback interface can't.
    /// @param port port in host order.
    /// @param loopback listen on local loopback interface.
    bool ListenTcp6(uint16_t port, bool loopback);

    /// @param port port in host order.
    /// @param loopback listen on local loopback interface.
    bool ListenTcp4(uint16_t port, bool loopback);

    /// IPv6 any interface can accept IPv4 connections, but loopback interface can't.
    /// @param port port in host order.
    /// @param loopback listen on local loopback interface.
    bool ListenTcp(uint16_t port, bool loopback);

    /// @param peer can NOT be NULL.
    /// @param me can be NULL.
    /// @retval <0 listen fd failure
    /// @retval  0 successful
    /// @retval >0 accepted fd failure
    int Accept(LinkagePeer *peer, LinkagePeer *me = NULL);

    /// If you accept from some other places.
    bool Accepted(int fd);

    /// <0 failed, 0 connected, >0 in progress.
    int ConnectUnix(const std::string &sockname,
                    bool file_based = true,
                    LinkagePeer *peer = NULL,
                    LinkagePeer *me = NULL);

    /// <0 failed, 0 connected, >0 in progress.
    /// @param hostname target hostname.
    /// @param port port in host order.
    /// @param peer save peer information if not NULL.
    int ConnectTcp4(const std::string &hostname,
                    uint16_t port,
                    LinkagePeer *peer = NULL,
                    LinkagePeer *me = NULL);

    /// Low level listen.
    bool Listen(const Socket &socket,
                const Option &option,
                LinkagePeer *me = NULL);

    /// Low level accepted.
    bool Accepted(const Option &option, int fd);

    /// Low level connect.
    int Connect(const Socket &socket,
                const Option &option,
                LinkagePeer *peer = NULL,
                LinkagePeer *me = NULL);

    /// @param peer can NOT be NULL.
    /// @param me can be NULL.
    /// @retval <0 listen fd failure
    /// @retval  0 successful
    /// @retval >0 accepted fd failure
    int Accept(const Option &option,
               LinkagePeer *peer,
               LinkagePeer *me = NULL);

    /// @param timeout to wait, <0 for infinity.
    /// @warning only call to this method if connect() gives "in progress".
    bool WaitUntilConnected(int64_t timeout);

    /// @warning only call to this method if connect() gives "in progress" and the fd is
    ///          writable for the first time.
    bool TestIfConnected();

    /// Shutdown (but keep fd) all underlying sockets.
    bool Shutdown();

    /// Close all underlying sockets.
    bool Close();

    int fd() const
    {
        return _socket;
    }

    static const int kMaximumQueueLength = 256;

private:
    std::string _sockname;
    int _socket;
    int _domain;

    template <class T>
    int DoConnectTcp(const Socket &socket, const Option &option,
                     LinkagePeer *peer, LinkagePeer *me);

    int DoConnectUnix(const Socket &socket, const Option &option,
                      LinkagePeer *peer, LinkagePeer *me);

    bool DoListenTcp4(const Socket &socket, const Option &option, LinkagePeer *me);
    bool DoListenTcp6(const Socket &socket, const Option &option, LinkagePeer *me);
    bool DoListenUnix(const Socket &socket, const Option &option, LinkagePeer *me);
    bool DoListenUnixSocket(const Socket &socket, const Option &option, LinkagePeer *me);
    bool DoListenUnixNamespace(const Socket &socket, const Option &option, LinkagePeer *me);

    static bool InitializeSocket(const Option &option, int s);
    static int CreateSocket(const Socket &socket, const Option &option);
    static int CreateListenSocket(const Socket &socket,
                                  const Option &option,
                                  void *sockaddr,
                                  size_t addrlen);

    static int Bind(int s, void *sockaddr, size_t addrlen);

}; // class Interface

} // namespace flinter

std::ostream &operator << (std::ostream &s, const flinter::Interface::Socket &d);
std::ostream &operator << (std::ostream &s, const flinter::Interface::Option &d);

#endif //FLINTER_LINKAGE_INTERFACE_H
