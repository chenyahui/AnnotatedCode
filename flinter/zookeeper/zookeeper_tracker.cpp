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

#include "flinter/zookeeper/zookeeper_tracker.h"

#include <assert.h>

#include "flinter/thread/mutex_locker.h"
#include "flinter/logger.h"

#include "flinter/zookeeper/zookeeper.h"

namespace flinter {

void ZooKeeperTracker::Callback::OnData(const char *path, const std::string * /*data*/)
{
    CLOG.Warn("ZooKeeperTracker: uhandled callback OnData(%s) called.", path);
}

void ZooKeeperTracker::Callback::OnChildren(const char *path,
                                            const std::list<std::string> * /*children*/)
{
    CLOG.Warn("ZooKeeperTracker: uhandled callback OnChildren(%s) called.", path);
}

ZooKeeperTracker::ZooKeeperTracker() : _zk(NULL)
                                     , _watcher(this)
                                     , _error_has_broadcasted(false)
{
    // Intended left blank.
}

ZooKeeperTracker::~ZooKeeperTracker()
{
    Shutdown();
}

bool ZooKeeperTracker::Initialize(ZooKeeper *zk)
{
    MutexLocker locker(&_mutex);
    if (_zk || !zk) {
        return false;
    }

    _zk = zk;
    _error_has_broadcasted = false;
    return true;
}

void ZooKeeperTracker::Shutdown()
{
    MutexLocker locker(&_mutex);
    if (!_zk) {
        return;
    }

    _zk->Detach(&_watcher, NULL);
    _children.clear();
    _tracks.clear();
    _exists.clear();
    _data.clear();
    _zk = NULL;
}

bool ZooKeeperTracker::Track(const char *path,
                             bool track_data,
                             bool track_children,
                             bool notify_missing,
                             Callback *cb)
{
    return DoTrack(path, track_data, track_children, notify_missing, false, cb);
}

bool ZooKeeperTracker::TrackExisting(const char *path,
                                     bool track_data,
                                     bool track_children,
                                     Callback *cb)
{
    return DoTrack(path, track_data, track_children, true, true, cb);
}

bool ZooKeeperTracker::DoTrack(const char *path,
                               bool track_data,
                               bool track_children,
                               bool notify_missing,
                               bool cancel_on_missing,
                               Callback *cb)
{
    if (!path || path[0] != '/' || !cb) {
        return false;
    }

    if (!track_data && !track_children) {
        return false;
    }

    Tracker track(cb, track_data, track_children, cancel_on_missing, notify_missing);
    MutexLocker locker(&_mutex);
    if (!_zk) {
        return false;
    }

    std::pair<std::map<std::string, Tracker>::iterator,
              std::map<std::string, Tracker>::iterator> range =
                    _tracks.equal_range(path);

    bool data_covered = false;
    bool children_covered = false;
    for (std::multimap<std::string, Tracker>::iterator p = range.first; p != range.second; ++p) {
        if (cb == p->second.cb) {
            if (!track_data || p->second.data) {
                data_covered = true;
                if (children_covered) {
                    break;
                }
            }

            if (!track_children || p->second.children) {
                children_covered = true;
                if (data_covered) {
                    break;
                }
            }
        }
    }

    if (data_covered && children_covered) {
        CLOG.Debug("ZooKeeperTracker: %s(%p) already tracked.", path, cb);
        return true;
    }

    _tracks.insert(std::multimap<std::string, Tracker>::value_type(path, track));
    int ret = ProcessNolock(path, true, true, false);
    if (ret == ZOK) {
        return true;
    }

    // Even if Process() is half successful, revert it all.
    CancelNolock(path, cb);

    // Remaining callbacks will get a failure too about this path.
    std::multimap<std::string, Tracker> tracks(_tracks);
    locker.Unlock();

    NotifyError(tracks, path, ret);
    return false;
}

void ZooKeeperTracker::Cancel(const char *path, Callback *cb)
{
    if (!path || path[0] != '/') {
        return;
    }

    MutexLocker locker(&_mutex);
    CancelNolock(path, cb);
}

void ZooKeeperTracker::CancelNolock(const char *path, Callback *cb)
{
    assert(path && path[0] == '/');

    std::pair<std::map<std::string, Tracker>::iterator,
              std::map<std::string, Tracker>::iterator> range =
                    _tracks.equal_range(path);

    for (std::multimap<std::string, Tracker>::iterator p = range.first; p != range.second;) {
        std::multimap<std::string, Tracker>::iterator q = p++;
        if (!cb || cb == q->second.cb) {
            _tracks.erase(q);
        }
    }
}

ZooKeeperTracker::Watcher::Watcher(ZooKeeperTracker *zkt) : _zkt(zkt)
{
    assert(zkt);
}

ZooKeeperTracker::Watcher::~Watcher()
{
    ZooKeeper *zk = _zkt->_zk;
    if (zk) {
        zk->Detach(this, NULL);
    }
}

void ZooKeeperTracker::Watcher::OnSession(int state)
{
    _zkt->Process(state);
}

void ZooKeeperTracker::Watcher::OnErased(int /*state*/, const char *path)
{
    _zkt->Process(path, true, true, true);
}

void ZooKeeperTracker::Watcher::OnCreated(int /*state*/, const char *path)
{
    _zkt->Process(path, true, true, false);
}

void ZooKeeperTracker::Watcher::OnChanged(int /*state*/, const char *path)
{
    _zkt->Process(path, true, false, false);
}

void ZooKeeperTracker::Watcher::OnChild(int /*state*/, const char *path)
{
    _zkt->Process(path, false, true, false);
}

bool ZooKeeperTracker::Process(int state)
{
    if (state == ZOO_CONNECTED_STATE) {
        return true;
    } else if (state == ZOO_CONNECTING_STATE || state == ZOO_ASSOCIATING_STATE) {
        return true;
    }

    // All other ones are critical and must trigger error reporting.
    int error = ZAUTHFAILED;
    if (state == ZOO_EXPIRED_SESSION_STATE) {
        error = ZSESSIONEXPIRED;
    }

    MutexLocker locker(&_mutex);
    bool broadcast = !_error_has_broadcasted;
    if (!broadcast) {
        return false;
    }

    _error_has_broadcasted = true;
    std::multimap<std::string, Tracker> tracks(_tracks);
    locker.Unlock();

    std::set<Callback *> callbacks;
    for (std::multimap<std::string, Tracker>::iterator p = tracks.begin(); p != tracks.end(); ++p) {
        callbacks.insert(p->second.cb);
    }

    CLOG.Warn("ZooKeeperWatcher: to notify %lu(%lu) clients about ZooKeeper error: %d: %s",
              callbacks.size(), tracks.size(), error, ZooKeeper::strerror(error));

    for (std::set<Callback *>::iterator p = callbacks.begin(); p != callbacks.end(); ++p) {
        Callback *cb = *p;
        assert(cb);
        cb->OnError(error, NULL);
    }

    return false;
}

void ZooKeeperTracker::ClearErrorState()
{
    _error_has_broadcasted = false;
}

bool ZooKeeperTracker::Process(const char *path, bool dreq, bool creq, bool erased)
{
    assert(dreq || creq);
    MutexLocker locker(&_mutex);
    int ret = ProcessNolock(path, dreq, creq, erased);

    std::multimap<std::string, Tracker> tracks(_tracks);
    locker.Unlock();

    if (ret == ZOK) {
        if (erased) {
            NotifyNull(tracks, path);
        }

    } else {
        NotifyError(tracks, path, ret);
    }

    return ret == ZOK;
}

void ZooKeeperTracker::NotifyNull(const std::multimap<std::string, Tracker> &tracks,
                                  const char *path)
{
    std::pair<std::map<std::string, Tracker>::const_iterator,
              std::map<std::string, Tracker>::const_iterator> range =
                    tracks.equal_range(path);

    std::set<Callback *> data_callbacks;
    std::set<Callback *> children_callbacks;
    for (std::multimap<std::string, Tracker>::const_iterator p = range.first;
         p != range.second; ++p) {

        Callback *cb = p->second.cb;
        assert(cb);

        if (p->second.data) {
            data_callbacks.insert(cb);
        }

        if (p->second.children) {
            children_callbacks.insert(cb);
        }
    }

    for (std::set<Callback *>::const_iterator p = data_callbacks.begin();
         p != data_callbacks.end(); ++p) {

        Callback *cb = *p;
        CLOG.Debug("ZooKeeperTracker: calling OnData(%p, %s, NULL)...", cb, path);
        cb->OnData(path, NULL);
    }

    for (std::set<Callback *>::const_iterator p = children_callbacks.begin();
         p != children_callbacks.end(); ++p) {

        Callback *cb = *p;
        CLOG.Debug("ZooKeeperTracker: calling OnChildren(%p, %s, NULL)...", cb, path);
        cb->OnChildren(path, NULL);
    }
}

int ZooKeeperTracker::ProcessNolock(const char *path, bool dreq, bool creq, bool erased)
{
    CLOG.Debug("ZooKeeperTracker::process_nolock(%s, %d, %d, %d)", path, dreq, creq, erased);

    assert(dreq || creq);

    std::pair<std::map<std::string, Tracker>::const_iterator,
              std::map<std::string, Tracker>::const_iterator> range =
                    _tracks.equal_range(path);

    bool dneed = false;
    bool cneed = false;
    for (std::multimap<std::string, Tracker>::const_iterator p = range.first;
         p != range.second; ++p) {

        if (erased && p->second.cancel_on_missing) {
            continue;
        }

        if (p->second.data) {
            dneed = true;
            if (cneed) {
                break;
            }
        }

        if (p->second.children) {
            cneed = true;
            if (dneed) {
                break;
            }
        }
    }

    dneed &= dreq;
    cneed &= creq;
    if (!dneed && !cneed) {
        return ZOK;
    }

    int error = Get(path, dneed, cneed, erased);
    return error;
}

void ZooKeeperTracker::NotifyError(const std::multimap<std::string, Tracker> &tracks,
                                   const char *path, int error)
{
    // Oops, we'll have to notify it out.
    std::pair<std::map<std::string, Tracker>::const_iterator,
              std::map<std::string, Tracker>::const_iterator> range =
                    tracks.equal_range(path);

    std::set<Callback *> callbacks;
    for (std::multimap<std::string, Tracker>::const_iterator p = range.first;
         p != range.second; ++p) {

        Callback *cb = p->second.cb;
        assert(cb);
        callbacks.insert(cb);
    }

    for (std::set<Callback *>::const_iterator p = callbacks.begin(); p != callbacks.end(); ++p) {
        Callback *cb = *p;
        CLOG.Debug("ZooKeeperTracker: calling OnError(%p, %d, %s)...", cb, error, path);
        cb->OnError(error, path);
    }
}

int ZooKeeperTracker::Get(const char *path, bool dreq, bool creq, bool exist_first)
{
    assert(dreq || creq);
    if (!_zk) {
        CLOG.Debug("ZooKeeperTracker: get() called after shutdown.");
        return ZINVALIDSTATE;
    }

    int state = _zk->state();
    if (state != ZOO_CONNECTED_STATE) {
        CLOG.Debug("ZooKeeperTracker: invalid state: %d", state);
        return ZINVALIDSTATE;
    }

    int error = ZOK;
    do {
        if (exist_first) {
            if (_exists.find(path) != _exists.end()) {
                break;
            }

            CLOG.Debug("ZooKeeperTracker: calling async_exists(%s)...", path);
            error = _zk->AsyncExists(path, this, &_watcher);
            if (error != ZOK) {
                CLOG.Warn("ZooKeeperTracker: async_exists(%s) failed: %d: %s",
                          path, error, ZooKeeper::strerror(error));

                break;
            }

            _exists.insert(path);
            break; ///< This break is necessary since exists() is special.
        }

        if (dreq) {
            if (_data.find(path) != _data.end()) {
                break;
            }

            CLOG.Debug("ZooKeeperTracker: calling async_get(%s)...", path);
            error = _zk->AsyncGet(path, this, &_watcher);
            if (error != ZOK) {
                CLOG.Warn("ZooKeeperTracker: async_get(%s) failed: %d: %s",
                          path, error, ZooKeeper::strerror(error));

                break;
            }

            _data.insert(path);
        }

        if (creq) {
            if (_children.find(path) != _children.end()) {
                break;
            }

            CLOG.Debug("ZooKeeperTracker: calling async_get_children(%s)...", path);
            error = _zk->AsyncGetChildren(path, this, &_watcher);
            if (error != ZOK) {
                CLOG.Warn("ZooKeeperTracker: async_get_children(%s) failed: %d: %s",
                          path, error, ZooKeeper::strerror(error));

                break;
            }

            _children.insert(path);
        }

    } while (false);

    return error;
}

void ZooKeeperTracker::OnExists(int rc, const char *path, const struct Stat *stat)
{
    CLOG.Debug("ZooKeeperTracker::on_exists(%d, %s, %p)", rc, path, stat);

    MutexLocker locker(&_mutex);
    _exists.erase(path);

    if (rc == ZOK) {
        // Hell, the node is created between my queries.
        rc = ProcessNolock(path, true, true, false);
        if (rc == ZOK) {
            return;
        }
    }

    std::multimap<std::string, Tracker> tracks(_tracks);
    if (rc != ZNONODE) {
        locker.Unlock();
        NotifyError(tracks, path, rc);
        return;
    }

    std::pair<std::map<std::string, Tracker>::const_iterator,
              std::map<std::string, Tracker>::const_iterator> range =
                    tracks.equal_range(path);

    for (std::multimap<std::string, Tracker>::const_iterator p = range.first;
         p != range.second; ++p) {

        if (p->second.cancel_on_missing) {
            CLOG.Debug("ZooKeeperTracker: %p no longer interests in %s", p->second.cb, path);
            CancelNolock(path, p->second.cb);
        }
    }
}

void ZooKeeperTracker::OnGet(int rc,
                             const char *path,
                             const std::string &data,
                             const struct Stat *stat)
{
    CLOG.Debug("ZooKeeperTracker::on_get(%d, %s, %lu, %p)", rc, path, data.length(), stat);

    MutexLocker locker(&_mutex);
    _data.erase(path);

    std::multimap<std::string, Tracker> tracks(_tracks);
    if (rc == ZNONODE) {
        rc = ProcessNolock(path, true, true, true);
        locker.Unlock();
        if (rc == ZOK) {
            NotifyNull(tracks, path);
        } else {
            NotifyError(tracks, path, rc);
        }
        return;
    }

    locker.Unlock();
    if (rc != ZOK) {
        NotifyError(tracks, path, rc);
        return;
    }

    std::pair<std::multimap<std::string, Tracker>::const_iterator,
              std::multimap<std::string, Tracker>::const_iterator> range =
                      tracks.equal_range(path);

    std::set<Callback *> callbacks;
    for (std::multimap<std::string, Tracker>::const_iterator p = range.first;
         p != range.second; ++p) {

        if (p->second.data) {
            callbacks.insert(p->second.cb);
        }
    }

    for (std::set<Callback *>::const_iterator p = callbacks.begin(); p != callbacks.end(); ++p) {
        Callback *cb = *p;
        CLOG.Debug("ZooKeeperTracker: calling OnData(%p, %s, %lu)...",
                   cb, path, data.length());

        cb->OnData(path, &data);
    }
}

void ZooKeeperTracker::OnGetChildren(int rc,
                                     const char *path,
                                     const std::list<std::string> &children,
                                     const struct Stat *stat)
{
    CLOG.Debug("ZooKeeperTracker::on_get_children(%d, %s, %lu, %p)",
               rc, path, children.size(), stat);

    MutexLocker locker(&_mutex);
    _children.erase(path);

    std::multimap<std::string, Tracker> tracks(_tracks);
    if (rc == ZNONODE) {
        rc = ProcessNolock(path, true, true, true);
        locker.Unlock();
        if (rc == ZOK) {
            NotifyNull(tracks, path);
        } else {
            NotifyError(tracks, path, rc);
        }
        return;
    }

    locker.Unlock();
    if (rc != ZOK) {
        NotifyError(tracks, path, rc);
        return;
    }

    std::pair<std::multimap<std::string, Tracker>::const_iterator,
              std::multimap<std::string, Tracker>::const_iterator> range =
                      tracks.equal_range(path);

    std::set<Callback *> callbacks;
    for (std::multimap<std::string, Tracker>::const_iterator p = range.first;
         p != range.second; ++p) {

        if (p->second.children) {
            callbacks.insert(p->second.cb);
        }
    }

    for (std::set<Callback *>::const_iterator p = callbacks.begin(); p != callbacks.end(); ++p) {
        Callback *cb = *p;
        CLOG.Debug("ZooKeeperTracker: calling OnChildren(%p, %s, %lu)...",
                   cb, path, children.size());

        cb->OnChildren(path, &children);
    }
}

} // namespace flinter
