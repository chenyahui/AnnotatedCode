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

#include "flinter/linkage/linkage_worker.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <list>
#include <stdexcept>

#include "flinter/linkage/interface.h"
#include "flinter/linkage/linkage_base.h"
#include "flinter/linkage/linkage_peer.h"
#include "flinter/thread/mutex.h"
#include "flinter/thread/mutex_locker.h"
#include "flinter/logger.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

#include "config.h"
#if HAVE_EV_H
#include <ev.h>

namespace flinter {

struct LinkageWorker::client_t {
    LinkageBase *linkage;
    struct ev_io ev_io_r;
    struct ev_io ev_io_w;
    bool auto_release;
    bool running_r;
    bool running_w;
    int fd;
}; // struct LinkageWorker::client_t

struct LinkageWorker::timer_t {
    struct ev_timer ev_timer;
    Runnable *runnable;
    bool auto_release;
}; // struct LinkageWorker::timer_t

class LinkageWorker::HealthTimer : public Runnable {
public:
    HealthTimer(LinkageWorker *worker) : _worker(worker) {}
    virtual ~HealthTimer() {}

protected:
    virtual bool Run()
    {
        return _worker->OnHealthCheck(get_monotonic_timestamp());
    }

private:
    LinkageWorker *_worker;

}; // class LinkageWorker::HealthTimer

LinkageWorker::LinkageWorker() : _loop(ev_loop_new(0))
                               , _quit(false)
                               , _running_thread_id(0)
                               , _async(new struct ev_async)
                               , _mutex(new Mutex)
{
    if (!_loop) {
        throw std::runtime_error("LinkageWorker: failed to initialize loop.");
    }

    ev_async_init(_async, command_cb);
    ev_async_start(_loop, _async);
    ev_set_userdata(_loop, this);
}

LinkageWorker::~LinkageWorker()
{
    ev_async_stop(_loop, _async);
    ev_loop_destroy(_loop);
    delete _mutex;
    delete _async;
}

void LinkageWorker::command_cb(struct ev_loop *loop, struct ev_async * /*w*/, int /*revents*/)
{
    LinkageWorker *worker = reinterpret_cast<LinkageWorker *>(ev_userdata(loop));
    worker->OnCommand();
}

void LinkageWorker::readable_cb(struct ev_loop *loop, struct ev_io *w, int /*revents*/)
{
    LinkageWorker *worker = reinterpret_cast<LinkageWorker *>(ev_userdata(loop));
    unsigned char *p = reinterpret_cast<unsigned char *>(w);
    p -= offsetof(struct client_t, ev_io_r);
    struct client_t *client = reinterpret_cast<struct client_t *>(p);
    worker->OnReadable(client);
}

void LinkageWorker::writable_cb(struct ev_loop *loop, struct ev_io *w, int /*revents*/)
{
    LinkageWorker *worker = reinterpret_cast<LinkageWorker *>(ev_userdata(loop));
    unsigned char *p = reinterpret_cast<unsigned char *>(w);
    p -= offsetof(struct client_t, ev_io_w);
    struct client_t *client = reinterpret_cast<struct client_t *>(p);
    worker->OnWritable(client);
}

void LinkageWorker::timer_cb(struct ev_loop *loop, struct ev_timer *w, int /*revents*/)
{
    LinkageWorker *worker = reinterpret_cast<LinkageWorker *>(ev_userdata(loop));
    unsigned char *p = reinterpret_cast<unsigned char *>(w);
    p -= offsetof(struct timer_t, ev_timer);
    struct timer_t *timer = reinterpret_cast<struct timer_t *>(p);
    worker->OnTimer(timer);
}

void LinkageWorker::OnTimer(struct timer_t *timer)
{
    if (!timer->runnable->Run()) {
        DoRelease(timer, true);
    }
}

bool LinkageWorker::RegisterTimer(int64_t interval,
                                  Runnable *runnable,
                                  bool auto_release)
{
    return RegisterTimer(interval, interval, runnable, auto_release);
}

bool LinkageWorker::RegisterTimer(int64_t after,
                                  int64_t repeat,
                                  Runnable *runnable,
                                  bool auto_release)
{
    if (after < 0 || repeat <= 0 || !runnable) {
        return false;
    }

    double r = static_cast<double>(repeat) / 1000000000;
    double a = static_cast<double>(after) / 1000000000;
    struct timer_t *timer = new struct timer_t;
    struct ev_timer *t = &timer->ev_timer;
    timer->auto_release = auto_release;
    timer->runnable = runnable;
    _timers.insert(timer);

    ev_timer_init(t, timer_cb, a, r);
    ev_timer_start(_loop, &timer->ev_timer);
    return true;
}

bool LinkageWorker::Run()
{
    if (!OnInitialize()) {
        CLOG.Error("Linkage: failed to initialize.");
        throw std::runtime_error("Linkage: failed to initialize.");
    }

    // 每1秒对连接进行一次健康检查
    HealthTimer *health = new HealthTimer(this);
    if (!RegisterTimer(1000000000LL, health, true)) {
        delete health;
        CLOG.Error("Linkage: failed to register timers.");
        throw std::runtime_error("Linkage: failed to register timers.");
    }

    _running_thread_id = get_current_thread_id();
    LOG(VERBOSE) << "Linkage: enter event loop [" << _running_thread_id << "]";
    while (!_quit) {
        if (!ev_run(_loop, 0)) {
            CLOG.Error("Linkage: ev_run() returned false.");
            throw std::runtime_error("Linkage: ev_run() returned false.");
        }
    }
    LOG(VERBOSE) << "Linkage: leave event loop [" << _running_thread_id << "]";
    _running_thread_id = 0;

    OnShutdown();

    for (events_t::iterator p = _events.begin(); p != _events.end(); ++p) {
        DoRelease(p->second, false);
    }

    for (timers_t::iterator p = _timers.begin(); p != _timers.end(); ++p) {
        DoRelease(*p, false);
    }

    _events.clear();
    _timers.clear();
    return true;
}

bool LinkageWorker::Shutdown()
{
    return SendCommand(NULL);
}

bool LinkageWorker::SendCommand(Runnable *command)
{
    MutexLocker locker(_mutex);
    _commands.push_back(command);
    locker.Unlock();

    ev_async_send(_loop, _async);
    return true;
}

void LinkageWorker::OnCommand()
{
    std::list<Runnable *> q;
    MutexLocker locker(_mutex);
    q.swap(_commands);
    locker.Unlock();

    for (std::list<Runnable *>::iterator p = q.begin(); p != q.end(); ++p) {
        Runnable *command = *p;
        if (!command) {
            ev_break(_loop, EVBREAK_ALL);
            _quit = true;
            return;
        }

        if (!command->Run()) {
            break;
        }
    }

    for (std::list<Runnable *>::iterator p = q.begin(); p != q.end(); ++p) {
        delete *p;
    }
}

bool LinkageWorker::Attach(LinkageBase *linkage,
                           int fd,
                           bool read_now,
                           bool write_now,
                           bool auto_release)
{
    if (!linkage) {
        return false;
    } else if (_events.find(linkage) != _events.end()) {
        return true;
    }

    struct client_t *client = new struct client_t;
    struct ev_io *ev_io_r = &client->ev_io_r;
    struct ev_io *ev_io_w = &client->ev_io_w;
    ev_io_init(ev_io_r, readable_cb, fd, EV_READ);
    ev_io_init(ev_io_w, writable_cb, fd, EV_WRITE);
    client->auto_release = auto_release;
    client->linkage = linkage;
    client->running_r = read_now;
    client->running_w = write_now;
    client->fd = fd;

    CLOG.Verbose("Linkage: attached %p:%p:%d:%d", this, linkage, fd, auto_release);
    _events.insert(std::make_pair(linkage, client));

    if (read_now) {
        ev_io_start(_loop, &client->ev_io_r);
    }

    if (write_now) {
        ev_io_start(_loop, &client->ev_io_w);
    }

    return true;
}

bool LinkageWorker::Detach(LinkageBase *linkage)
{
    if (!linkage) {
        return false;
    }

    events_t::iterator p = _events.find(linkage);
    if (p == _events.end()) {
        return true;
    }

    struct client_t *client = p->second;
    if (client->running_r) {
        ev_io_stop(_loop, &client->ev_io_r);
    }

    if (client->running_w) {
        ev_io_stop(_loop, &client->ev_io_w);
    }

    CLOG.Verbose("Linkage: detached %p:%p", this, linkage);
    _events.erase(p);
    delete client;
    return true;
}

void LinkageWorker::OnReadable(struct client_t *client)
{
    CLOG.Verbose("LinkageWorker: OnReadable(%p)", client->linkage);
    int ret = client->linkage->OnReadable(this);
    if (ret < 0) {
        OnError(client, true, errno);
    } else if (ret == 0) {
        OnDisconnected(client);
    }
}

void LinkageWorker::OnWritable(struct client_t *client)
{
    CLOG.Verbose("LinkageWorker: OnWritable(%p)", client->linkage);
    int ret = client->linkage->OnWritable(this);
    if (ret < 0) {
        OnError(client, false, errno);
    } else if (ret == 0) {
        OnDisconnected(client);
    }
}

void LinkageWorker::OnError(struct client_t *client, bool reading_or_writing, int errnum)
{
    client->linkage->OnError(reading_or_writing, errnum);
    DoRelease(client, true);
}

void LinkageWorker::OnDisconnected(struct client_t *client)
{
    DoRelease(client, true);
}

void LinkageWorker::DoRelease(struct client_t *client, bool erase_container)
{
    if (client->running_r) {
        ev_io_stop(_loop, &client->ev_io_r);
    }

    if (client->running_w) {
        ev_io_stop(_loop, &client->ev_io_w);
    }

    if (erase_container) {
        events_t::iterator p = _events.find(client->linkage);
        assert(p != _events.end());
        assert(client == p->second);
        _events.erase(p);
    }

    // TODO(yiyuanzhong): dangerous, crash might occur.
    //
    // It was originally designed that users should never use the pointer
    //     `linkage` after OnDisconnected() is called, which is correct if
    //     users are within the same thread as LinkageWorker. However if
    //     they call SendCommand() from another thread, which adds a job
    //     referring to the pointer, the job might be executed after the
    //     pointer has been released.
    //
    // There're two possible fixes, one is to eliminate all jobs referring
    //     the pointer before releasing it, simple as it sounds, there's no
    //     way I can tell jobs from each other, unless I design a different
    //     command queue.
    //
    // Another way is to defer releasing linkages, which requires additional
    //     variables and memory can be held longer than usual. Also if you're
    //     debugging you might find it inconvenient.
    //
    // I'm still thinking about it.
    client->linkage->OnDisconnected();

    if (client->auto_release) {
        CLOG.Verbose("Linkage: closed linkage [%p], auto released.", client->linkage);
        delete client->linkage;
    } else {
        CLOG.Verbose("Linkage: closed linkage [%p], not released.", client->linkage);
    }

    delete client;
}

void LinkageWorker::DoRelease(struct timer_t *timer, bool erase_container)
{
    ev_timer_stop(_loop, &timer->ev_timer);
    if (timer->auto_release) {
        CLOG.Verbose("Linkage: stopped timer [%p], auto released.", timer->runnable);
        delete timer->runnable;
    } else {
        CLOG.Verbose("Linkage: stopped timer [%p], not released.", timer->runnable);
    }

    delete timer;
    if (erase_container) {
        _timers.erase(timer);
    }
}

bool LinkageWorker::SetWanna(LinkageBase *linkage, bool wanna_read, bool wanna_write)
{
    events_t::iterator p = _events.find(linkage);
    if (p == _events.end()) {
        return false;
    }

    struct client_t *client = p->second;
    if (wanna_read != client->running_r) {
        client->running_r = wanna_read;
        if (wanna_read) {
            ev_io_start(_loop, &client->ev_io_r);
        } else {
            ev_io_stop(_loop, &client->ev_io_r);
        }
    }

    if (wanna_write != client->running_w) {
        client->running_w = wanna_write;
        if (wanna_write) {
            ev_io_start(_loop, &client->ev_io_w);
        } else {
            ev_io_stop(_loop, &client->ev_io_w);
        }
    }

    return true;
}

bool LinkageWorker::SetWannaRead(LinkageBase *linkage, bool wanna)
{
    events_t::iterator p = _events.find(linkage);
    if (p == _events.end()) {
        return false;
    }

    struct client_t *client = p->second;
    if (wanna == client->running_r) {
        return true;
    }

    if (wanna) {
        ev_io_start(_loop, &client->ev_io_r);
    } else {
        ev_io_stop(_loop, &client->ev_io_r);
    }

    client->running_r = wanna;
    return true;
}

bool LinkageWorker::SetWannaWrite(LinkageBase *linkage, bool wanna)
{
    events_t::iterator p = _events.find(linkage);
    if (p == _events.end()) {
        return false;
    }

    struct client_t *client = p->second;
    if (wanna == client->running_w) {
        return true;
    }

    if (wanna) {
        ev_io_start(_loop, &client->ev_io_w);
    } else {
        ev_io_stop(_loop, &client->ev_io_w);
    }

    client->running_w = wanna;
    return true;
}

bool LinkageWorker::OnInitialize()
{
    return true;
}

void LinkageWorker::OnShutdown()
{
    // Intended left blank.
}

bool LinkageWorker::OnHealthCheck(int64_t now)
{
    std::list<struct client_t *> drops;
    for (events_t::iterator p = _events.begin(); p != _events.end(); ++p) {
        struct client_t *client = p->second;
        LinkageBase *linkage = client->linkage;
        if (!linkage->Cleanup(now)) {
            drops.push_back(client);
        }
    }

    for (std::list<struct client_t *>::iterator p = drops.begin();
         p != drops.end(); ++p) {

        struct client_t *client = *p;
        CLOG.Verbose("Linkage: disconnect timed out connection: fd = %d",
                     client->fd);

        DoRelease(client, true);
    }

    return true;
}

} // namespace flinter

#endif // HAVE_EV_H
