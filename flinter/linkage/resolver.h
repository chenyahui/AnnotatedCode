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

#ifndef FLINTER_LINKAGE_RESOLVER_H
#define FLINTER_LINKAGE_RESOLVER_H

#include <netinet/in.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <flinter/thread/condition.h>
#include <flinter/thread/mutex.h>
#include <flinter/singleton.h>

struct addrinfo;

namespace flinter {

/// Resolver is a hostname to IPv4 cache.
/// @warning srand(3) before using any random based resolving.
class Resolver : public Singleton<Resolver> {
    DECLARE_SINGLETON(Resolver);

public:
    enum Option {
        kFirst,
        kRandom,
        kSequential,
    }; // enum Option

    bool Resolve(const char *hostname, uint16_t port,
                 struct sockaddr_in *addr,
                 const Option &option = kSequential);

    bool Resolve(const char *hostname, uint16_t port,
                 struct sockaddr_in6 *addr,
                 const Option &option = kSequential);

    /// Invalidate all cache immediately.
    void Clear();

private:
    template <class T, class A>
    class Cache {
    public:
        Cache() : _resolved(-1), _current(-1), _resolving(false) {}
        void Set(struct addrinfo *res);
        typedef T sockaddr_t;
        typedef A addr_t;

        std::vector<T> _addr;
        int64_t _resolved;
        ssize_t _current;
        bool _resolving;
    }; // class Cache

    typedef Cache<struct sockaddr_in,  struct in_addr > cache4_t;
    typedef Cache<struct sockaddr_in6, struct in6_addr> cache6_t;

    template <class T, class C>
    static bool Pick(T *addr, C *cache,
                     const Option &option,
                     unsigned int *seed);

    Resolver();
    ~Resolver();

    template <class T, class C>
    bool DoResolve(T *addr, const char *hostname,
                   uint16_t port, const Option &option,
                   std::map<std::string, C *> *address,
                   int domain, int flags);

    template <class C>
    bool DoResolve_getaddrinfo(const char *hostname, C *cache,
                               int domain, int flags);

    void Aging(int64_t now);

    static const int64_t kRefreshTime;   ///< Resolve again for a host.
    static const int64_t kCacheExpire;   ///< No resolving and purge the host.
    static const int64_t kAgingInterval; ///< How many seconds that we purge.

    typedef std::map<std::string, cache4_t *> address4_t;
    typedef std::map<std::string, cache6_t *> address6_t;
    address4_t _addr4;
    address6_t _addr6;

    Condition _condition;
    int64_t _last_aging;
    unsigned int _seed;
    Mutex _mutex;

}; // class Resolver

} // namespace flinter

#endif // FLINTER_LINKAGE_RESOLVER_H
