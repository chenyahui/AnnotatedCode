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

#ifndef FLINTER_ZOOKEEPER_ZOOKEEPER_WATCHER_H
#define FLINTER_ZOOKEEPER_ZOOKEEPER_WATCHER_H

namespace flinter {

/// ZooKeeperWatcher is a wrapper around ZooKeeper API watchers.
class ZooKeeperWatcher {
public:
    /// Pure virtual destructor to prevent constructing.
    virtual ~ZooKeeperWatcher() = 0;

    /// Entry point.
    void OnEvent(int type, int state, const char *path);

protected:
    virtual void OnChild      (int state, const char *path); ///< @sa ZooKeeper
    virtual void OnCreated    (int state, const char *path); ///< @sa ZooKeeper
    virtual void OnErased     (int state, const char *path); ///< @sa ZooKeeper
    virtual void OnChanged    (int state, const char *path); ///< @sa ZooKeeper
    virtual void OnNotWatching(int state, const char *path); ///< @sa ZooKeeper

    /// There's typically no need to override this method.
    /// ZooKeeper will take care of session restoring.
    virtual void OnSession     (int state);

}; // class ZooKeeperWatcher

} // namespace flinter

#endif // FLINTER_ZOOKEEPER_ZOOKEEPER_WATCHER_H
