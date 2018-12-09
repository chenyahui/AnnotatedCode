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

#include "flinter/zookeeper/zookeeper.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include <vector>

#include <zookeeper/zookeeper.h>

#include "flinter/thread/mutex_locker.h"
#include "flinter/common.h"
#include "flinter/logger.h"
#include "flinter/msleep.h"
#include "flinter/utility.h"

#include "flinter/zookeeper/zookeeper_callback.h"
#include "flinter/zookeeper/zookeeper_watcher.h"

namespace flinter {

namespace {
static const ZooKeeper::Id ANYONE("world", "anyone");
static const ZooKeeper::Acl::value_type ANYONE_OPEN[] = {
    ZooKeeper::Acl::value_type(ANYONE, ZOO_PERM_ALL)
};

static const ZooKeeper::Acl::value_type ANYONE_READ[] = {
    ZooKeeper::Acl::value_type(ANYONE, ZOO_PERM_READ)
};
} // anonymous namespace

const ZooKeeper::Acl ZooKeeper::ACL_ANYONE_OPEN(ANYONE_OPEN,
                                                ANYONE_OPEN + ARRAYSIZEOF(ANYONE_OPEN));

const ZooKeeper::Acl ZooKeeper::ACL_ANYONE_READ(ANYONE_READ,
                                                ANYONE_READ + ARRAYSIZEOF(ANYONE_READ));

ZooKeeper::ZooKeeper() : _resuming(false)
                       , _session_pending(this)
                       , _handle(NULL)
                       , _connecting(false)
                       , _timeout(-1)
{
    memset(&_client_id, 0, sizeof(_client_id));
}

ZooKeeper::~ZooKeeper()
{
    Shutdown();
}

void ZooKeeper::SetLog(FILE *stream, const ZooLogLevel &level)
{
    zoo_set_log_stream(stream);
    zoo_set_debug_level(level);
}

void ZooKeeper::GlobalWatcher(zhandle_t * /*zh*/,
                               int type,
                               int state,
                               const char *path,
                               void *watcherCtx)
{
    CLOG.Debug("ZooKeeper: GlobalWatcher(type=%d, state=%d, path=%s, watcherCtx=%p)",
               type, state, path, watcherCtx);

    assert(watcherCtx);

    Pending *p = reinterpret_cast<Pending *>(watcherCtx);
    assert(p->zk());

    // Session events are special: they're triggered automatically and passively.
    if (p->op() == OP_SESSION) {
        if (type == ZOO_SESSION_EVENT) {
            // The global session watcher.
            p->zk()->OnSession(state);
        }
        return;
    }

    if (!p->zkw_detached()) {
        assert(p->zkw());
        p->zkw()->OnEvent(type, state, path);
    }

    // Normal watchers are short lived.
    // But session event is an exception.
    if (type != ZOO_SESSION_EVENT) {
        p->clear_watcher();
        p->zk()->ErasePending(p);
    }
}

void ZooKeeper::GlobalVoidCompletion(int rc, const void *data)
{
    CLOG.Debug("ZooKeeper: GlobalVoidCompletion(rc=%d, data=%p)", rc, data);

    // Don't worry, it's born mutable.
    assert(data);
    void *mutable_data = const_cast<void *>(data);
    Pending *p = reinterpret_cast<Pending *>(mutable_data);

    assert(!p->zkw());
    if (!p->zkc()) {
        CLOG.Debug("ZooKeeper: global completion: detached pending %p done, not deleted.", p);
        return;
    }

    if (p->op() == OP_DELETE) {
        p->zkc()->OnErase(rc, p->path());
    } else if (p->op() == OP_SETA) {
        p->zkc()->OnSetAcl(rc, p->path());
    } else {
        CLOG.Warn("ZooKeeper: global completion has got an invalid event.");
    }

    p->clear_callback();
    p->zk()->ErasePending(p);
}

void ZooKeeper::GlobalStatCompletion(int rc, const struct Stat *stat, const void *data)
{
    CLOG.Debug("ZooKeeper: GlobalStatCompletion(rc=%d, stat=%p, data=%p)", rc, stat, data);

    // Don't worry, it's born mutable.
    assert(data);
    void *mutable_data = const_cast<void *>(data);
    Pending *p = reinterpret_cast<Pending *>(mutable_data);

    if (!p->zkc()) { // It's OK, we're just resuming watchers.
        if (p->zkw_detached()) {
            CLOG.Debug("ZooKeeper: global completion: detached pending %p done, not deleted.", p);
            return;
        }

        assert(p->zkw());
        assert(p->op() == OP_EXISTS);
        if (rc == ZOK) {
            CLOG.Trace("ZooKeeper: resumed EXISTS (pending=%p, path=%s)", p, p->path());
        } else {
            CLOG.Warn("ZooKeeper: failed to resume EXISTS (pending=%p, path=%s, rc=%d)",
                      p, p->path(), rc);

            p->zk()->ErasePending(p);
        }
        return;
    }

    if (p->op() == OP_EXISTS) {
        p->zkc()->OnExists(rc, p->path(), stat);
    } else if (p->op() == OP_SET) {
        p->zkc()->OnSet(rc, p->path(), stat);
    } else {
        CLOG.Warn("ZooKeeper: global completion has got an invalid event.");
    }

    p->clear_callback();
    if (!p->zkw() && !p->zkw_detached()) { // Pending done.
        p->zk()->ErasePending(p);
    }
}

void ZooKeeper::GlobalDataCompletion(int rc,
                                     const char *value,
                                     int value_len,
                                     const struct Stat *stat,
                                     const void *data)
{
    CLOG.Debug("ZooKeeper: GlobalDataCompletion(rc=%d, value=%p, value_len=%d, stat=%p, data=%p)",
               rc, value, value_len, stat, data);

    // Don't worry, it's born mutable.
    assert(data);
    void *mutable_data = const_cast<void *>(data);
    Pending *p = reinterpret_cast<Pending *>(mutable_data);

    if (!p->zkc()) { // It's OK, we're just resuming watchers.
        if (p->zkw_detached()) {
            CLOG.Debug("ZooKeeper: global completion: detached pending %p done, not deleted.", p);
            return;
        }

        assert(p->zkw());
        assert(p->op() == OP_GET);
        if (rc == ZOK) {
            CLOG.Trace("ZooKeeper: resumed  GET   (pending=%p, path=%s)", p, p->path());
        } else {
            CLOG.Warn("ZooKeeper: failed to resume  GET   (pending=%p, path=%s, rc=%d)",
                      p, p->path(), rc);

            p->zk()->ErasePending(p);
        }
        return;
    }

    std::string buffer(value, value + value_len);
    if (p->op() == OP_GET) {
        p->zkc()->OnGet(rc, p->path(), buffer, stat);
    } else {
        CLOG.Warn("ZooKeeper: global completion has got an invalid event.");
    }

    p->clear_callback();
    if (!p->zkw() && !p->zkw_detached()) { // Pending done.
        p->zk()->ErasePending(p);
    }
}

void ZooKeeper::GlobalStringsStatCompletion(int rc,
                                            const String_vector *strings,
                                            const struct Stat *stat,
                                            const void *data)
{
    CLOG.Debug("ZooKeeper: GlobalStringsStatCompletion(rc=%d, strings=%p, stat=%p, data=%p)",
              rc, strings, stat, data);

    // Don't worry, it's born mutable.
    assert(data);
    void *mutable_data = const_cast<void *>(data);
    Pending *p = reinterpret_cast<Pending *>(mutable_data);

    if (!p->zkc()) { // It's OK, we're just resuming watchers.
        if (p->zkw_detached()) {
            CLOG.Debug("ZooKeeper: global completion: detached pending %p done, not deleted.", p);
            return;
        }

        assert(p->zkw());
        assert(p->op() == OP_GETC);
        if (rc == ZOK) {
            CLOG.Trace("ZooKeeper: resumed  GETC  (pending=%p, path=%s)", p, p->path());
        } else {
            CLOG.Warn("ZooKeeper: failed to resume  GETC  (pending=%p, path=%s, rc=%d)",
                      p, p->path(), rc);

            p->zk()->ErasePending(p);
        }
        return;
    }

    std::list<std::string> buffer;
    if (strings) {
        for (int i = 0; i < strings->count; ++i) {
            buffer.push_back(strings->data[i]);
        }
    }

    if (p->op() == OP_GETC) {
        p->zkc()->OnGetChildren(rc, p->path(), buffer, stat);
    } else {
        CLOG.Warn("ZooKeeper: global completion has got an invalid event.");
    }

    p->clear_callback();
    if (!p->zkw() && !p->zkw_detached()) { // Pending done.
        p->zk()->ErasePending(p);
    }
}

void ZooKeeper::GlobalStringCompletion(int rc,
                                       const char *value,
                                       const void *data)
{
    CLOG.Debug("ZooKeeper: GlobalStringCompletion(rc=%d, value=%p, data=%p)", rc, value, data);

    // Don't worry, it's born mutable.
    assert(data);
    void *mutable_data = const_cast<void *>(data);
    Pending *p = reinterpret_cast<Pending *>(mutable_data);

    assert(!p->zkw());
    if (!p->zkc()) {
        CLOG.Debug("ZooKeeper: global completion: detached pending %p done, not deleted.", p);
        return;
    }

    if (p->op() == OP_CREATE) {
        p->zkc()->OnCreate(rc, p->path(), value);
    } else if (p->op() == OP_SYNC) {
        p->zkc()->OnSync(rc, p->path(), value);
    } else {
        CLOG.Warn("ZooKeeper: global completion has got an invalid event.");
    }

    p->clear_callback();
    if (!p->zkw() && !p->zkw_detached()) { // Pending done.
        p->zk()->ErasePending(p);
    }
}

void ZooKeeper::GlobalAclCompletion(int rc,
                                    struct ACL_vector *acl,
                                    struct Stat *stat,
                                    const void *data)
{
    CLOG.Debug("ZooKeeper: GlobalAclCompletion(rc=%d, acl=%p, stat=%p, data=%p)",
               rc, acl, stat, data);

    // Don't worry, it's born mutable.
    assert(data);
    void *mutable_data = const_cast<void *>(data);
    Pending *p = reinterpret_cast<Pending *>(mutable_data);

    assert(!p->zkw());
    if (!p->zkc()) {
        CLOG.Debug("ZooKeeper: global completion: detached pending %p done, not deleted.", p);
        return;
    }

    if (p->op() == OP_GETA) {
        Acl a;
        Translate(acl, &a);
        p->zkc()->OnGetAcl(rc, p->path(), a, stat);

    } else {
        CLOG.Warn("ZooKeeper: global completion has got an invalid event.");
    }

    p->clear_callback();
    if (!p->zkw() && !p->zkw_detached()) { // Pending done.
        p->zk()->ErasePending(p);
    }
}

void ZooKeeper::Detach(ZooKeeperWatcher *zkw, ZooKeeperCallback *zkc)
{
    MutexLocker locker(&_mutex);

    for (std::set<Pending *>::iterator p = _pending.begin(); p != _pending.end(); ++p) {
        Pending *pending = *p;
        if (zkc && pending->zkc() == zkc) {
            pending->clear_callback();
            CLOG.Debug("ZooKeeper: detached callback: %p(%p)", pending, zkc);
        }

        if (zkw && pending->zkw() == zkw) {
            pending->detach_watcher();
            CLOG.Debug("ZooKeeper: detached watcher: %p(%p)", pending, zkw);
        }
    }

    // Don't really erase pending.
}

void ZooKeeper::ErasePending(Pending *pending)
{
    assert(pending);
    assert(!pending->zkc() && !pending->zkw() && !pending->zkw_detached());
    CLOG.Debug("ZooKeeper: deleting %spending %p",
               pending->zkw_detached() ? "detached " : "", pending);

    MutexLocker locker(&_mutex);
    std::set<Pending *>::iterator p = _pending.find(pending);
    if (p != _pending.end()) {
        delete *p;
        _pending.erase(p);
    }
}

void ZooKeeper::ResumeSession(const std::set<Pending *> &pending)
{
    CLOG.Info("ZooKeeper: try to resume session with %lu pendings...", pending.size());

    int status = state();
    size_t failed = 0;
    for (std::set<Pending *>::const_iterator i = pending.begin(); i != pending.end(); ++i) {
        Pending *p = *i;
        assert(p);

        if (!p->zkc() && (!p->zkw() || p->zkw_detached())) {
            CLOG.Debug("ZooKeeper: detached pending %p not resumed.", p);
            continue;
        }

        switch (p->op()) {
        case OP_CREATE:
            // TODO(yiyuanzhong): implement this.
            assert(p->zkc());
            assert(!p->zkw());
            CLOG.Debug("ZooKeeper: resuming CREATE (pending=%p, path=%s)", p, p->path());
            break;

        case OP_DELETE:
            // TODO(yiyuanzhong): implement this.
            assert(p->zkc());
            assert(!p->zkw());
            CLOG.Debug("ZooKeeper: resuming DELETE (pending=%p, path=%s)", p, p->path());
            break;

        case OP_EXISTS:
            assert(p->zkc() || p->zkw());
            if (!p->zkc()) { // Force trigger the watcher.
                p->zkw()->OnEvent(ZOO_CREATED_EVENT, status, p->path());
            } else if (AsyncExists(p->path(), p->zkc(), p->zkw()) == ZOK) {
                CLOG.Debug("ZooKeeper: resuming EXISTS (pending=%p, path=%s)", p, p->path());
            } else {
                CLOG.Warn("ZooKeeper: failed to resume EXISTS (pending=%p, path=%s)", p, p->path());
                p->zkc()->OnExists(ZSESSIONEXPIRED, p->path(), NULL);
                ++failed;
            }
            break;

        case OP_SYNC:
            // TODO(yiyuanzhong): implement this.
            assert(p->zkc());
            assert(!p->zkw());
            CLOG.Debug("ZooKeeper: resuming  SYNC  (pending=%p, path=%s)", p, p->path());
            break;

        case OP_GET:
            assert(p->zkc() || p->zkw());
            if (!p->zkc()) { // Force trigger the watcher.
                p->zkw()->OnEvent(ZOO_CHANGED_EVENT, status, p->path());
            } else if (AsyncGet(p->path(), p->zkc(), p->zkw()) == ZOK) {
                CLOG.Debug("ZooKeeper: resuming  GET   (pending=%p, path=%s)", p, p->path());
            } else {
                CLOG.Warn("ZooKeeper: failed to resume  GET   (pending=%p, path=%s)", p, p->path());
                p->zkc()->OnGet(ZSESSIONEXPIRED, p->path(), std::string(), NULL);
                ++failed;
            }
            break;

        case OP_SET:
            // TODO(yiyuanzhong): implement this.
            assert(p->zkc());
            assert(!p->zkw());
            CLOG.Debug("ZooKeeper: resuming  SET   (pending=%p, path=%s)", p, p->path());
            break;

        case OP_GETA:
            // TODO(yiyuanzhong): implement this.
            assert(p->zkc());
            assert(!p->zkw());
            CLOG.Debug("ZooKeeper: resuming  GETA  (pending=%p, path=%s)", p, p->path());
            break;

        case OP_SETA:
            // TODO(yiyuanzhong): implement this.
            assert(p->zkc());
            assert(!p->zkw());
            CLOG.Debug("ZooKeeper: resuming  SETA  (pending=%p, path=%s)", p, p->path());
            break;

        case OP_GETC:
            assert(p->zkc() || p->zkw());
            if (!p->zkc()) { // Force trigger the watcher.
                p->zkw()->OnEvent(ZOO_CHILD_EVENT, status, p->path());
            } else if (AsyncGetChildren(p->path(), p->zkc(), p->zkw()) == ZOK) {
                CLOG.Debug("ZooKeeper: resuming  GETC  (pending=%p, path=%s)", p, p->path());
            } else {
                CLOG.Warn("ZooKeeper: failed to resume  GETC  (pending=%p, path=%s)", p, p->path());
                p->zkc()->OnGetChildren(ZSESSIONEXPIRED, p->path(),
                                          std::list<std::string>(), NULL);

                ++failed;
            }
            break;

        default:
            assert(false);
            CLOG.Error("ZooKeeper: internal error with invalid pending data: %d", p->op());
            break;
        }
    }

    for (std::set<Pending *>::const_iterator p = pending.begin(); p != pending.end(); ++p) {
        delete *p;
    }

    if (failed) {
        CLOG.Warn("ZooKeeper: erased %lu pending tasks that failed to resume.", failed);
    }
}

void ZooKeeper::OnConnected()
{
    MutexLocker locker(&_mutex);
    if (!_handle) {
        CLOG.Warn("ZooKeeper: connected but the handle is NULL, maybe shutting down...");
        assert(_connecting);
        return;
    }

    zhandle_t *handle = _handle;
    const clientid_t *cid = zoo_client_id(handle);
    assert(cid);
    if (!cid) {
        CLOG.Error("ZooKeeper: connected but the session id is invalid.");
        assert(false);
        Shutdown();
        return;
    }

    _client_id = *cid;
    CLOG.Info("ZooKeeper: connected session 0x%016llx(%p)",
              static_cast<long long>(cid->client_id), handle);

    if (_resuming) {
        _resuming = false;

        if (!_pending.empty()) {
            std::set<Pending *> pending(_pending);
            _pending.clear();
            locker.Unlock();

            ResumeSession(pending);
        }
    }
}

void ZooKeeper::OnSession(int state)
{
    /*
     * Reason unknown, but even on_session() is called within ZooKeeper C client library
     * completions, aka worker threads, calling zookeeper_close() inside this context will
     * not cause ZooKeeper to pthread_join() the worker.
     *
     * In one word, it's OK to operate on zhandle_t within here.
     */
    if (state == ZOO_CONNECTING_STATE || state == ZOO_ASSOCIATING_STATE) {
        CLOG.Warn("ZooKeeper: connection interrupted, resuming...");
        return;
    }

    if (state == ZOO_CONNECTED_STATE) {
        OnConnected();

    } else if (state == ZOO_EXPIRED_SESSION_STATE) {
        CLOG.Warn("ZooKeeper: session expired, starting a new session...");
        Reconnect();

    } else if (state == ZOO_AUTH_FAILED_STATE) {
        CLOG.Error("ZooKeeper: authorization failed.");
        Shutdown();

    } else {
        CLOG.Warn("ZooKeeper: unknown state: %d", state);
    }
}

int ZooKeeper::Set(const char *path, const std::string &data, struct Stat *stat, int version)
{
    if (!path || path[0] != '/') {
        return ZBADARGUMENTS;
    }

    if (data.length() > INT_MAX) {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int length = static_cast<int>(data.length());
    int error = ZOK;
    if (stat) {
        // You call version 2 with a non-null stat.
        error = zoo_set2(_handle, path, data.data(), length, version, stat);
    } else {
        // Or you call version 1.
        error = zoo_set(_handle, path, data.data(), length, version);
    }

    return error;
}

int ZooKeeper::Exists(const char *path,
                      struct Stat *stat,
                      ZooKeeperWatcher *watcher)
{
    if (!path || path[0] != '/') {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    if (watcher) {
        Pending *p = new Pending(this, OP_EXISTS, path, watcher, NULL);
        error = zoo_wexists(_handle, path, GlobalWatcher, p, stat);
        if (error == ZOK || error == ZNONODE) { // Birds out, keep track.
            _pending.insert(p);
        } else {
            delete p;
        }

    } else {
        error = zoo_exists(_handle, path, 0, stat);
    }

    return error;
}

int ZooKeeper::Get(const char *path,
                   std::string *data,
                   struct Stat *stat,
                   ZooKeeperWatcher *watcher)
{
    if (!path || path[0] != '/') {
        return ZBADARGUMENTS;
    }

    int length = 0;
    char *buffer = NULL;
    if (data) {
        size_t len = data->length();
        if (!len) {
            len = 1024;
            data->resize(len);
        } else if (len > INT_MAX) {
            len = INT_MAX;
        }

        length = static_cast<int>(len);
        buffer = &data->at(0);
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    if (watcher) {
        Pending *p = new Pending(this, OP_GET, path, watcher, NULL);
        error = zoo_wget(_handle, path, GlobalWatcher, p, buffer, &length, stat);
        if (error == ZOK) { // Birds out, keep track.
            _pending.insert(p);
        } else {
            delete p;
        }

    } else {
        error = zoo_get(_handle, path, 0, buffer, &length, stat);
    }

    if (data) {
        // Always good, but sometimes with data truncated.
        if (length < 0) {
            data->clear();
        } else {
            data->resize(static_cast<size_t>(length));
        }
    }

    return error;
}

int ZooKeeper::GetChildren(const char *path,
                           std::list<std::string> *children,
                           struct Stat *stat,
                           ZooKeeperWatcher *watcher)
{
    if (!path || path[0] != '/') {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    struct Stat rstat, *pstat = stat ? stat : &rstat;
    struct String_vector strings;
    memset(&strings, 0, sizeof(strings));
    if (watcher) {
        Pending *p = new Pending(this, OP_GETC, path, watcher, NULL);
        error = zoo_wget_children2(_handle, path, GlobalWatcher, p, &strings, pstat);

        if (error == ZOK) { // Birds out, keep track.
            _pending.insert(p);
            CLOG.Debug("ZooKeeper: get_children: inserted %p", p);
        } else {
            delete p;
        }
    } else {
        error = zoo_get_children2(_handle, path, 0, &strings, pstat);
    }

    if (error != ZOK) {
        return error;
    }

    if (!children) { // Easy one.
        deallocate_String_vector(&strings);
        return ZOK;
    }

    children->clear();
    for (int i = 0; i < strings.count; ++i) {
        children->push_back(strings.data[i]);
    }
    deallocate_String_vector(&strings);
    return ZOK;
}

int ZooKeeper::Create(const char *path,
                      const char *data,
                      std::string *actual_path,
                      bool ephemeral,
                      bool sequence,
                      const Acl &acl)
{
    if (!path || path[0] != '/' || !data) {
        return ZBADARGUMENTS;
    }

    size_t plen = strlen(path);
    if (plen > INT_MAX - 16) {
        return ZBADARGUMENTS;
    }
    plen += 16;

    size_t dlen = strlen(data);
    if (dlen > INT_MAX) {
        return ZBADARGUMENTS;
    }

    struct ACL_vector av;
    Translate(acl, &av);
    int flags = 0;
    if (ephemeral) {
        flags |= ZOO_EPHEMERAL;
    }

    if (sequence) {
        flags |= ZOO_SEQUENCE;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        deallocate_ACL_vector(&av);
        return ZINVALIDSTATE;
    }

    // Theoretically the actual path is at most appended a %10d
    std::vector<char> actual(plen);
    int error = zoo_create(_handle,
                           path,
                           data,
                           static_cast<int>(dlen),
                           &av,
                           flags,
                           &actual[0],
                           static_cast<int>(plen));

    deallocate_ACL_vector(&av);
    if (error != ZOK) {
        return error;
    }

    if (actual_path) {
        *actual_path = &actual[0];
    }
    return ZOK;
}

/// allocate_ACL_vector() is hidden by libzookeeper.
void ZooKeeper::Translate(const Acl &acl, struct ACL_vector *result)
{
    assert(result);

    if (acl.empty()) {
        result->count = 0;
        result->data = NULL;
        return;
    }

    int32_t i = 0;

    result->data = reinterpret_cast<struct ACL *>(calloc(sizeof(*result->data), acl.size()));
    if (!result->data) {
        throw std::bad_alloc();
    }

    result->count = static_cast<int32_t>(acl.size());
    for (Acl::const_iterator p = acl.begin(); p != acl.end(); ++p, ++i) {
        struct ACL &a = result->data[i];
        a.perms = p->second;
        a.id.scheme = strdup(p->first.first.c_str());
        a.id.id = strdup(p->first.second.c_str());
    }
}

void ZooKeeper::Translate(const struct ACL_vector *acl, Acl *result)
{
    assert(acl);
    assert(result);
    result->clear();
    for (int32_t i = 0; i < acl->count; ++i) {
        struct ACL &a = acl->data[i];
        Id id(a.id.scheme, a.id.id);

        Acl::iterator p = result->find(id);
        if (p != result->end()) {
            p->second |= a.perms;
        } else {
            result->insert(Acl::value_type(id, a.perms));
        }
    }
}

int ZooKeeper::GetAcl(const char *path, Acl *acl, struct Stat *stat)
{
    if (!path || path[0] != '/' || !acl) {
        return ZBADARGUMENTS;
    }

    struct ACL_vector av;
    memset(&av, 0, sizeof(av));
    struct Stat rstat, *pstat = stat ? stat : &rstat;

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = zoo_get_acl(_handle, path, &av, pstat);
    if (error != ZOK) {
        return error;
    }

    Translate(&av, acl);
    deallocate_ACL_vector(&av);
    return ZOK;
}

int ZooKeeper::SetAcl(const char *path, const Acl &acl, int version)
{
    if (!path || path[0] != '/') {
        return ZBADARGUMENTS;
    }

    struct ACL_vector av;
    Translate(acl, &av);

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = zoo_set_acl(_handle, path, version, &av);
    deallocate_ACL_vector(&av);
    return error;
}

int ZooKeeper::Erase(const char *path, int version)
{
    if (!path || path[0] != '/') {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    return zoo_delete(_handle, path, version);
}

int ZooKeeper::Reconnect()
{
    MutexLocker locker(&_mutex);
    if (_connecting) {
        return ZOK;
    }

    zhandle_t *handle = _handle;
    _handle = NULL;

    if (handle) {
        _connecting = true;
        locker.Unlock();
        Disconnect(handle);
        locker.Relock();
        _connecting = false;
    }

    memset(&_client_id, 0, sizeof(_client_id));
    _resuming = true;

    int ret = ReconnectNolock();
    return ret;
}

int ZooKeeper::ReconnectNolock()
{
    assert(!_handle);

    if (!_hosts.length() || _timeout < 0) {
        return ZBADARGUMENTS;
    }

    if (_client_id.client_id) {
        CLOG.Info("ZooKeeper: resuming session 0x%016llx...",
                  static_cast<long long>(_client_id.client_id));
    } else {
        CLOG.Info("ZooKeeper: starting new session...");
    }

    int milliseconds = -1;
    if (_timeout >= 0) {
        milliseconds = static_cast<int>(_timeout / 1000000LL);
    }

    zhandle_t *zh = zookeeper_init(_hosts.c_str(),
                                   GlobalWatcher,
                                   milliseconds,
                                   &_client_id,
                                   &_session_pending,
                                   0);

    int error = errno;
    if (!zh) {
        CLOG.Error("ZooKeeper: failed to start a session: %d: %s", error, strerror(error));
        return error;
    }

    _handle = zh;
    CLOG.Info("ZooKeeper: session establishing...");
    return ZOK;
}

int ZooKeeper::Initialize(const std::string &hosts, int64_t timeout)
{
    MutexLocker locker(&_mutex);
    if (_handle) {
        return ZINVALIDSTATE;
    } else if (_connecting) {
        return ZOK;
    }

    _hosts = hosts;
    _timeout = timeout;
    _connecting = false;

    // Always initialize a new session.
    memset(&_client_id, 0, sizeof(_client_id));

    return ReconnectNolock();
}

int ZooKeeper::Disconnect(zhandle_t *handle)
{
    if (!handle) {
        return ZOK;
    }

    int64_t id = 0;
    const clientid_t *cid = zoo_client_id(handle);
    if (cid) {
        id = cid->client_id;
    }

    int error = zookeeper_close(handle);
    if (error != ZOK) {
        CLOG.Warn("ZooKeeper: failed to disconnect session 0x%016llx(%p) "
                  "cleanly, still disconnected: %d",
                  static_cast<long long>(id), handle, error);

        return error;
    }

    CLOG.Info("ZooKeeper: session 0x%016llx(%p) disconnected.",
              static_cast<long long>(id), handle);

    return ZOK;
}

int ZooKeeper::Shutdown()
{
    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZOK;
    }
    assert(!_connecting);

    memset(&_client_id, 0, sizeof(_client_id));
    zhandle_t *handle = _handle;
    _handle = NULL;

    if (handle) {
        _connecting = true;
        locker.Unlock();
        int error = Disconnect(handle);
        if (error != ZOK) {
            return error;
        }

        locker.Relock();
        assert(!_handle);
        assert(_connecting);
        _connecting = false;
    }

    for (std::set<Pending *>::iterator p = _pending.begin(); p != _pending.end(); ++p) {
        CLOG.Debug("ZooKeeper: delete pending %p after shutdown.", *p);
        delete *p;
    }

    _pending.clear();
    return ZOK;
}

int ZooKeeper::AsyncGet(const char *path,
                        ZooKeeperCallback *zkc,
                        ZooKeeperWatcher *watcher)
{
    if (!path || path[0] != '/' || (!zkc && !watcher)) {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    Pending *p = new Pending(this, OP_GET, path, watcher, zkc);
    if (watcher) {
        error = zoo_awget(_handle, path, GlobalWatcher, p, GlobalDataCompletion, p);
    } else {
        error = zoo_aget(_handle, path, 0, GlobalDataCompletion, p);
    }

    if (error != ZOK) {
        delete p;
        return error;
    }

    _pending.insert(p);
    return ZOK;
}

int ZooKeeper::AsyncExists(const char *path,
                           ZooKeeperCallback *zkc,
                           ZooKeeperWatcher *watcher)
{
    if (!path || path[0] != '/' || (!zkc && !watcher)) {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    Pending *p = new Pending(this, OP_EXISTS, path, watcher, zkc);
    if (watcher) {
        error = zoo_awexists(_handle, path, GlobalWatcher, p, GlobalStatCompletion, p);
    } else {
        error = zoo_aexists(_handle, path, 0, GlobalStatCompletion, p);
    }

    if (error != ZOK) {
        delete p;
        return error;
    }

    _pending.insert(p);
    return ZOK;
}

int ZooKeeper::AsyncGetChildren(const char *path,
                                ZooKeeperCallback *zkc,
                                ZooKeeperWatcher *watcher)
{
    if (!path || path[0] != '/' || (!zkc && !watcher)) {
        return ZBADARGUMENTS;
    }

    MutexLocker locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    Pending *p = new Pending(this, OP_GETC, path, watcher, zkc);
    if (watcher) {
        error = zoo_awget_children2(_handle, path, GlobalWatcher, p,
                                                   GlobalStringsStatCompletion, p);
    } else {
        error = zoo_aget_children2(_handle, path, 0, GlobalStringsStatCompletion, p);
    }

    if (error != ZOK) {
        delete p;
        return error;
    }

    _pending.insert(p);
    return ZOK;
}

int ZooKeeper::state() const
{
    MutexLocker handle_locker(&_mutex);
    if (!_handle) {
        return ZINVALIDSTATE;
    }

    return zoo_state(_handle);
}

int ZooKeeper::is_connected() const
{
    int s = state();
    if (s == ZOO_CONNECTED_STATE) {
        return 0;
    } else if (s == ZOO_AUTH_FAILED_STATE) {
        return -1;
    } else {
        return 1;
    }
}

bool ZooKeeper::WaitUntilConnected(int64_t timeout) const
{
    int64_t deadline = -1;
    if (timeout >= 0) {
        deadline = get_monotonic_timestamp() + timeout;
    }

    while (true) {
        int s = state();
        if (s == ZOO_CONNECTED_STATE) {
            return true;
        } else if (s == ZOO_AUTH_FAILED_STATE) {
            return false;
        }

        msleep(100);
        if (deadline >= 0) {
            if (get_monotonic_timestamp() >= deadline) {
                return false;
            }
        }
    }
}

const char *ZooKeeper::strerror(int error)
{
    switch (error) {
    case ZOK:
        return "Everything is OK";

    case ZRUNTIMEINCONSISTENCY:
        return "A runtime inconsistency was found";

    case ZDATAINCONSISTENCY:
        return "A data inconsistency was found";

    case ZCONNECTIONLOSS:
        return "Connection to the server has been lost";

    case ZMARSHALLINGERROR:
        return "Error while marshalling or unmarshalling data";

    case ZUNIMPLEMENTED:
        return "Operation is unimplemented";

    case ZOPERATIONTIMEOUT:
        return "Operation timeout";

    case ZBADARGUMENTS:
        return "Invalid arguments";

    case ZINVALIDSTATE:
        return "Invalid zhandle state";

    case ZNONODE:
        return "Node does not exist";

    case ZNOAUTH:
        return "Not authenticated";

    case ZBADVERSION:
        return "Version conflict";

    case ZNOCHILDRENFOREPHEMERALS:
        return "Ephemeral nodes may not have children";

    case ZNODEEXISTS:
        return "The node already exists";

    case ZNOTEMPTY:
        return "The node has children";

    case ZSESSIONEXPIRED:
        return "The session has been expired by the server";

    case ZINVALIDCALLBACK:
        return "Invalid callback specified";

    case ZINVALIDACL:
        return "Invalid ACL specified";

    case ZAUTHFAILED:
        return "Client authentication failed";

    case ZCLOSING:
        return "ZooKeeper is closing";

    case ZNOTHING:
        return "(not error) no server responses to process";

    case ZSESSIONMOVED:
        return "Session moved to another server, so operation is ignored";

    default:
        if (error < ZSYSTEMERROR && error > ZAPIERROR) {
            return "Unknown system and server-wide error.";
        } else if (error < ZAPIERROR) {
            return "Unknown API error";
        } else {
            return "Unknown error";
        }
    }
}

} // namespace flinter
