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

#ifndef FLINTER_ZOOKEEPER_ZOOKEEPER_TRACKER_H
#define FLINTER_ZOOKEEPER_ZOOKEEPER_TRACKER_H

#include <list>
#include <map>
#include <string>

#include <flinter/thread/mutex.h>

#include <flinter/zookeeper/zookeeper_callback.h>
#include <flinter/zookeeper/zookeeper_watcher.h>

namespace flinter {

class ZooKeeper;

/// ZooKeeperTracker automatically track data/children changes and calls back.
class ZooKeeperTracker : public ZooKeeperCallback {
public:
    /// Callback interface.
    class Callback {
    public:
        /// Destructor
        virtual ~Callback() {}

        /// In case of ZooKeeper failures.
        /// @param error ZooKeeper return value.
        /// @warning This callback will be triggered ONLY ONCE, until clear_error_state()
        ///          is called.
        virtual void OnError(int error, const char *path) = 0;

        /// @param path changed node path
        /// @param data NULL if the node is missing.
        virtual void OnData(const char *path, const std::string *data);

        /// @param path changed node path
        /// @param children NULL if the node is missing.
        virtual void OnChildren(const char *path, const std::list<std::string> *children);

    }; // class Callback

    ZooKeeperTracker();     ///< Constructor
    ~ZooKeeperTracker();    ///< Destructor

    bool Initialize(ZooKeeper *zk);     ///< Call before any methods.
    void Shutdown();                    ///< Don't call any methods afterwards.

    /// Track a node for data/children.
    /// Automatically cancel watching if the node is missing, calling OnData(NULL) /
    ///     on_children(NULL) when it is. No watch is left on ZooKeeper server.
    bool TrackExisting(const char *path,       // path to tracked node
                       bool track_data,        // whether to call back when data changes
                       bool track_children,    // whether to call back when children changes
                       Callback *cb);          // will not take life span

    /// Track a node for data/children.
    /// Always track until cancelled, leaving a watch on ZooKeeper server even after that.
    bool Track(const char *path,        // path to tracked node
               bool track_data,         // whether to call back when data changes
               bool track_children,     // whether to call back when children changes
               bool notify_missing,     // whether to call back if the node is missing
               Callback *cb);           // will not take life span

    /// Don't track a node.
    /// However the tracking is still in effect as seen by ZooKeeper, until it triggers.
    void Cancel(const char *path, Callback *cb);

    /// Allow global ZooKeeper failures to broadcast again.
    void ClearErrorState();

    // Don't call below.

    /// ZooKeeperCallback
    virtual void OnExists(int rc, const char *path, const struct Stat *stat);

    /// ZooKeeperCallback
    virtual void OnGet(int rc,
                       const char *path,
                       const std::string &data,
                       const struct Stat *stat);

    /// ZooKeeperCallback
    virtual void OnGetChildren(int rc,
                               const char *path,
                               const std::list<std::string> &children,
                               const struct Stat *stat);

private:
    friend class Watcher;

    /// Internal watcher callback.
    class Watcher : public ZooKeeperWatcher {
    public:
        Watcher(ZooKeeperTracker *zkt); ///< Constructor
        virtual ~Watcher();             ///< Destructor

    protected:
        /// I really need to monitor this.
        virtual void OnSession(int state);

        virtual void OnCreated(int state, const char *path);   ///< Nothing.
        virtual void OnChanged(int state, const char *path);   ///< Nothing.
        virtual void OnErased (int state, const char *path);   ///< Nothing.
        virtual void OnChild  (int state, const char *path);   ///< Nothing.

    private:
        ZooKeeperTracker *_zkt;         ///< ZooKeeperTracker instance.

    }; // class Watcher

    /// Internal state container.
    struct Tracker {
        Tracker(Callback *c, bool d, bool r, bool a, bool n)
                : cb(c), data(d), children(r), cancel_on_missing(a), null(n) {}

        Callback *cb;
        bool data;
        bool children;
        bool cancel_on_missing;
        bool null;

    }; // struct Tracker

    /// @sa track() and track_existing()
    bool DoTrack(const char *path, bool track_data, bool track_children,
                 bool notify_missing, bool cancel_on_missing, Callback *cb);

    /// Session handler.
    bool Process(int state);

    /// Everything handler except for session events.
    bool Process(const char *path, bool dreq, bool creq, bool erased);

    /// Everything handler except for session events.
    /// @warning call this method with mutex held.
    int ProcessNolock(const char *path, bool dreq, bool creq, bool erased);

    void NotifyNull(const std::multimap<std::string, Tracker> &tracks, const char *path);

    void NotifyError(const std::multimap<std::string, Tracker> &tracks,
                     const char *path, int error);

    /// Don't track a node.
    /// @warning call this method with mutex held.
    void CancelNolock(const char *path, Callback *cb);

    /// Internal worker.
    /// @warning call this method with mutex held.
    int Get(const char *path, bool dreq, bool creq, bool exist_first);

    ZooKeeper *_zk;
    Watcher _watcher;

    /// All tracked objects.
    std::multimap<std::string, Tracker> _tracks;

    /// All tracked path:exist.
    /// Multiple zoo_exists() will double async results each time.
    std::set<std::string> _exists;

    /// All tracked path:data.
    std::set<std::string> _data;

    /// All tracked path:children.
    std::set<std::string> _children;

    /// Only broadcast the first global ZooKeeper failure.
    /// @sa Callback::on_error().
    bool _error_has_broadcasted;

    mutable Mutex _mutex;

}; // class ZooKeeperTracker

} // namespace flinter

#endif // FLINTER_ZOOKEEPER_ZOOKEEPER_TRACKER_H
