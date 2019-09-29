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

#ifndef FLINTER_ZOOKEEPER_NAMING_H
#define FLINTER_ZOOKEEPER_NAMING_H

#include <stdint.h>

#include <map>
#include <string>

#include <flinter/thread/mutex.h>

#include <flinter/zookeeper/zookeeper_tracker.h>
#include <flinter/zookeeper/zookeeper_watcher.h>

namespace flinter {

class Naming : public ZooKeeperTracker::Callback
             , public ZooKeeperWatcher {
public:
    class Callback {
    public:
        virtual ~Callback() {}
        virtual void OnError(const std::string & /*path*/) {}
        virtual void OnChanged(const std::map<int32_t, std::string> & /*naming*/) {}

    }; // class Callback

    Naming();
    virtual ~Naming();

    bool Watch(ZooKeeper *zk,
               const std::string &naming_path,
               Callback *cb = NULL);

    bool Attend(ZooKeeper *zk,
                const std::string &naming_path,
                const std::string &identity,
                Callback *cb = NULL);

    bool Shutdown();

    /// Only for requesters.
    bool Get(std::map<int32_t, std::string> *naming);

    // Don't call below explicitly, used internally.

    /// ZooKeeperTracker callback.
    virtual void OnError(int error, const char *path);

    /// ZooKeeperTracker callback.
    virtual void OnChildren(const char *path,
                            const std::list<std::string> *children);

private:
    bool DoWatch(ZooKeeper *zk, const std::string &naming_path, Callback *cb);
    bool RemoveNode();

    bool CreateNode(ZooKeeper *zk,
                    const std::string &naming_path,
                    const std::string &identity);

    Callback *_cb;
    ZooKeeper *_zk;
    ZooKeeperTracker *_zkt;

    std::string _path;
    std::string _actual_path;
    std::map<int32_t, std::string> _naming;

    mutable Mutex _mutex;

}; // class Naming

} // namespace flinter

#endif // FLINTER_ZOOKEEPER_NAMING_H
