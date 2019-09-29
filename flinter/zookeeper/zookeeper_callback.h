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

#ifndef FLINTER_ZOOKEEPER_ZOOKEEPER_CALLBACK_H
#define FLINTER_ZOOKEEPER_ZOOKEEPER_CALLBACK_H

#include <list>
#include <string>

#include <zookeeper/zookeeper.h>

#include <flinter/zookeeper/zookeeper.h>

namespace flinter {

/// ZooKeeperCallback is a wrapper around ZooKeeper API callback.
class ZooKeeperCallback {
public:
    /// Pure virtual destructor to prevent constructing.
    virtual ~ZooKeeperCallback() = 0;

    /// @sa ZooKeeper
    virtual void OnErase(int rc, const char *path);

    /// @sa ZooKeeper
    virtual void OnCreate(int rc, const char *path, const char *callback_path);

    /// @sa ZooKeeper
    virtual void OnSync(int rc, const char *path, const char *callback_path);

    /// @sa ZooKeeper
    virtual void OnExists(int rc, const char *path, const struct Stat *stat);

    /// @sa ZooKeeper
    virtual void OnSet(int rc, const char *path, const struct Stat *stat);

    /// @sa ZooKeeper
    virtual void OnGetChildren(int rc,
                               const char *path,
                               const std::list<std::string> &children,
                               const struct Stat *stat);

    /// @sa ZooKeeper
    virtual void OnGet(int rc,
                       const char *path,
                       const std::string &data,
                       const struct Stat *stat);

    /// @sa ZooKeeper
    virtual void OnSetAcl(int rc, const char *path);

    /// @sa ZooKeeper
    virtual void OnGetAcl(int rc,
                          const char *path,
                          const ZooKeeper::Acl &acl,
                          const struct Stat *stat);

}; // class ZooKeeperCallback

} // namespace flinter

#endif // FLINTER_ZOOKEEPER_ZOOKEEPER_CALLBACK_H
