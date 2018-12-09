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

#ifndef FLINTER_ZOOKEEPER_COUNCIL_H
#define FLINTER_ZOOKEEPER_COUNCIL_H

#include <stdint.h>

#include <map>
#include <string>

#include <flinter/thread/condition.h>
#include <flinter/thread/mutex.h>

#include <flinter/zookeeper/zookeeper_tracker.h>
#include <flinter/zookeeper/zookeeper_watcher.h>

namespace flinter {

class ZooKeeper;

/// Council is an election facility using ZooKeeper as backend.
class Council : public ZooKeeperTracker::Callback
              , public ZooKeeperWatcher {
public:
    /// Council callback.
    class Callback {
    public:
        /// Destructor.
        virtual ~Callback() = 0;

        /// Fired from council, or just when we quit actively.
        /// @param path the election path.
        /// @param recoverable if false, you must disconnect ZooKeeper immediately to
        ///        prevent additional damage to the Council ecosphere.
        virtual void OnFired(const std::string & /*path*/, bool /*recoverable*/) {}

        /// Successfully elected.
        virtual void OnElected(const std::string & /*path*/, int32_t /*id*/) {}

    }; // class Callback

    Council();          ///< Constructor.
    virtual ~Council(); ///< Destructor.

    /// If not set, hostname will be used.
    void set_name(const std::string &name)
    {
        _name = name;
    }

    /// @param zk life span is NOT taken, make sure the object is alive while attended.
    /// @param election_path a node path that election will take place.
    /// @param cb can be NULL.
    bool Initialize(ZooKeeper *zk,
                    const std::string &election_path,
                    Callback *cb = NULL);

    /// Block and wait, might not be reliable if we're elected then fired immediately.
    bool WaitUntilAttended() const;

    /// Might not be reliable if we're elected then fired immediately.
    /// @retval <0 for error, 0 for elected, >0 for pending.
    int IsElected() const;

    /// @return <-1 for error, -1 if in queue, >=0 for councilman id.
    long id() const;

    /// @retval Council size.
    long size() const;

    /// After Shutdown(), the ZooKeeper object can be released.
    bool Shutdown();

    // Don't call below explicitly, used internally.

    /// ZooKeeperTracker callback.
    virtual void OnError(int error, const char *path);

    /// ZooKeeperTracker callback.
    virtual void OnData(const char *path, const std::string *data);

private:
    /// Everything set, try to attend the council.
    void Attend();

    /// Create my node if necessary.
    bool CreateNodeAsNeeded();

    /// Get candidate list.
    bool GetCandidates(std::map<int32_t, std::string> *candidates);

    /// Got fired, gotta notify someone.
    void OnFired(bool recoverable);

    /// Council size shrinked and we're fired.
    void OnShrinked(bool recoverable);

    /// Session lost, candidate node got deleted automatically.
    void OnLost();

    /// Actively quit, remove candidate node.
    void OnQuit();

    /// We're quiting either because we want to or are fired.
    void DoQuitClean(bool recoverable);

    /// We've lost connection, clean it up.
    void DoLostClean(bool recoverable);

    /// Clear internal variants.
    void ResetState(bool recoverable);

    /// Sometimes hostname changes.
    bool UpdateName();

    ZooKeeper *_zk;
    ZooKeeperTracker *_zkt;

    int32_t         _actual_id;     ///< Node ID that I created.
    std::string     _election_path; ///< A node path to perform election.
    std::string     _node_name;     ///< Last used election name.
    std::string     _name;          ///< User provided election name.
    Callback       *_cb;            ///< Callback to notify.

    int32_t         _id;            ///< Elected ID, if >=0 then == _actual_id.
    int32_t         _size;          ///< Council size.

    mutable Condition _condition;
    mutable Mutex _mutex;

}; // class Council

} // namespace flinter

#endif // FLINTER_ZOOKEEPER_COUNCIL_H
