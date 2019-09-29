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

#ifdef __unix__

#include "flinter/linkage/resolver.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <set>

#include "flinter/thread/mutex_locker.h"
#include "flinter/logger.h"
#include "flinter/trim.h"
#include "flinter/utility.h"

namespace flinter {

const int64_t Resolver::kRefreshTime   =  300000000000LL;   ///< 5min
const int64_t Resolver::kCacheExpire   = 3600000000000LL;   ///< 1h
const int64_t Resolver::kAgingInterval =  300000000000LL;   ///< 5min

static inline struct in_addr &Get(struct sockaddr_in *addr)
{
    return addr->sin_addr;
}

static inline struct in6_addr &Get(struct sockaddr_in6 *addr)
{
    return addr->sin6_addr;
}

static inline void Set(struct sockaddr_in *addr, uint16_t port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
}

static inline void Set(struct sockaddr_in6 *addr, uint16_t port)
{
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
}

class Less {
public:
    bool operator()(const struct in_addr &a, const struct in_addr &b)
    {
        return a.s_addr < b.s_addr;
    }
    bool operator()(const struct in6_addr &a, const struct in6_addr &b)
    {
        return memcmp(&a, &b, sizeof(struct in6_addr)) < 0;
    }
}; // class Less4

template <class T, class A>
void Resolver::Cache<T, A>::Set(struct addrinfo *res)
{
    if (!res) {
        _addr.clear();
        _current = -1;
        return;
    }

    size_t size = 0;
    for (struct addrinfo *p = res; p; p = p->ai_next, ++size);

    typename std::set<A, Less> s;
    for (typename std::vector<T>::iterator
         p = _addr.begin(); p != _addr.end(); ++p) {

        s.insert(Get(&*p));
    }

    bool diff = false;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        T *a = reinterpret_cast<T *>(p->ai_addr);
        typename std::set<A>::iterator q = s.find(Get(a));
        if (q == s.end()) {
            diff = true;
            break;
        }

        s.erase(q);
    }

    if (!diff && s.empty()) {
        return;
    }

    size_t i = 0;
    _current = -1;
    _addr.resize(size);
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        T *addr = &_addr[i++];
        memcpy(addr, p->ai_addr, sizeof(T));
    }
}

template <class T, class C>
bool Resolver::Pick(T *addr, C *cache,
                    const Option &option,
                    unsigned int *seed)
{
    if (cache->_addr.empty()) {
        return false;
    }

    int size = static_cast<int>(cache->_addr.size());
    if (option == kFirst || cache->_addr.size() == 1) {
        cache->_current = 0;

    } else if (option == kSequential) {
        cache->_current = (cache->_current + 1) % size;

    } else {
        cache->_current = ranged_rand_r(size, seed);
    }

    memcpy(addr, &cache->_addr[static_cast<size_t>(cache->_current)], sizeof(T));
    return true;
}

template <class C>
bool Resolver::DoResolve_getaddrinfo(const char *hostname, C *cache,
                                     int domain, int flags)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = domain;
    hints.ai_flags    = flags;

    // Might not be the case but it's always the same using numeric port.
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res;
    int ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret) {
        switch (ret) {
        case EAI_NONAME:
            res = NULL;
            break;

        case EAI_SYSTEM:
            CLOG.Trace("Resolver: failed to resolve [%s]: %d: %s",
                       hostname, errno, strerror(errno));

            return false;

        default:
            CLOG.Trace("Resolver: failed to resolve [%s]: %d: %s",
                       hostname, ret, gai_strerror(ret));

            return false;
        }

    } else if (!res) {
        CLOG.Trace("Resolver: failed to resolve [%s]: empty response", hostname);
        return false;
    }

    cache->Set(res);
    if (res) {
        freeaddrinfo(res);
    }

    return true;
}

bool Resolver::Resolve(const char *hostname, uint16_t port,
                       struct sockaddr_in *addr,
                       const Option &option)
{
    return DoResolve(addr, hostname, port, option, &_addr4,
                     AF_INET, AI_ADDRCONFIG);
}

bool Resolver::Resolve(const char *hostname, uint16_t port,
                       struct sockaddr_in6 *addr,
                       const Option &option)
{
    return DoResolve(addr, hostname, port, option, &_addr6,
                     AF_INET6, AI_ADDRCONFIG | AI_V4MAPPED);
}

template <class T, class C>
bool Resolver::DoResolve(T *addr, const char *hostname,
                         uint16_t port, const Option &option,
                         std::map<std::string, C *> *address,
                         int domain, int flags)
{
    if (!hostname || !*hostname || !port || !addr) {
        return false;
    }

    memset(addr, 0, sizeof(*addr));
    if (inet_pton(domain, hostname, &Get(addr)) == 1) {
        Set(addr, port);
        return true;
    }

    char host[1024];
    size_t hlen = strlen(hostname);
    if (hlen >= sizeof(host)) {
        return false;
    }

    const int64_t now = get_monotonic_timestamp();
    const int64_t deadline = now - kRefreshTime;
    std::transform(hostname, hostname + hlen, host, ::tolower);
    host[hlen] = '\0';
    std::string shost = host;

    MutexLocker locker(&_mutex);
    Aging(now);

    typename std::map<std::string, C *>::iterator p = address->find(shost);
    if (p == address->end()) {
        p = address->insert(std::make_pair(shost, new C)).first;
    }

    C *c = p->second;
    while (c->_resolved < 0 || c->_resolved < deadline) {
        if (c->_resolving) {
            do {
                _condition.Wait(&_mutex);
            } while (c->_resolving);
            continue;
        }

        c->_resolving = true;
        locker.Unlock();
        bool ret = DoResolve_getaddrinfo(host, c, domain, flags);
        locker.Relock();

        c->_resolving = false;
        _condition.WakeAll();

        if (ret) {
            CLOG.Verbose("Resolver: resolved [%s] to %lu result(s).",
                         hostname, c->_addr.size());

            c->_resolved = get_monotonic_timestamp();
        }
    }

    if (!Pick(addr, c, option, &_seed)) {
        return false;
    }

    Set(addr, port);
    return true;
}

void Resolver::Clear()
{
    size_t count = 0;
    MutexLocker locker(&_mutex);
    for (address4_t::iterator p = _addr4.begin(); p != _addr4.end();) {
        address4_t::iterator q = p++;
        if (!q->second->_resolving) {
            delete q->second;
            _addr4.erase(q);
            ++count;
        }
    }

    for (address6_t::iterator p = _addr6.begin(); p != _addr6.end();) {
        address6_t::iterator q = p++;
        if (!q->second->_resolving) {
            delete q->second;
            _addr6.erase(q);
            ++count;
        }
    }

    if (count) {
        CLOG.Verbose("Resolver: removed %lu hosts.", count);
    }
}

// Call locked.
void Resolver::Aging(int64_t now)
{
    // Don't make it too frequent.
    if (now - _last_aging < kAgingInterval) {
        return;
    }
    _last_aging = now;

    size_t count = 0;
    const int64_t deadline = now - kCacheExpire;
    for (address4_t::iterator p = _addr4.begin(); p != _addr4.end();) {
        address4_t::iterator q = p++;
        if (!q->second->_resolving && q->second->_resolved < deadline) {
            delete q->second;
            _addr4.erase(q);
            ++count;
        }
    }

    for (address6_t::iterator p = _addr6.begin(); p != _addr6.end();) {
        address6_t::iterator q = p++;
        if (!q->second->_resolving && q->second->_resolved < deadline) {
            delete q->second;
            _addr6.erase(q);
            ++count;
        }
    }

    if (count) {
        CLOG.Verbose("Resolver: aged %lu hosts.", count);
    }
}

Resolver::Resolver() : _last_aging(get_monotonic_timestamp())
                     , _seed(randomize_r())
{
    // Intended left blank.
}

Resolver::~Resolver()
{
    Clear();
}

} // namespace flinter

#endif // __unix__
