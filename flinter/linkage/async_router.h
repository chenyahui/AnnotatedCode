/* Copyright 2015 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_LINKAGE_ASYNC_ROUTER_H
#define FLINTER_LINKAGE_ASYNC_ROUTER_H

#include <assert.h>
#include <stdint.h>

#include <string>

#include <flinter/linkage/route_handler.h>
#include <flinter/object_map.h>
#include <flinter/timeout_pool.h>

namespace flinter {

template <class key_t,
          class task_t,
          class seq_t = uint64_t,
          class channel_t = uint64_t>
class AsyncRouter {
public:
    /// @param route_handler life span NOT taken.
    /// @param default_timeout 5s if <= 0.
    explicit AsyncRouter(
            RouteHandler<key_t, task_t, seq_t, channel_t> *route_handler,
            int64_t default_timeout = 0);

    ~AsyncRouter();

    void Add(const key_t &host);
    void Del(const key_t &host);

    template <class all_t>
    void SetAll(const all_t &hosts);

    /// @param seq to identify this call in a multiplex connection.
    /// @param task can be NULL, can be retrieved in Put(). If timed out, it's
    ///             auto released.
    /// @param channel return outgoing channel.
    /// @param timeout use default if <= 0.
    bool Get(const seq_t &seq,
             task_t *task,
             channel_t *channel,
             int64_t timeout = 0);

    /// @param seq to identify this call in a multiplex connection.
    /// @param rc 0 if succeeded.
    /// @param task if not null, return pointer set in Get(), caller to release.
    bool Put(const seq_t &seq, int rc, task_t **task);

    void Shutdown();
    void Cleanup();

    /// @warning not thread safe.
    void Clear();

private:
    struct context_t {
        key_t key;
        size_t pendings;
        channel_t channel;
        int64_t last_active;

    }; // struct context_t

    class Map : public ObjectMap<key_t, context_t> {
    public:
        explicit Map(AsyncRouter *router) : _router(router) {}
        virtual ~Map() {}

        virtual context_t *Create(const key_t &key)
        {
            return _router->CreateConnection(key);
        }

        virtual void Destroy(context_t *value)
        {
            _router->DestroyConnection(value);
        }

    private:
        AsyncRouter *const _router;

    }; // class Map

    class Track {
    public:
        Track(AsyncRouter *router,
              const seq_t &seq,
              task_t *task,
              context_t *context) : _seq(seq)
                                  , _task(task)
                                  , _context(context)
                                  , _router(router)
                                  , _done(false) {}

        ~Track()
        {
            if (!_done) {
                _router->OnTimeout(this);
            }
        }

        void Done()
        {
            _done = true;
        }

        const seq_t _seq;
        task_t *const _task;
        context_t *const _context;
        AsyncRouter *const _router;

    private:
        bool _done;

    }; // class Track

    // Called by Map.
    context_t *CreateConnection(const key_t &key);
    void DestroyConnection(context_t *context);

    // Called by Track.
    void OnTimeout(Track *track);

    RouteHandler<key_t, task_t, seq_t, channel_t> *const _route_handler;
    TimeoutPool<seq_t, Track *> _timeout;
    Map *const _map;
    bool _clearing;

    AsyncRouter &operator = (const AsyncRouter &);
    explicit AsyncRouter(const AsyncRouter &);

}; // class AsyncRouter

template <class K, class T, class S, class C>
inline AsyncRouter<K, T, S, C>::AsyncRouter(
        RouteHandler<K, T, S, C> *route_handler,
        int64_t default_timeout)
        : _route_handler(route_handler)
        , _timeout(default_timeout > 0 ? default_timeout : 5000000000LL)
        , _map(new Map(this))
        , _clearing(false)
{
    // Intended left blank.
}

template <class K, class T, class S, class C>
inline AsyncRouter<K, T, S, C>::~AsyncRouter()
{
    Clear();
    delete _map;
}

template <class K, class T, class S, class C>
inline bool AsyncRouter<K, T, S, C>::Get(const S &seq,
                                         T *task,
                                         C *channel,
                                         int64_t timeout)
{
    context_t *c = _map->GetNext();
    if (!c) {
        return false;
    }

    _timeout.Insert(seq, new Track(this, seq, task, c), timeout);
    *channel = c->channel;
    return true;
}

template <class K, class T, class S, class C>
inline bool AsyncRouter<K, T, S, C>::Put(const S &seq, int rc, T **task)
{
    Track *track = _timeout.Erase(seq);
    if (!track) {
        return false;
    }

    if (rc) {
        // TODO(yiyuanzhong): report call result.
    }

    if (task) {
        *task = track->_task;
    }

    _map->Release(track->_context->key, track->_context);
    track->Done();

    // Don't delete task, give it back.
    delete track;
    return true;
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::Clear()
{
    _clearing = true;
    _timeout.Clear();
    _map->Clear();
    _clearing = false;
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::Shutdown()
{
    _timeout.Clear();
    _map->EraseAll(true);
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::Cleanup()
{
    _timeout.Check();
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::OnTimeout(Track *track)
{
    // Don't lock!
    if (_clearing) {
        return;
    }

    _route_handler->OnTimeout(track->_seq, track->_task);
    _map->Release(track->_context->key, track->_context);
    delete track->_task;
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::Add(const K &host)
{
    context_t *c = _map->Add(host);
    if (!c) {
        return;
    }

    _map->Release(host, c);
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::Del(const K &host)
{
    _map->Erase(host);
}

template <class K, class T, class S, class C>
template <class A>
inline void AsyncRouter<K, T, S, C>::SetAll(const A &hosts)
{
    _map->SetAll(hosts);
}

template <class K, class T, class S, class C>
inline typename AsyncRouter<K, T, S, C>::context_t *
AsyncRouter<K, T, S, C>::CreateConnection(const K &key)
{
    // Don't lock!
    C channel;
    if (!_route_handler->Create(key, &channel)) {
        return NULL;
    }

    context_t *c = new context_t;
    c->channel = channel;
    c->key = key;
    return c;
}

template <class K, class T, class S, class C>
inline void AsyncRouter<K, T, S, C>::DestroyConnection(context_t *value)
{
    // Don't lock!
    _route_handler->Destroy(value->channel);
    delete value;
}

} // namespace flinter

#endif // FLINTER_LINKAGE_ASYNC_ROUTER_H
